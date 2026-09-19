#include "KaLayerInformation.h"
#include "core/LayerLabelControls.h"
#include "core/LayerOps.h"

#include <QCheckBox>
#include <QBrush>
#include <QFont>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QKeyEvent>
#include <QSignalBlocker>
#include <QScrollBar>
#include <QScrollArea>
#include <QSplitter>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <qgslayertree.h>
#include <qgslayertreeview.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

namespace {
int labelColumnWidth(const QgsLayerTreeView* view) {
  return qMax(88, view->fontMetrics().horizontalAdvance(QStringLiteral("이름·면적")) +
      view->style()->pixelMetric(QStyle::PM_IndicatorWidth, nullptr, view) + 18);
}
}

int KaLayerInformationView::minimumListHeight() const {
  const int hinted = sizeHintForRow(0);
  const int row = qMax(22, hinted > 0 ? hinted : fontMetrics().height() + 10);
  const int head = header() ? qMax(header()->sizeHint().height(), fontMetrics().height() + 6) : row;
  return head + row * kMinVisibleRows + frameWidth() * 2;
}

void KaLayerInformationView::protectSidebarList(QSplitter* split, QgsLayerTreeView* tree,
                                                QToolButton* filesToggle, QWidget* filesPane,
                                                KaLayerInformationPanel* panel) {
  if (!split || !tree) return;
  const int hinted = tree->sizeHintForRow(0);
  const int row = qMax(22, hinted > 0 ? hinted : tree->fontMetrics().height() + 10);
  const int head = tree->header() ? qMax(tree->header()->sizeHint().height(),
                                         tree->fontMetrics().height() + 6)
                                  : row;
  const int need = head + row * kMinVisibleRows + tree->frameWidth() * 2;
  tree->setMinimumHeight(need);
  if (split->count() > 1) split->setCollapsible(1, true);
  if (filesPane) filesPane->setMinimumHeight(0);
  const int chrome = 48;
  const int handle = split->handleWidth();
  const int treeH = tree->viewport() ? tree->viewport()->height() : tree->height();
  const bool filesOpen = filesToggle && filesToggle->isChecked();
  const bool tight = split->height() < need + chrome ||
                     (filesOpen && split->height() < need + chrome + 160) ||
                     (treeH > 0 && treeH < row * kMinVisibleRows);
  if (tight && panel) panel->collapseDetails();
  if (tight && filesToggle) filesToggle->setChecked(false);
  if (tight && filesPane) filesPane->hide();
  if (tight && split->count() >= 2) {
    const int layers = qMax(need + chrome, split->height() - handle);
    split->setSizes({layers, 0});
  }
}

void KaLayerInformationView::resizeEvent(QResizeEvent* event) {
  const int layerWidth = m_columnsSized ? header()->sectionSize(0) : 0;
  QgsLayerTreeView::resizeEvent(event);
  // QGIS's one-column view makes EVERY header section at least viewport-wide.
  // Restore the user's split after that native resize hook, keeping both visible.
  header()->setMinimumSectionSize(24);
  const int maximumLayerWidth = qMax(24, viewport()->width() - labelColumnWidth(this));
  header()->setMaximumSectionSize(maximumLayerWidth);
  if (header()->count() == 2) {
    const int desiredWidth = m_columnsSized ? layerWidth : viewport()->width() - labelColumnWidth(this);
    header()->resizeSection(0, qBound(24, desiredWidth, maximumLayerWidth));
    m_columnsSized = true;
  }
  horizontalScrollBar()->setValue(0);
}

