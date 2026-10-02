#include "KaLayerListChrome.h"

#include "KaIcons.h"
#include "KaTheme.h"

#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

#include <qgslayertree.h>
#include <qgslayertreemodel.h>
#include <qgslayertreeview.h>
#include <qgslayertreeviewindicator.h>
#include <qgsvectorlayer.h>

namespace {
QgsLayerTreeGroup* rootOf(QgsLayerTreeView* view) {
  QgsLayerTreeModel* model = view ? view->layerTreeModel() : nullptr;
  return model ? model->rootGroup() : nullptr;
}
}  // namespace

KaLayerListChrome::KaLayerListChrome(QgsLayerTreeView* view, QWidget* parent) : QWidget(parent), m_view(view) {
  setObjectName(QStringLiteral("layerListChrome"));
  auto* column = new QVBoxLayout(this);
  column->setContentsMargins(0, 0, 0, 0);
  column->setSpacing(4);
  m_edit = new QLineEdit(this);
  m_edit->setObjectName(QStringLiteral("layerFilterEdit"));
  m_edit->setPlaceholderText(QStringLiteral("레이어 찾기"));
  m_edit->setClearButtonEnabled(true);
  m_edit->setMinimumHeight(30);
  m_edit->addAction(KaIcons::icon(QStringLiteral("search"), KaTheme::tokens().inkMuted), QLineEdit::LeadingPosition);
  m_edit->installEventFilter(this);
  column->addWidget(m_edit);
  m_empty = new QLabel(this);
  m_empty->setObjectName(QStringLiteral("layerFilterEmpty"));
  m_empty->setWordWrap(true);
  m_empty->hide();
  column->addWidget(m_empty);

  m_debounce.setSingleShot(true);
  m_debounce.setInterval(kDebounceMs);
  connect(&m_debounce, &QTimer::timeout, this, &KaLayerListChrome::applyFilter);
  connect(m_edit, &QLineEdit::textChanged, this, [this] { m_debounce.start(); });

  m_pencil = new QgsLayerTreeViewIndicator(this);
  m_pencil->setIcon(KaIcons::icon(QStringLiteral("pencil"), KaTheme::iconPalette().ink));
  m_pencil->setToolTip(QStringLiteral("편집 중 · 저장하지 않은 변경이 있습니다 (「저장」 Ctrl+S)"));

  if (!m_view) return;
  m_dragWasEnabled = m_view->dragEnabled();
  if (QAbstractItemModel* shown = m_view->model()) {
    connect(shown, &QAbstractItemModel::rowsInserted, this, &KaLayerListChrome::updateEmptySentence);
    connect(shown, &QAbstractItemModel::rowsRemoved, this, &KaLayerListChrome::updateEmptySentence);
    connect(shown, &QAbstractItemModel::modelReset, this, &KaLayerListChrome::updateEmptySentence);
    connect(shown, &QAbstractItemModel::layoutChanged, this, &KaLayerListChrome::updateEmptySentence);
  }
  if (QgsLayerTreeGroup* root = rootOf(m_view)) {
    connect(root, &QgsLayerTreeNode::addedChildren, this, &KaLayerListChrome::watchLayers);
    watchLayers();
  }
}

QString KaLayerListChrome::filterText() const { return m_edit->text().trimmed(); }

void KaLayerListChrome::setFilterText(const QString& text) {
  m_debounce.stop();
  if (m_edit->text() != text) {
    const QSignalBlocker quiet(m_edit);
    m_edit->setText(text);
  }
  applyFilter();
}

bool KaLayerListChrome::isEmptySentenceShown() const { return !m_empty->isHidden(); }

QString KaLayerListChrome::emptySentence() const { return m_empty->text(); }

bool KaLayerListChrome::eventFilter(QObject* watched, QEvent* event) {
  if (watched == m_edit && event->type() == QEvent::KeyPress &&
      static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
    setFilterText(QString());
    return true;
  }
  return QWidget::eventFilter(watched, event);
}

