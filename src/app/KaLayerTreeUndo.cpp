#include "KaLayerTreeUndo.h"

#include <QDateTime>
#include <QEvent>
#include <QObject>
#include <QTimer>
#include <algorithm>

#include <qgslayertree.h>
#include <qgslayertreegroup.h>
#include <qgslayertreelayer.h>
#include <qgslayertreeview.h>
#include <qgsproject.h>

namespace {
constexpr qint64 kGestureWindowMs = 800;  // app reactions to one click land in this window

// Clicks, drops and keys inside the layer list mark the next change as the user's.
class GestureFilter final : public QObject {
public:
  explicit GestureFilter(std::function<void()> onGesture) : m_onGesture(std::move(onGesture)) {}
  bool eventFilter(QObject* watched, QEvent* event) override {
    switch (event->type()) {
      case QEvent::MouseButtonRelease:
      case QEvent::Drop:
      case QEvent::KeyPress:
        if (m_onGesture) m_onGesture();
        break;
      default:
        break;
    }
    return QObject::eventFilter(watched, event);
  }

private:
  std::function<void()> m_onGesture;
};

qint64 nowMs() { return QDateTime::currentMSecsSinceEpoch(); }

void walk(QgsLayerTreeGroup* group, const QString& groupKey, const QString& path, KaLayerTreeSnapshot* out) {
  QStringList keys;
  for (QgsLayerTreeNode* child : group->children()) {
    if (auto* layer = qobject_cast<QgsLayerTreeLayer*>(child)) {
      const QString key = QStringLiteral("L:") + layer->layerId();
      keys << key;
      out->checked.insert(key, layer->itemVisibilityChecked());
    } else if (auto* sub = qobject_cast<QgsLayerTreeGroup*>(child)) {
      const QString subPath = path + QLatin1Char('/') + sub->name();
      const QString key = QStringLiteral("G:") + subPath;
      keys << key;
      out->checked.insert(key, sub->itemVisibilityChecked());
      walk(sub, key, subPath, out);
    }
  }
  out->children.insert(groupKey, keys);
}

QgsLayerTreeGroup* groupByPath(QgsLayerTree* root, const QString& path) {
  QgsLayerTreeGroup* group = root;
  const QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
  for (const QString& name : parts) {
    QgsLayerTreeGroup* next = nullptr;
    for (QgsLayerTreeNode* child : group->children()) {
      auto* sub = qobject_cast<QgsLayerTreeGroup*>(child);
      if (sub && sub->name() == name) { next = sub; break; }
    }
    if (!next) {
      // Moved to another parent since: fall back to the name anywhere in the tree.
      return parts.isEmpty() ? nullptr : root->findGroup(parts.last());
    }
    group = next;
  }
  return group;
}

QgsLayerTreeNode* nodeFor(QgsLayerTree* root, const QString& key) {
  if (key.startsWith(QLatin1String("L:"))) return root->findLayer(key.mid(2));
  if (key.startsWith(QLatin1String("G:"))) return groupByPath(root, key.mid(2));
  return nullptr;
}
}  // namespace

QStringList KaLayerTreeSnapshot::rowKeys() const {
  QStringList keys = checked.keys();
  keys.sort();
  return keys;
}

KaLayerTreeUndo::KaLayerTreeUndo(QgsProject* project, QgsLayerTreeView* view, Push push)
    : m_project(project), m_view(view), m_push(std::move(push)) {
  auto filter = std::make_unique<GestureFilter>([this] { noteUserGesture(); });
  if (view) {
    view->installEventFilter(filter.get());
    if (view->viewport()) view->viewport()->installEventFilter(filter.get());
  }
  m_context = std::move(filter);
  // The timer lives and dies with the context object.
  m_timer = new QTimer(m_context.get());
  m_timer->setSingleShot(true);
  m_timer->setInterval(0);
  QObject::connect(m_timer, &QTimer::timeout, m_context.get(), [this] { check(); });
  if (QgsLayerTree* root = project ? project->layerTreeRoot() : nullptr) {
    const auto changed = [this] { scheduleCheck(); };
    QObject::connect(root, &QgsLayerTreeNode::visibilityChanged, m_context.get(), changed);
    QObject::connect(root, &QgsLayerTreeNode::addedChildren, m_context.get(), changed);
    QObject::connect(root, &QgsLayerTreeNode::removedChildren, m_context.get(), changed);
    m_baseline = capture(root);
  }
}