KaLayerInformationModel::KaLayerInformationModel(QgsProject* project, bool layout, QObject* parent)
    : QgsLayerTreeModel(project->layerTreeRoot(), parent), m_project(project), m_layout(layout) {
  m_stateTimer.setSingleShot(true);
  m_stateTimer.setInterval(0);
  m_editTimer.setSingleShot(true);
  m_editTimer.setInterval(120);
  connect(&m_editTimer, &QTimer::timeout, this, &KaLayerInformationModel::labelsEdited);
  connect(&m_stateTimer, &QTimer::timeout, this, [this] {
    const auto visit = [this](auto&& self, QgsLayerTreeNode* node) -> void {
      for (auto* child : node->children()) {
        const auto item = node2index(child).siblingAtColumn(1);
        emit dataChanged(item.siblingAtColumn(0), item,
            {Qt::DisplayRole, Qt::CheckStateRole, Qt::ToolTipRole, Qt::ForegroundRole, Qt::FontRole});
        self(self, child);
      }
    };
    visit(visit, rootGroup());
    emit labelStateChanged();
  });
  connect(rootGroup(), &QgsLayerTreeNode::visibilityChanged, this, &KaLayerInformationModel::refreshLabels);
  connect(project, &QgsProject::layersAdded, this, [this](const QList<QgsMapLayer*>& layers) {
    for (auto* layer : layers) watch(layer);
    refreshLabels();
  });
  connect(project, &QgsProject::layersRemoved, this, &KaLayerInformationModel::refreshLabels);
  for (auto* layer : project->mapLayers()) watch(layer);
}

void KaLayerInformationModel::watch(QgsMapLayer* layer) {
  connect(layer, &QgsMapLayer::styleChanged, this, &KaLayerInformationModel::refreshLabels);
  connect(layer, &QgsMapLayer::repaintRequested, this, &KaLayerInformationModel::refreshLabels);
  connect(layer, &QgsMapLayer::customPropertyChanged, this, &KaLayerInformationModel::refreshLabels);
  if (auto* vector = qobject_cast<QgsVectorLayer*>(layer))
    connect(vector, &QgsVectorLayer::updatedFields, this, &KaLayerInformationModel::refreshLabels);
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
    if (LayerLabelControls::describe(leaf->layer(), m_layout).supported) result.append(leaf->layer());
  } else if (auto* group = qobject_cast<QgsLayerTreeGroup*>(node)) {
    for (auto* child : group->findLayers())
      if (LayerLabelControls::describe(child->layer(), m_layout).supported && !result.contains(child->layer()))
        result.append(child->layer());
  }
  return result;
}