void KaLayerListChrome::applyFilter() {
  if (!m_view) return;
  const QString text = filterText();
  const bool filtering = !text.isEmpty();
  if (filtering && !m_filtering) m_dragWasEnabled = m_view->dragEnabled();
  // Taken before the proxy removes rows: the selection model then moves the current
  // index to a neighbour, which would hide that the chosen row was filtered away.
  QgsLayerTreeNode* current = m_view->currentIndex().isValid() ? m_view->currentNode() : nullptr;
  if (QgsLayerTreeProxyModel* proxy = m_view->proxyModel()) proxy->setFilterText(text);
  // No reordering while rows are hidden: a drop between hidden rows lands somewhere unseen.
  m_view->setDragEnabled(filtering ? false : m_dragWasEnabled);
  m_filtering = filtering;
  // A current row the filter hid must not stay current for 「표시 설정」.
  if (filtering && current && !m_view->node2index(current).isValid()) m_view->setCurrentLayer(nullptr);
  updateEmptySentence();
}

int KaLayerListChrome::visibleLayerRows(const QModelIndex& parent) const {
  const QAbstractItemModel* shown = m_view->model();
  int count = 0;
  for (int row = 0; row < shown->rowCount(parent); ++row) {
    const QModelIndex index = shown->index(row, 0, parent);
    QgsLayerTreeNode* node = m_view->index2node(index);
    if (!node) continue;  // legend rows
    if (QgsLayerTree::isLayer(node)) ++count;
    else count += visibleLayerRows(index);
  }
  return count;
}

int KaLayerListChrome::visibleLayerCount() const {
  return m_view && m_view->model() ? visibleLayerRows(QModelIndex()) : 0;
}

void KaLayerListChrome::updateEmptySentence() {
  const bool none = m_filtering && visibleLayerCount() == 0;
  if (none) m_empty->setText(QStringLiteral("「%1」에 맞는 레이어가 없습니다.").arg(filterText()));
  m_empty->setVisible(none);
}

void KaLayerListChrome::watchLayers() {
  QgsLayerTreeGroup* root = rootOf(m_view);
  if (!root) return;
  const QList<QgsLayerTreeLayer*> leaves = root->findLayers();
  for (QgsLayerTreeLayer* leaf : leaves) {
    auto* vector = qobject_cast<QgsVectorLayer*>(leaf->layer());
    if (!vector || m_watched.contains(vector->id())) continue;
    m_watched.insert(vector->id());
    const auto sync = [this, vector] { syncIndicator(vector); };
    connect(vector, &QgsVectorLayer::editingStarted, this, sync);
    connect(vector, &QgsVectorLayer::editingStopped, this, sync);
    connect(vector, &QgsVectorLayer::layerModified, this, sync);
    connect(vector, &QObject::destroyed, this, [this, id = vector->id()] { m_watched.remove(id); });
    syncIndicator(vector);
  }
}

void KaLayerListChrome::syncEditIndicators() {
  QgsLayerTreeGroup* root = rootOf(m_view);
  if (!root) return;
  const QList<QgsLayerTreeLayer*> leaves = root->findLayers();
  for (QgsLayerTreeLayer* leaf : leaves)
    if (auto* vector = qobject_cast<QgsVectorLayer*>(leaf->layer())) syncIndicator(vector);
}

void KaLayerListChrome::syncIndicator(QgsVectorLayer* layer) {
  QgsLayerTreeGroup* root = rootOf(m_view);
  QgsLayerTreeLayer* node = root && layer ? root->findLayer(layer->id()) : nullptr;
  if (!node) return;
  const bool on = layer->isEditable() && layer->isModified();
  const bool shown = m_view->indicators(node).contains(m_pencil);
  if (on && !shown) m_view->addIndicator(node, m_pencil);
  else if (!on && shown) m_view->removeIndicator(node, m_pencil);
}
