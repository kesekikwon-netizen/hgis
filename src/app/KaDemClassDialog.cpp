#include "KaDemClassDialog.h"
#include "core/DemPresentation.h"
#include "core/LayerOps.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <qgscoordinatetransform.h>
#include <qgsmapcanvas.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>

KaDemClassDialog::KaDemClassDialog(QgsRasterLayer* layer, QWidget* parent, QgsMapCanvas* canvas)
    : QDialog(parent), m_layer(layer), m_canvas(canvas) {
  setWindowTitle(QStringLiteral("DEM 표현"));
  setModal(false);
  setMinimumWidth(380);
  auto* form = new QFormLayout(this);
  auto* hint = new QLabel(QStringLiteral("표고에 따라 색이 부드럽게 이어집니다. 유적처럼 높이차가 작은 곳은 "
                                         "「현재 화면에 색 맞추기」로 화면 안의 높이만으로 색을 나누세요."), this);
  hint->setWordWrap(true);
  form->addRow(hint);
  m_preset = new QComboBox(this);
  m_preset->setObjectName(QStringLiteral("demPreset"));
  m_preset->addItem(QStringLiteral("전국 표준 (0–2000m)"), QStringLiteral("national"));
  m_preset->addItem(QStringLiteral("저지대 강조"), QStringLiteral("lowland"));
  m_preset->addItem(QStringLiteral("현재 화면 맞춤"), QStringLiteral("viewport"));
  const int selected = layer ? m_preset->findData(layer->customProperty(QStringLiteral("ka_hgis/dem_preset"), QStringLiteral("national"))) : 0;
  m_preset->setCurrentIndex(qMax(0, selected));
  auto* fit = new QPushButton(QStringLiteral("현재 화면에 색 맞추기"), this);
  fit->setObjectName(QStringLiteral("demFitView"));
  fit->setToolTip(QStringLiteral("지금 지도 화면에 보이는 가장 낮은 곳과 높은 곳으로 색띠를 다시 나눕니다. "
                                 "지도를 옮기면 따라 바뀝니다."));
  fit->setEnabled(canvas != nullptr);
  auto* presetRow = new QHBoxLayout();
  presetRow->addWidget(m_preset, 1);
  presetRow->addWidget(fit);
  form->addRow(QStringLiteral("색 표현"), presetRow);
  m_relief = new QCheckBox(QStringLiteral("지형 음영 합성"), this);
  m_relief->setObjectName(QStringLiteral("demReliefEnabled"));
  m_relief->setChecked(!layer || layer->customProperty(QStringLiteral("ka_hgis/dem_relief_enabled"), true).toBool());
  form->addRow(m_relief);
  m_exaggeration = new QDoubleSpinBox(this);
  m_exaggeration->setObjectName(QStringLiteral("demExaggeration"));
  m_exaggeration->setRange(.1, 5.);
  m_exaggeration->setSingleStep(.1);
  m_exaggeration->setSuffix(QStringLiteral(" 배"));
  m_exaggeration->setValue(layer ? layer->customProperty(QStringLiteral("ka_hgis/dem_z_factor"), 1.).toDouble() : 1.);
  form->addRow(QStringLiteral("수직과장"), m_exaggeration);
  m_strength = new QSpinBox(this);
  m_strength->setObjectName(QStringLiteral("demReliefStrength"));
  m_strength->setRange(0, 80);
  m_strength->setSuffix(QStringLiteral(" %"));
  m_strength->setValue(layer ? qRound(layer->customProperty(QStringLiteral("ka_hgis/dem_relief_strength"), .30).toDouble() * 100.) : 30);
  form->addRow(QStringLiteral("음영 세기"), m_strength);
  m_status = new QLabel(this);
  m_status->setObjectName(QStringLiteral("demStatus"));
  m_status->setWordWrap(true);
  form->addRow(m_status);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Close, this);
  buttons->button(QDialogButtonBox::Apply)->setText(QStringLiteral("적용"));
  buttons->button(QDialogButtonBox::Close)->setText(QStringLiteral("닫기"));
  connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &KaDemClassDialog::applyStyle);
  connect(fit, &QPushButton::clicked, this, &KaDemClassDialog::fitToView);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
  form->addRow(buttons);
  if (layer) connect(layer, &QObject::destroyed, this, &QDialog::reject);
  DemPresentation::followCanvas(layer, canvas);
  suggestViewport();
}

