#include "KaInspectorStyle.h"

#include "KaStylePalette.h"
#include "KaTheme.h"
#include "core/FeaturePresets.h"
#include "core/LayerOps.h"
#include "core/LayerStyleKinds.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <qgsvectorlayer.h>

namespace {
constexpr int kFillAlpha = 50;  // the same see-through fill as a new survey area
}  // namespace

KaInspectorStyle::KaInspectorStyle(QWidget* parent) : QWidget(parent) {
  setObjectName(QStringLiteral("inspectorStyle"));
  auto* column = new QVBoxLayout(this);
  column->setContentsMargins(0, 0, 0, 0);
  column->setSpacing(8);
  m_sentence = new QLabel(this);
  m_sentence->setObjectName(QStringLiteral("inspectorSentence"));
  m_sentence->setWordWrap(true);
  m_sentence->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  column->addWidget(m_sentence);

  m_controls = new QWidget(this);
  auto* form = new QFormLayout(m_controls);
  form->setContentsMargins(0, 0, 0, 0);
  form->setVerticalSpacing(8);
  form->setRowWrapPolicy(QFormLayout::WrapAllRows);  // labels above: the swatches fit the 272 px panel
  m_layerName = new QLabel(m_controls);
  m_layerName->setWordWrap(true);
  QFont bold = m_layerName->font();
  bold.setBold(true);
  m_layerName->setFont(bold);
  form->addRow(m_layerName);
  auto* grid = new QGridLayout;
  grid->setSpacing(4);
  const QList<QColor> colors = KaStylePalette::colors();
  const QStringList names = KaStylePalette::names();
  for (int i = 0; i < colors.size(); ++i) {
    auto* swatch = new QPushButton(m_controls);
    swatch->setObjectName(QStringLiteral("inspectorStyleSwatch"));
    swatch->setFixedSize(44, 24);
    swatch->setCursor(Qt::PointingHandCursor);
    swatch->setToolTip(names.at(i));
    swatch->setAccessibleName(names.at(i));
    connect(swatch, &QPushButton::clicked, this, [this, color = colors.at(i)] { pickColor(color); });
    grid->addWidget(swatch, i / 4, i % 4);
    m_swatches << swatch;
  }
  form->addRow(QStringLiteral("색"), grid);
  // 「채우기 없음 (선만)」·「선 없음」 (user 2026-10-03 「선채우기없음도 나와야한다」).
  m_noFillCheck = new QCheckBox(QStringLiteral("채우기 없음 (선만)"), m_controls);
  m_noFillCheck->setObjectName(QStringLiteral("inspectorStyleNoFill"));
  m_noStrokeCheck = new QCheckBox(QStringLiteral("선 없음"), m_controls);
  m_noStrokeCheck->setObjectName(QStringLiteral("inspectorStyleNoStroke"));
  for (QCheckBox* check : {m_noFillCheck, m_noStrokeCheck}) {
    connect(check, &QCheckBox::toggled, this, [this, check](bool on) { setOff(check == m_noFillCheck, on); });
    form->addRow(check);
  }
  m_widthLabel = new QLabel(m_controls);
  m_width = new QDoubleSpinBox(m_controls);
  m_width->setObjectName(QStringLiteral("inspectorStyleWidth"));
  m_width->setRange(0.2, 12.0);
  m_width->setSingleStep(0.2);
  m_width->setDecimals(1);
  m_width->setSuffix(QStringLiteral(" mm"));
  connect(m_width, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &KaInspectorStyle::setWidth);
  form->addRow(m_widthLabel, m_width);
  m_note = new QLabel(m_controls);
  m_note->setWordWrap(true);
  m_note->setStyleSheet(QStringLiteral("color: %1;").arg(KaTheme::tokens().inkMuted.name()));
  form->addRow(m_note);
  column->addWidget(m_controls);
  column->addStretch(1);
  setLayer(nullptr);
}

bool KaInspectorStyle::isPoint() const {
  return m_layer && m_layer->geometryType() == Qgis::GeometryType::Point;
}

