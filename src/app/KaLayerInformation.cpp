#include "KaLayerInformation.h"
#include "core/HeritageStyle.h"
#include "core/LayerLabelControls.h"
#include "core/LayerOps.h"
#include "core/LayerTreePolicy.h"

#include <QBrush>
#include <QFont>
#include <QKeyEvent>
#include <cmath>
#include <qgslayertree.h>
#include <qgslayertreeview.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

namespace {
double relativeLuminance(const QColor& c) {
  const auto channel = [](double v) { return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
  return 0.2126 * channel(c.redF()) + 0.7152 * channel(c.greenF()) + 0.0722 * channel(c.blueF());
}

// Heritage colors are map-symbol colors; as 10 px list text on white, yellow
// (#D4AA00) reads at 2.2:1. Keep the hue, darken until 4.5:1 on the darkest row
// tint (hover, KaTheme pressedBottom #E1ECF7).
QColor readableListInk(QColor ink) {
  const double limit = (relativeLuminance(QColor(0xE1, 0xEC, 0xF7)) + 0.05) / 4.5 - 0.05;
  for (int i = 0; i < 24 && relativeLuminance(ink) > limit; ++i) ink = ink.darker(106);
  return ink;
}

// Layer list inks. Every one reads at 4.5:1 or better on the white, alternate,
// selected and hover (#E1ECF7) rows; the hidden-row ink was #64727e (4.13:1 on hover).
namespace ListInk {
constexpr const char* kHidden = "#5c6a76";
constexpr const char* kLabelColumn = "#52606d";
constexpr const char* kGroup = "#1f5275";
constexpr const char* kLayer = "#202831";
}

// 목록 이름은 종류 색으로 찾는다. 색은 HeritageStyle 한 곳만 쓴다.
// 오른쪽 글자 칸은 그대로 둔다. 꺼진 줄은 종류와 상관없이 흐리게 둔다.
QColor layerListInk(QgsLayerTreeNode* node, int column) {
  if (!node->isVisible()) return QColor(QString::fromLatin1(ListInk::kHidden));
  if (column == 0) {
    for (QgsLayerTreeNode* n = node; n; n = n->parent()) {
      if (const auto dataset = HeritageStyle::fromLayerName(n->name()))
        return readableListInk(HeritageStyle::color(*dataset));
    }
  }
  if (column == 1) return QColor(QString::fromLatin1(ListInk::kLabelColumn));
  return QColor(QString::fromLatin1(QgsLayerTree::isGroup(node) ? ListInk::kGroup : ListInk::kLayer));
}

}

KaLayerInformationModel::KaLayerInformationModel(QgsProject* project, bool layout, QObject* parent)
    : QgsLayerTreeModel(project->layerTreeRoot(), parent), m_project(project), m_layout(layout) {
  m_stateTimer.setSingleShot(true);
  m_stateTimer.setInterval(0);
  m_editTimer.setSingleShot(true);
  m_editTimer.setInterval(120);
  connect(&m_editTimer, &QTimer::timeout, this, &KaLayerInformationModel::labelsEdited);
  connect(&m_stateTimer, &QTimer::timeout, this, [this] {
    if (m_fullRefresh) {
      const auto visit = [this](auto&& self, QgsLayerTreeNode* node) -> void {
        for (auto* child : node->children()) {
          const auto item = node2index(child).siblingAtColumn(1);
          emit dataChanged(item.siblingAtColumn(0), item,
              {Qt::DisplayRole, Qt::CheckStateRole, Qt::ToolTipRole, Qt::ForegroundRole, Qt::FontRole});
          self(self, child);
        }
      };
      visit(visit, rootGroup());
    } else {
      // Only the rows of layers that changed, plus their groups (a group's check
      // state summarizes its children).
      QSet<QgsLayerTreeNode*> rows;
      for (QgsLayerTreeLayer* leaf : rootGroup()->findLayers()) {
        if (!m_dirtyLayers.contains(leaf->layerId())) continue;
        for (QgsLayerTreeNode* n = leaf; n && n != rootGroup(); n = n->parent()) rows.insert(n);
      }
      for (QgsLayerTreeNode* node : std::as_const(rows)) emitRowChanged(node);
    }
    m_fullRefresh = false;
    m_dirtyLayers.clear();
    emit labelStateChanged();
  });
  connect(rootGroup(), &QgsLayerTreeNode::visibilityChanged, this, &KaLayerInformationModel::refreshLabels);
  // Heritage captions depend on parent group names; a moved or renamed row changes them.
  connect(rootGroup(), &QgsLayerTreeNode::addedChildren, this, &KaLayerInformationModel::refreshLabels);
  connect(rootGroup(), &QgsLayerTreeNode::removedChildren, this, &KaLayerInformationModel::refreshLabels);
  connect(rootGroup(), &QgsLayerTreeNode::nameChanged, this, &KaLayerInformationModel::refreshLabels);
  connect(project, &QgsProject::layersAdded, this, [this](const QList<QgsMapLayer*>& layers) {
    for (auto* layer : layers) watch(layer);
    refreshLabels();
  });
  connect(project, &QgsProject::layersRemoved, this, &KaLayerInformationModel::refreshLabels);
  for (auto* layer : project->mapLayers()) watch(layer);
}

void KaLayerInformationModel::watch(QgsMapLayer* layer) {
  const QString id = layer->id();
  const auto changed = [this, id] { refreshLayer(id); };
  connect(layer, &QgsMapLayer::styleChanged, this, changed);
  connect(layer, &QgsMapLayer::repaintRequested, this, changed);
  connect(layer, &QgsMapLayer::customPropertyChanged, this, changed);
  connect(layer, &QgsMapLayer::nameChanged, this, changed);
  if (auto* vector = qobject_cast<QgsVectorLayer*>(layer))
    connect(vector, &QgsVectorLayer::updatedFields, this, changed);
}

void KaLayerInformationModel::emitRowChanged(QgsLayerTreeNode* node) {
  const auto item = node2index(node);
  if (!item.isValid()) return;
  emit dataChanged(item.siblingAtColumn(0), item.siblingAtColumn(1),
      {Qt::DisplayRole, Qt::CheckStateRole, Qt::ToolTipRole, Qt::ForegroundRole, Qt::FontRole});
}

LayerLabelControls::Info KaLayerInformationModel::labelInfo(const QgsMapLayer* layer, bool withScale) const {
  if (!layer) return LayerLabelControls::describe(nullptr, m_layout);
  const QString key = layer->id() + (withScale ? QStringLiteral("|s") : QStringLiteral("|0"));
  const auto hit = m_infoCache.constFind(key);
  if (hit != m_infoCache.constEnd()) return hit.value();
  ++m_describeCalls;
  const auto info = LayerLabelControls::describe(layer, m_layout, withScale ? m_scale : 0.);
  m_infoCache.insert(key, info);
  return info;
}

int KaLayerInformationModel::columnCount(const QModelIndex&) const { return 2; }
int KaLayerInformationModel::rowCount(const QModelIndex& parent) const {
  return parent.column() > 0 ? 0 : QgsLayerTreeModel::rowCount(parent);
}
QModelIndex KaLayerInformationModel::index(int row, int column, const QModelIndex& parent) const {
  if (column < 0 || column > 1 || parent.column() > 0) return {};
  const auto base = QgsLayerTreeModel::index(row, 0, parent);
  return base.isValid() ? createIndex(row, column, base.internalPointer()) : QModelIndex();
}
QVariant KaLayerInformationModel::headerData(int section, Qt::Orientation orientation, int role) const {
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    return section == 0 ? QStringLiteral("도면 · 레이어") : QStringLiteral("글자");
  return QgsLayerTreeModel::headerData(section, orientation, role);
}

QList<QgsMapLayer*> KaLayerInformationModel::targets(QgsLayerTreeNode* node) const {
  QList<QgsMapLayer*> result;
  if (auto* leaf = qobject_cast<QgsLayerTreeLayer*>(node)) {
    if (labelInfo(leaf->layer()).supported) result.append(leaf->layer());
  } else if (auto* group = qobject_cast<QgsLayerTreeGroup*>(node)) {
    for (auto* child : group->findLayers())
      if (labelInfo(child->layer()).supported && !result.contains(child->layer()))
        result.append(child->layer());
  }
  return result;
}

QVariant KaLayerInformationModel::data(const QModelIndex& item, int role) const {
  if (role == Qt::ForegroundRole || role == Qt::FontRole) {
    if (auto* node = index2node(item.siblingAtColumn(0))) {
      const bool group = QgsLayerTree::isGroup(node);
      if (role == Qt::ForegroundRole)
        return QBrush(layerListInk(node, item.column()));
      QFont font = layerTreeNodeFont(node->nodeType());
      font.setBold(group);
      // 꺼진 레이어는 위의 옅은 글자색으로 구분한다. 맑은 고딕에는 기울임꼴이 없어
      // setItalic 은 한글을 억지로 비틀어 그렸다.
      font.setItalic(false);
      return font;
    }
  }
  if (item.column() == 0 && role == Qt::ToolTipRole) {
    // Rows whose map position is fixed by a user rule say so, instead of a drag
    // that silently does nothing (background below, satellite at the bottom,
    // survey shapes drawn again above labels).
    const QVariant base = QgsLayerTreeModel::data(item, role);
    auto* leaf = qobject_cast<QgsLayerTreeLayer*>(index2node(item));
    const QString note = leaf ? LayerTreePolicy::forcedOrderNote(leaf->layer(), m_project) : QString();
    if (note.isEmpty()) return base;
    const QString text = base.toString();
    return text.isEmpty() ? note : text + QStringLiteral("<br/><br/>") + note.toHtmlEscaped();
  }
  if (item.column() != 1) return QgsLayerTreeModel::data(item, role);
  auto* node = index2node(item.siblingAtColumn(0));
  if (!node) return {};
  auto* leaf = qobject_cast<QgsLayerTreeLayer*>(node);
  const auto info = labelInfo(leaf ? leaf->layer() : nullptr, true);
  const auto layers = targets(node);
  if (role == Qt::DisplayRole) return leaf ? (info.supported ? info.caption : QStringLiteral("—")) : QStringLiteral("글자");
  if (role == Qt::CheckStateRole && !layers.isEmpty()) {
    int enabled = 0;
    for (auto* layer : layers) if (labelInfo(layer).enabled) ++enabled;
    return enabled == layers.size() ? Qt::Checked : enabled == 0 ? Qt::Unchecked : Qt::PartiallyChecked;
  }
  if (role == Qt::ToolTipRole || role == Qt::AccessibleDescriptionRole) {
    if (!node->isVisible()) return QStringLiteral("도면 또는 상위 그룹이 꺼져 있습니다. 글자 설정은 기억합니다.");
    if (leaf) return info.reason.isEmpty() ? info.caption + QStringLiteral("만 켜고 끕니다. 도형은 유지됩니다.") : info.reason;
    return QStringLiteral("하위 도면의 글자를 함께 켜고 끕니다. 일부 켜짐은 중간 체크로 표시됩니다.");
  }
  if (role == Qt::AccessibleTextRole) return node->name() + QStringLiteral(" 글자 표시");
  return {};
}

Qt::ItemFlags KaLayerInformationModel::flags(const QModelIndex& item) const {
  if (item.column() != 1) return QgsLayerTreeModel::flags(item);
  auto* node = index2node(item.siblingAtColumn(0));
  if (!node) return Qt::NoItemFlags;
  Qt::ItemFlags result = Qt::ItemIsSelectable;
  if (node->isVisible() && !targets(node).isEmpty()) result |= Qt::ItemIsEnabled | Qt::ItemIsUserCheckable;
  return result;
}

bool KaLayerInformationModel::setData(const QModelIndex& item, const QVariant& value, int role) {
  if (item.column() != 1) return QgsLayerTreeModel::setData(item, value, role);
  if (role != Qt::CheckStateRole || !(flags(item) & Qt::ItemIsEnabled)) return false;
  const bool enabled = value.toInt() != Qt::Unchecked;
  bool changed = false;
  for (auto* layer : targets(index2node(item.siblingAtColumn(0))))
    changed = LayerLabelControls::setVisible(layer, enabled, m_layout) || changed;
  if (changed) notifyEdited();
  return changed;
}
void KaLayerInformationModel::setScale(double scale) {
  if (qFuzzyCompare(m_scale, scale)) return;
  m_scale = scale; refreshLabels();
}
void KaLayerInformationModel::refreshLabels() {
  m_infoCache.clear();
  m_fullRefresh = true;
  if (!m_stateTimer.isActive()) m_stateTimer.start();
}
void KaLayerInformationModel::refreshLayer(const QString& layerId) {
  // Drop the cached answer now, so data() asked before the timer is current.
  m_infoCache.remove(layerId + QStringLiteral("|s"));
  m_infoCache.remove(layerId + QStringLiteral("|0"));
  m_dirtyLayers.insert(layerId);
  if (!m_stateTimer.isActive()) m_stateTimer.start();
}
void KaLayerInformationModel::notifyEdited() {
  if (m_project) m_project->setDirty(true);
  refreshLabels(); m_editTimer.start();
}
bool KaLayerInformationModel::eventFilter(QObject* watched, QEvent* event) {
  auto* view = qobject_cast<QgsLayerTreeView*>(watched);
  if (view && event->type() == QEvent::KeyPress && view->currentIndex().column() == 1) {
    auto* key = static_cast<QKeyEvent*>(event);
    if (key->key() == Qt::Key_Space && key->modifiers() == Qt::NoModifier) {
      if (auto* node = view->index2node(view->currentIndex())) {
        const auto item = node2index(node).siblingAtColumn(1);
        setData(item, data(item, Qt::CheckStateRole).toInt() == Qt::Checked ? Qt::Unchecked : Qt::Checked, Qt::CheckStateRole);
      }
      return true;
    }
  }
  return QgsLayerTreeModel::eventFilter(watched, event);
}
