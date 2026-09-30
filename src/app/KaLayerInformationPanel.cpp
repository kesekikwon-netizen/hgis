// 표시 설정 panel under the layer list (split from KaLayerInformation.cpp).
#include "KaLayerInformation.h"
#include "core/LayerLabelControls.h"
#include "core/LayerOps.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QIcon>
#include <QLabel>
#include <QPixmap>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>
#include <iterator>
#include <qgslayertree.h>
#include <qgslayertreeview.h>
#include <qgsvectorlayer.h>

namespace {
// Optional label inks for dense feature drawings (표시 설정). Default first.
struct LabelInk {
  const char* name;
  const char* hex;
};
constexpr LabelInk kLabelInks[] = {{"기본 (먹색)", "#1f2328"}, {"흰색", "#ffffff"},
                                   {"진한 파랑", "#1e3a8a"}, {"진한 빨강", "#991b1b"},
                                   {"갈색", "#7c2d12"},       {"진한 초록", "#14532d"}};
}  // namespace

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
  // Optional look for dense feature drawings. Nothing changes until the user picks.
  m_color = new QComboBox(this); m_color->setObjectName(QStringLiteral("layerLabelColor"));
  for (const LabelInk& ink : kLabelInks) {
    QPixmap swatch(12, 12);
    swatch.fill(QColor(QString::fromLatin1(ink.hex)));
    m_color->addItem(QIcon(swatch), QString::fromUtf8(ink.name), QColor(QString::fromLatin1(ink.hex)));
  }
  form->addRow(QStringLiteral("글자 색"), m_color);
  m_halo = new QCheckBox(QStringLiteral("흰 테두리 (위성·지적 위에서 읽기)"), this);
  m_halo->setObjectName(QStringLiteral("layerLabelHalo"));
  form->addRow(m_halo);
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
  connect(m_color, &QComboBox::activated, this, [this](int index) {
    auto* vector = m_view ? qobject_cast<QgsVectorLayer*>(m_view->currentLayer()) : nullptr;
    const QColor color = m_color->itemData(index).value<QColor>();
    if (vector && m_model && LayerOps::setLabelColor(vector, color)) m_model->notifyEdited();
    refresh();
  });
  connect(m_halo, &QCheckBox::clicked, this, [this](bool on) {
    auto* vector = m_view ? qobject_cast<QgsVectorLayer*>(m_view->currentLayer()) : nullptr;
    if (vector && m_model && LayerOps::setLabelHalo(vector, on)) m_model->notifyEdited();
    refresh();
  });
  refresh();
}

void KaLayerInformationPanel::refresh() {
  if (!m_view || !m_model) return;
  const QSignalBlocker labelsBlock(m_labels), fieldBlock(m_field), areaBlock(m_area), sizeBlock(m_size),
      colorBlock(m_color), haloBlock(m_halo);
  // A 「현재 색」 row added for an earlier layer is not a preset: drop it.
  while (m_color->count() > int(std::size(kLabelInks))) m_color->removeItem(0);
  auto* node = m_view->currentNode();
  auto* vector = qobject_cast<QgsVectorLayer*>(m_view->currentLayer());
  const auto info = m_model->labelInfo(m_view->currentLayer(), true);
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
  const bool styled = vector && vector->labeling() && info.supported && !numbered;
  m_size->setEnabled(styled && node && node->isVisible());
  m_details->setRowVisible(m_size, styled);
  m_size->setValue(vector ? LayerOps::labelFontSize(vector, LayerOps::kDefaultLabelSizePt)
                          : LayerOps::kDefaultLabelSizePt);
  m_color->setEnabled(styled && node && node->isVisible());
  m_details->setRowVisible(m_color, styled);
  m_halo->setEnabled(styled && node && node->isVisible());
  m_details->setRowVisible(m_halo, styled);
  if (styled) {
    const QColor ink = LayerOps::labelColor(vector);
    int match = m_color->findData(ink.isValid() ? QColor(ink.rgb()) : QColor());
    if (match < 0 && ink.isValid()) {
      // Keep a colour set elsewhere visible as the current choice.
      QPixmap swatch(12, 12);
      swatch.fill(ink);
      m_color->insertItem(0, QIcon(swatch), QStringLiteral("현재 색"), QColor(ink.rgb()));
      match = 0;
    }
    m_color->setCurrentIndex(qMax(0, match));
    m_halo->setChecked(LayerOps::labelHalo(vector));
  }
}
