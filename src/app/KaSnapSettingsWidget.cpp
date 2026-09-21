#include "KaSnapSettingsWidget.h"
#include "core/LayerOps.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <qgsproject.h>

KaSnapSettingsWidget::KaSnapSettingsWidget(QWidget* parent) : QWidget(parent) {
  setObjectName(QStringLiteral("kaSnapSettings"));
  auto* row = new QHBoxLayout(this);
  row->setContentsMargins(4, 0, 4, 0);
  row->setSpacing(6);
  m_on = new QCheckBox(QStringLiteral("자석"), this);
  m_on->setToolTip(QStringLiteral(
      "켜면 조사구역·유구·지적 선에 붙습니다. 위성 그림에는 붙지 않습니다"));
  m_tolerance = new QDoubleSpinBox(this);
  m_tolerance->setRange(4.0, 64.0);
  m_tolerance->setDecimals(0);
  m_tolerance->setSuffix(QStringLiteral(" px"));
  m_tolerance->setToolTip(QStringLiteral("자석이 붙는 화면 거리"));
  m_target = new QComboBox(this);
  m_target->addItem(QStringLiteral("모든 조사 레이어"), QStringLiteral("survey"));
  m_target->addItem(QStringLiteral("현재 레이어"), QStringLiteral("current"));
  m_target->setToolTip(QStringLiteral("조사 도형과 지적 선에 붙습니다. 위성 그림은 제외"));
  m_topo = new QCheckBox(QStringLiteral("공유 경계"), this);
  m_topo->setToolTip(QStringLiteral(
      "켜면 맞닿은 유구의 같은 꼭짓점을 같이 옮깁니다. QgsProject 위상 편집"));
  row->addWidget(m_on);
  row->addWidget(m_topo);
  row->addWidget(new QLabel(QStringLiteral("허용치"), this));
  row->addWidget(m_tolerance);
  row->addWidget(m_target);
  connect(m_on, &QCheckBox::toggled, this, [this]() { writeToProject(); });
  connect(m_topo, &QCheckBox::toggled, this, [this]() { writeToProject(); });
  connect(m_tolerance, &QDoubleSpinBox::valueChanged, this, [this]() { writeToProject(); });
  connect(m_target, &QComboBox::currentIndexChanged, this, [this]() { writeToProject(); });
  syncFromProject();
}

void KaSnapSettingsWidget::syncFromProject() {
  m_syncing = true;
  const auto settings = LayerOps::readSnapSettings(QgsProject::instance());
  m_on->setChecked(settings.enabled);
  m_topo->setChecked(settings.topological);
  m_tolerance->setValue(settings.tolerancePx);
  const int idx = m_target->findData(settings.target == LayerOps::SnapTarget::CurrentLayer
                                         ? QStringLiteral("current")
                                         : QStringLiteral("survey"));
  if (idx >= 0) m_target->setCurrentIndex(idx);
  m_syncing = false;
}

void KaSnapSettingsWidget::writeToProject() {
  if (m_syncing) return;
  LayerOps::SnapSettings settings;
  settings.enabled = m_on->isChecked();
  settings.topological = m_topo->isChecked();
  settings.tolerancePx = m_tolerance->value();
  settings.target = m_target->currentData().toString() == QLatin1String("current")
                        ? LayerOps::SnapTarget::CurrentLayer
                        : LayerOps::SnapTarget::SurveyLayers;
  LayerOps::applySnapSettings(QgsProject::instance(), settings);
  emit settingsChanged();
}