KaLayerTreeUndo::~KaLayerTreeUndo() {
  // The context owns the timer and the connections; Qt also drops it from the
  // view's event filters when it is deleted.
  m_context.reset();
}

std::shared_ptr<KaLayerTreeSnapshot> KaLayerTreeUndo::capture(QgsLayerTree* root) {
  auto out = std::make_shared<KaLayerTreeSnapshot>();
  if (root) walk(root, QString(), QString(), out.get());
  return out;
}

bool KaLayerTreeUndo::restoreInto(QgsLayerTree* root, const KaLayerTreeSnapshot& snapshot, QString* error) {
  if (!root) {
    if (error) *error = QStringLiteral("레이어 목록이 없습니다.");
    return false;
  }
  // Parents before children, so a group is in place before its rows are ordered.
  QStringList groups = snapshot.children.keys();
  std::sort(groups.begin(), groups.end(), [](const QString& a, const QString& b) {
    const int da = int(a.count(QLatin1Char('/'))), db = int(b.count(QLatin1Char('/')));
    return da != db ? da < db : a < b;
  });
  int found = 0;
  for (const QString& groupKey : std::as_const(groups)) {
    auto* group = groupKey.isEmpty() ? root : qobject_cast<QgsLayerTreeGroup*>(nodeFor(root, groupKey));
    if (!group) continue;
    int position = 0;
    for (const QString& key : snapshot.children.value(groupKey)) {
      QgsLayerTreeNode* node = nodeFor(root, key);
      if (!node) continue;  // removed since; its own undo brings it back
      ++found;
      const QList<QgsLayerTreeNode*> rows = group->children();
      if (position < rows.size() && rows.at(position) == node) {
        ++position;
        continue;
      }
      auto* from = qobject_cast<QgsLayerTreeGroup*>(node->parent());
      if (!from) continue;
      // Insert the copy before removing the original: a layer with no tree node at
      // all would be dropped from the project by the registry bridge.
      group->insertChildNode(std::min(position, int(rows.size())), node->clone());
      from->removeChildNode(node);
      ++position;
    }
  }
  for (auto it = snapshot.checked.cbegin(); it != snapshot.checked.cend(); ++it) {
    QgsLayerTreeNode* node = nodeFor(root, it.key());
    if (node && node->itemVisibilityChecked() != it.value()) node->setItemVisibilityChecked(it.value());
  }
  if (found == 0 && !snapshot.checked.isEmpty()) {
    if (error) *error = QStringLiteral("되돌릴 레이어가 목록에 없습니다.");
    return false;
  }
  return true;
}

bool KaLayerTreeUndo::restore(const KaLayerTreeSnapshot& snapshot, QString* error) {
  QgsLayerTree* root = m_project ? m_project->layerTreeRoot() : nullptr;
  m_restoring = true;
  const bool ok = restoreInto(root, snapshot, error);
  m_restoring = false;
  // The restored state is the new baseline; the app's own follow-up moves fold into it.
  m_baseline = capture(root);
  m_gestureUntilMs = 0;
  return ok;
}

void KaLayerTreeUndo::noteUserGesture() {
  m_gestureUntilMs = nowMs() + kGestureWindowMs;
}

void KaLayerTreeUndo::scheduleCheck() {
  if (m_timer && !m_timer->isActive()) m_timer->start();
}

void KaLayerTreeUndo::check() {
  if (!m_project) return;
  auto now = capture(m_project->layerTreeRoot());
  if (!m_baseline) {
    m_baseline = now;
    return;
  }
  if (*now == *m_baseline) return;
  // Only a pure reorder / check change by the user becomes an undo step. A row that
  // appeared or disappeared (a layer, or a group - even an empty one deleted with
  // the Delete key) has its own LayersRemoved / LayerAdded step, and a step here
  // would sit on top of it and swallow the first Ctrl+Z. Automatic moves are not
  // the user's.
  const bool sameRows = now->rowKeys() == m_baseline->rowKeys();
  const bool byUser = !m_restoring && nowMs() <= m_gestureUntilMs;
  if (sameRows && byUser && m_push) m_push(m_baseline);
  m_baseline = now;
}