bool KaDemClassDialog::canvasExtentInLayerCrs(QgsRectangle* extent) const {
  if (!m_canvas || !m_layer || !extent) return false;
  try {
    const QgsCoordinateTransform transform(m_canvas->mapSettings().destinationCrs(), m_layer->crs(), m_canvas->mapSettings().transformContext());
    *extent = transform.transformBoundingBox(m_canvas->extent());
  } catch (const QgsCsException&) {
    return false;
  }
  return true;
}

void KaDemClassDialog::fitToView() {
  m_preset->setCurrentIndex(m_preset->findData(QStringLiteral("viewport")));
  applyStyle();
}

// The national 0–2000 m ramp stays the default; a flat site only gets a pointer to the view fit.
void KaDemClassDialog::suggestViewport() {
  if (!m_layer || m_preset->currentData().toString() == QLatin1String("viewport")) return;
  // Opening the dialog must not wait on the network for a hint.
  const QString source = m_layer->source();
  if (source.contains(QLatin1String("vsicurl")) || source.startsWith(QLatin1String("http"), Qt::CaseInsensitive)) return;
  QgsRectangle extent;
  double low = 0., high = 0.;
  if (!canvasExtentInLayerCrs(&extent) || !DemPresentation::sampleRange(m_layer, extent, &low, &high)) return;
  if (high - low >= 100.) return;
  m_status->setText(QStringLiteral("지금 화면의 높이차는 약 %1 m뿐이라 이 색 표현으로는 거의 한 색으로 보입니다. "
                                   "「현재 화면에 색 맞추기」를 누르면 화면 안의 높이로 색을 나눕니다.")
                        .arg(qMax(1, qRound(high - low))));
}

void KaDemClassDialog::applyStyle() {
  if (!m_layer) return;
  const QString preset = m_preset->currentData().toString();
  QgsRectangle extent;
  if (preset == QLatin1String("viewport")) {
    if (!m_canvas) { m_status->setText(QStringLiteral("지도 화면에서 DEM 표현을 다시 열어 주세요.")); return; }
    if (!canvasExtentInLayerCrs(&extent)) {
      m_status->setText(QStringLiteral("화면 좌표를 변환하지 못했습니다. 작업 좌표계를 확인하세요.")); return;
    }
  }
  if (!DemPresentation::apply(m_layer, preset, extent)) {
    m_status->setText(QStringLiteral("이 화면에서 표고값을 읽지 못했습니다. DEM이 있는 곳으로 이동한 뒤 적용하세요.")); return;
  }
  m_layer->setCustomProperty(QStringLiteral("ka_hgis/dem_relief_enabled"), m_relief->isChecked());
  m_layer->setCustomProperty(QStringLiteral("ka_hgis/dem_z_factor"), m_exaggeration->value());
  m_layer->setCustomProperty(QStringLiteral("ka_hgis/dem_relief_strength"), m_strength->value() / 100.);
  auto* project = QgsProject::instance();
  auto* shade = LayerOps::ensureDemRelief(project, m_layer);
  if (m_canvas) LayerOps::syncMapCanvas(project, m_canvas, false);
  project->setDirty(true);
  m_status->setText(m_relief->isChecked() && !shade
      ? m_layer->customProperty(QStringLiteral("ka_hgis/dem_relief_error")).toString()
      : QStringLiteral("적용했습니다. 색띠로 표고를 확인하세요. 음영은 지형 방향에 따라 밝기를 바꿉니다."));
}