QVariant KaLayerInformationModel::data(const QModelIndex& item, int role) const {
  if (role == Qt::ForegroundRole || role == Qt::FontRole) {
    if (auto* node = index2node(item.siblingAtColumn(0))) {
      const bool group = QgsLayerTree::isGroup(node);
      if (role == Qt::ForegroundRole)
        return QBrush(QColor(!node->isVisible() ? QStringLiteral("#64727e") :
            group ? QStringLiteral("#1f5275") :
            item.column() == 1 ? QStringLiteral("#52606d") : QStringLiteral("#202831")));
      QFont font = layerTreeNodeFont(node->nodeType());
      font.setBold(group);
      if (!node->isVisible()) font.setItalic(true);
      return font;
    }
  }
  if (item.column() != 1) return QgsLayerTreeModel::data(item, role);
  auto* node = index2node(item.siblingAtColumn(0));
  if (!node) return {};
  auto* leaf = qobject_cast<QgsLayerTreeLayer*>(node);
  const auto info = LayerLabelControls::describe(leaf ? leaf->layer() : nullptr, m_layout, m_scale);
  const auto layers = targets(node);
  if (role == Qt::DisplayRole) return leaf ? (info.supported ? info.caption : QStringLiteral("—")) : QStringLiteral("글자");
  if (role == Qt::CheckStateRole && !layers.isEmpty()) {
    int enabled = 0;
    for (auto* layer : layers) if (LayerLabelControls::describe(layer, m_layout).enabled) ++enabled;
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
void KaLayerInformationModel::refreshLabels() { if (!m_stateTimer.isActive()) m_stateTimer.start(); }
void KaLayerInformationModel::notifyEdited() {
  if (m_project) m_project->setDirty(true);
  refreshLabels(); m_editTimer.start();
}
void KaLayerInformationModel::configureView(QgsLayerTreeView* view) {
  if (auto* model = qobject_cast<KaLayerInformationModel*>(view->layerTreeModel())) view->installEventFilter(model);
  view->setProperty("kaLayerInformation", true);
  QFont compactFont = view->font();
  compactFont.setPixelSize(10); // The application layer list previously used 13 px.
  view->setFont(compactFont);
  if (auto* model = view->layerTreeModel()) {
    model->setLayerTreeNodeFont(QgsLayerTreeNode::NodeLayer, compactFont);
    compactFont.setBold(true);
    model->setLayerTreeNodeFont(QgsLayerTreeNode::NodeGroup, compactFont);
  }
  view->setAlternatingRowColors(true);
  view->setHeaderHidden(false);
  view->header()->setStretchLastSection(false);
  view->header()->setMinimumSectionSize(24);
  view->header()->setSectionResizeMode(0, QHeaderView::Interactive);
  view->header()->setSectionResizeMode(1, QHeaderView::Stretch);
  view->header()->resizeSection(0, qMax(24, view->viewport()->width() - labelColumnWidth(view)));
  view->header()->setToolTip(QStringLiteral("도면 · 레이어와 글자 사이 경계선을 끌어 너비를 조절하세요."));
  view->setMinimumWidth(300);
  // Keep the header and at least four compact rows usable in a short sidebar.
  view->setMinimumHeight(view->header()->sizeHint().height() + 4 * 22 + 4);
  view->setIndentation(12);
  view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  view->setUniformRowHeights(true);
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

void KaLayerInformationPanel::collapseDetails() {
  if (auto* toggle = findChild<QToolButton*>(QStringLiteral("layerInformationToggle")))
    toggle->setChecked(false);
}

KaLayerInformationPanel::KaLayerInformationPanel(KaLayerInformationModel* model, QgsLayerTreeView* view, QWidget* parent)
    : QWidget(parent), m_model(model), m_view(view) {
  setObjectName(QStringLiteral("layerInformationPanel"));
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  auto* outer = new QVBoxLayout(this); outer->setContentsMargins(0, 0, 0, 0); outer->setSpacing(4);
  auto* toggle = new QToolButton(this);
  toggle->setObjectName(QStringLiteral("layerInformationToggle"));
  toggle->setText(QStringLiteral("표시 설정"));
  toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  toggle->setArrowType(Qt::RightArrow);
  toggle->setCheckable(true);
  toggle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  outer->addWidget(toggle);
  auto* scroll = new QScrollArea(this);
  scroll->setObjectName(QStringLiteral("layerInformationScroll"));
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setMinimumHeight(40);
  scroll->setMaximumHeight(170);
  auto* contents = new QWidget(scroll);
  auto* layout = new QVBoxLayout(contents); layout->setContentsMargins(4, 4, 4, 4); layout->setSpacing(4);
  scroll->setWidget(contents);
  outer->addWidget(scroll);
  scroll->hide();
  connect(toggle, &QToolButton::toggled, this, [toggle, scroll](bool open) {
    toggle->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
    scroll->setVisible(open);
  });
  m_title = new QLabel(this); m_title->setWordWrap(true); layout->addWidget(m_title);
  m_labels = new QCheckBox(this); m_labels->setObjectName(QStringLiteral("layerLabelVisible")); layout->addWidget(m_labels);
  m_reason = new QLabel(this); m_reason->setObjectName(QStringLiteral("layerLabelReason"));
  m_reason->setWordWrap(true); layout->addWidget(m_reason);
  auto* form = new QFormLayout(); form->setContentsMargins(0, 0, 0, 0);
  m_details = form;
  form->setRowWrapPolicy(QFormLayout::WrapLongRows);
  m_field = new QComboBox(this); m_field->setObjectName(QStringLiteral("layerLabelField"));
  m_field->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon); m_field->setMinimumContentsLength(8);
  form->addRow(QStringLiteral("내용"), m_field);
  m_area = new QCheckBox(QStringLiteral("면적(㎡) 함께 표시"), this); m_area->setObjectName(QStringLiteral("layerLabelArea"));
  form->addRow(m_area);
  m_size = new QDoubleSpinBox(this); m_size->setObjectName(QStringLiteral("layerLabelSize"));
  m_size->setRange(1., 72.); m_size->setDecimals(1); m_size->setSuffix(QStringLiteral(" pt"));
  m_size->setKeyboardTracking(false); form->addRow(QStringLiteral("크기"), m_size);
  layout->addLayout(form);
  connect(view->selectionModel(), &QItemSelectionModel::currentChanged, this, [this] { refresh(); });
  connect(model, &KaLayerInformationModel::labelStateChanged, this, &KaLayerInformationPanel::refresh);
  connect(m_labels, &QCheckBox::clicked, this, [this] {
    if (!m_view || !m_model) return;
    auto* node = m_view->currentNode(); if (!node) return;
    const auto item = m_model->node2index(node).siblingAtColumn(1);
    const bool on = m_model->data(item, Qt::CheckStateRole).toInt() != Qt::Checked;
    m_model->setData(item, on ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole); refresh();
  });
  connect(m_field, &QComboBox::activated, this, [this](int index) {
    auto* vector = m_view ? qobject_cast<QgsVectorLayer*>(m_view->currentLayer()) : nullptr;
    if (!vector || !m_model || m_field->itemData(index).isNull()) return;
    if (LayerLabelControls::setField(vector, m_field->itemData(index).toString())) m_model->notifyEdited();
    refresh();
  });
  connect(m_area, &QCheckBox::clicked, this, [this](bool checked) {
    auto* vector = m_view ? qobject_cast<QgsVectorLayer*>(m_view->currentLayer()) : nullptr;
    if (!vector || !m_model) return;
    const auto info = LayerLabelControls::describe(vector, m_model->layoutMode());
    if (!info.areaEditable) return;
    if (LayerLabelControls::setArea(vector, checked)) m_model->notifyEdited();
    refresh();
  });
  connect(m_size, &QDoubleSpinBox::valueChanged, this, [this](double size) {
    auto* vector = m_view ? qobject_cast<QgsVectorLayer*>(m_view->currentLayer()) : nullptr;
    if (vector && m_model && LayerOps::setLabelFontSize(vector, size)) m_model->notifyEdited();
  });
  refresh();
}

void KaLayerInformationPanel::refresh() {
  if (!m_view || !m_model) return;
  const QSignalBlocker labelsBlock(m_labels), fieldBlock(m_field), areaBlock(m_area), sizeBlock(m_size);
  auto* node = m_view->currentNode();
  auto* vector = qobject_cast<QgsVectorLayer*>(m_view->currentLayer());
  const auto info = LayerLabelControls::describe(m_view->currentLayer(), m_model->layoutMode(), m_model->scale());
  m_title->setText(node ? node->name() + QStringLiteral(" · 표시 설정") : QStringLiteral("도면을 선택하면 표시 설정이 나옵니다."));
  const auto item = node ? m_model->node2index(node).siblingAtColumn(1) : QModelIndex();
  const auto state = m_model->data(item, Qt::CheckStateRole);
  m_labels->setTristate(state.isValid() && state.toInt() == Qt::PartiallyChecked);
  m_labels->setCheckState(state.isValid() ? static_cast<Qt::CheckState>(state.toInt()) : Qt::Unchecked);
  m_labels->setText(info.supported ? info.caption + QStringLiteral(" 표시") : QStringLiteral("글자 표시"));
  m_labels->setEnabled(item.isValid() && (m_model->flags(item) & Qt::ItemIsUserCheckable));
  m_reason->setText(node ? m_model->data(item, Qt::ToolTipRole).toString() : QString());
  m_reason->setVisible(!m_reason->text().isEmpty());
  m_field->clear();
  m_field->addItem(info.needsField ? QStringLiteral("표시할 내용 선택…") : QStringLiteral("현재 설정 유지"));
  const bool numbered = m_model->layoutMode() && LayerLabelControls::isHeritage(vector);
  // Specialized expressions (height, thematic classes, layout numbers) are not
  // overwritten by a generic field picker. Unknown imports require explicit choice.
  const bool fieldEditable = vector && !numbered && info.fieldEditable;
  if (fieldEditable) {
    for (const auto& field : vector->fields()) {
      m_field->addItem(field.alias().isEmpty() ? field.name() : field.alias(), field.name());
      if (field.name() == info.field) m_field->setCurrentIndex(m_field->count() - 1);
    }
  }
  m_field->setEnabled(fieldEditable && node && node->isVisible());
  m_details->setRowVisible(m_field, fieldEditable);
  m_area->setEnabled(vector && info.areaEditable && node && node->isVisible());
  m_details->setRowVisible(m_area, vector && info.areaEditable && !numbered);
  m_area->setChecked(vector && info.areaEditable && LayerOps::labelShowArea(vector));
  m_size->setEnabled(vector && vector->labeling() && info.supported && !numbered && node && node->isVisible());
  m_details->setRowVisible(m_size, vector && vector->labeling() && info.supported && !numbered);
  m_size->setValue(vector ? LayerOps::labelFontSize(vector, 8.) : 8.);
}