void KaInspectorStyle::setLayer(QgsVectorLayer* layer) {
  disconnect(m_watch);
  const bool lineMap = layer && (LayerOps::isReferenceLayer(layer) || LayerOps::isCadastralLayer(layer));
  m_layer = layer && layer->isValid() && !lineMap ? layer : nullptr;
  if (!m_layer) {
    m_sentence->setText(lineMap ? QStringLiteral("선으로 된 지도의 선 색은 왼쪽 목록에서 그 레이어를 오른쪽 클릭해 「면·외곽선 색」으로 바꿉니다.")
                                : QStringLiteral("지도에서 도형을 고르거나 왼쪽 목록에서 레이어를 고르면 그 레이어의 색과 굵기를 여기서 바로 바꿉니다."));
    m_sentence->show();
    m_controls->hide();
    return;
  }
  m_sentence->hide();
  m_controls->show();
  m_layerName->setText(m_layer->name());
  const bool line = m_layer->geometryType() == Qgis::GeometryType::Line;
  m_noFillCheck->setVisible(!line);  // a line has no fill, and no line would hide it
  m_noStrokeCheck->setVisible(!line);
  m_widthLabel->setText(isPoint() ? QStringLiteral("점 크기") : line ? QStringLiteral("선 굵기") : QStringLiteral("외곽선 굵기"));
  m_watch = connect(m_layer, &QgsMapLayer::rendererChanged, this, &KaInspectorStyle::refresh);  // e.g. 「면·외곽선 색」
  refresh();
}

void KaInspectorStyle::refresh() {
  if (!m_layer) return;
  readLayerStyle();
  const bool automatic = FeaturePresets::isPresetStyled(m_layer) || LayerStyleKinds::isByKind(m_layer);
  {
    const QSignalBlocker quietFill(m_noFillCheck);
    const QSignalBlocker quietStroke(m_noStrokeCheck);
    const QSignalBlocker quietWidth(m_width);
    m_noFillCheck->setChecked(m_noFill && !automatic);  // a kind/period look is always filled and outlined
    m_noStrokeCheck->setChecked(m_noStroke && !automatic);
    m_width->setValue(isPoint() ? m_markerMm : m_widthMm);
  }
  m_note->setText(automatic ? QStringLiteral("지금은 종류·시대별 색입니다. 여기서 색·굵기나 채우기·선을 바꾸면 한 가지 색으로 바뀝니다.")
                            : QString());
  m_note->setVisible(automatic);
  markSwatches();
}

void KaInspectorStyle::readLayerStyle() {
  LayerOps::readSimpleVectorStyle(m_layer, &m_fill, &m_stroke, &m_widthMm, &m_markerMm, &m_noFill, &m_noStroke,
                                  &m_dashed);
}

void KaInspectorStyle::pickColor(const QColor& color) {
  if (!m_layer) return;
  readLayerStyle();
  if (isPoint()) {
    m_fill = color;
    m_noFill = false;  // a colour picked for an empty point fills it
  } else {
    m_stroke = color;  // a box turned on stays on: 「선 없음」 keeps the area without outline
    m_fill = QColor(color.red(), color.green(), color.blue(), kFillAlpha);
  }
  apply();
}

void KaInspectorStyle::setOff(bool fill, bool on) {
  if (!m_layer) return;
  readLayerStyle();
  (fill ? m_noFill : m_noStroke) = on;
  if (on) (fill ? m_noStroke : m_noFill) = false;  // never both: the shape would vanish (a grey hairline instead)
  // Turned off earlier, a stored colour is see-through: bring back a visible one.
  const QColor base = m_stroke.alpha() > 0 ? QColor(m_stroke.rgb())
                      : m_fill.alpha() > 0 ? QColor(m_fill.rgb())
                                           : KaStylePalette::colors().value(1);
  if (!m_noFill && m_fill.alpha() == 0) m_fill = isPoint() ? base : QColor(base.red(), base.green(), base.blue(), kFillAlpha);
  if (!m_noStroke && m_stroke.alpha() == 0) m_stroke = isPoint() ? base.darker(160) : base;  // a point keeps a ring
  apply();
}

void KaInspectorStyle::setWidth(double value) {
  if (!m_layer) return;
  readLayerStyle();
  (isPoint() ? m_markerMm : m_widthMm) = value;
  apply();
}

void KaInspectorStyle::apply() {
  if (!LayerOps::applySimpleVectorStyle(m_layer, m_fill, m_stroke, m_widthMm, m_markerMm, m_noFill, m_noStroke,
                                        m_dashed))
    return;
  refresh();  // one colour now: the automatic look is gone
  emit styleApplied(m_layer.data());
}

void KaInspectorStyle::markSwatches() {
  const QColor current = isPoint() || m_noStroke ? m_fill : m_stroke;
  const KaTheme::Tokens& theme = KaTheme::tokens();
  const QList<QColor> colors = KaStylePalette::colors();
  for (int i = 0; i < m_swatches.size(); ++i) {
    const bool chosen = colors.at(i).rgb() == current.rgb();
    m_swatches.at(i)->setStyleSheet(
        QStringLiteral("QPushButton { background-color: %1; border: %2 solid %3; border-radius: 4px; "
                       "min-height: 0px; padding: 0px; }")
            .arg(colors.at(i).name(), chosen ? QStringLiteral("3px") : QStringLiteral("1px"),
                 (chosen ? theme.ink : theme.border).name()));
  }
}
