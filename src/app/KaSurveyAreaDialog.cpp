#include "KaSurveyAreaDialog.h"
#include "KaTheme.h"
#include "core/LayerOps.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>
#include <cmath>
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgsgeometry.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

namespace {

double relativeLuminance(const QColor& color) {
  auto channel = [](double value) {
    return value <= 0.03928 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
  };
  return 0.2126 * channel(color.redF()) + 0.7152 * channel(color.greenF()) + 0.0722 * channel(color.blueF());
}

double contrastRatio(const QColor& a, const QColor& b) {
  const double la = relativeLuminance(a) + 0.05;
  const double lb = relativeLuminance(b) + 0.05;
  return la > lb ? la / lb : lb / la;
}

}  // namespace

QColor KaSurveyAreaDialog::readableTextOn(const QColor& fill) {
  for (const QColor& text : {QColor(Qt::white), KaTheme::tokens().ink})
    if (contrastRatio(fill, text) >= 4.5) return text;
  return QColor(Qt::black);
}

QString KaSurveyAreaDialog::areaText(QgsVectorLayer* layer) {
  if (!layer || !layer->isValid() || layer->crs().mapUnits() != Qgis::DistanceUnit::Meters) return {};
  double squareMetres = 0;
  QgsFeature feature;
  QgsFeatureIterator it = layer->getFeatures(QgsFeatureRequest().setNoAttributes());
  while (it.nextFeature(feature))
    if (feature.hasGeometry()) squareMetres += feature.geometry().area();
  if (!(squareMetres > 0)) return {};
  const QLocale locale(QLocale::Korean, QLocale::SouthKorea);
  // 1 평 = 400/121 ㎡.
  return QStringLiteral("%1 ㎡ (%2 평)")
      .arg(locale.toString(qRound64(squareMetres)), locale.toString(qRound64(squareMetres * 121.0 / 400.0)));
}

KaSurveyAreaDialog::KaSurveyAreaDialog(QWidget* parent, QgsProject* project, const QString& gpkgPath)
    : QDialog(parent), m_project(project) {
  Q_UNUSED(gpkgPath);
  setWindowTitle(QStringLiteral("조사구역 그리기 설정"));
  setMinimumWidth(460);
  setModal(true);
  if (m_project) m_existingLayers = LayerOps::surveyAreaLayers(m_project);

  // 여덟 가지 표준 색 (빨, 주, 노, 초, 파, 남, 보, 갈)
  m_paletteColors = {QColor(0xDC, 0x26, 0x26), QColor(0xEA, 0x58, 0x0C), QColor(0xCA, 0x8A, 0x04),
                     QColor(0x16, 0xA3, 0x4A), QColor(0x25, 0x63, 0xEB), QColor(0x1E, 0x3A, 0x8A),
                     QColor(0x7C, 0x3A, 0xED), QColor(0x92, 0x40, 0x0E)};
  m_colorNames = {QStringLiteral("빨강"), QStringLiteral("주황"), QStringLiteral("노랑"), QStringLiteral("초록"),
                  QStringLiteral("파랑"), QStringLiteral("남색"), QStringLiteral("보라"), QStringLiteral("갈색")};

  // 기존 레이어 개수에 따라 기본 추천 색상 선택 (첫 번째: 주황, 두 번째: 파랑, 세 번째: 빨강 ...)
  int defaultColorIdx = 1;
  if (m_existingLayers.size() == 1) defaultColorIdx = 4;
  else if (m_existingLayers.size() == 2) defaultColorIdx = 0;
  else if (m_existingLayers.size() == 3) defaultColorIdx = 3;
  else if (m_existingLayers.size() >= 4) defaultColorIdx = (m_existingLayers.size() + 1) % m_paletteColors.size();
  setupUi();
  selectColorIndex(defaultColorIdx);  // sets stroke, fill and the preview
}

void KaSurveyAreaDialog::setupUi() {
  const KaTheme::Tokens& theme = KaTheme::tokens();
  auto* mainLayout = new QVBoxLayout(this);
  mainLayout->setSpacing(14);
  mainLayout->setContentsMargins(18, 18, 18, 18);

  auto* titleLabel = new QLabel(QStringLiteral("<b>조사구역 레이어 및 스타일 지정</b>"), this);
  titleLabel->setStyleSheet(QStringLiteral("font-size: 14px; color: %1;").arg(theme.ink.name()));
  mainLayout->addWidget(titleLabel);

  // One survey-area layer holds every area polygon, as in any GIS; a new layer is only
  // for an area that must be switched on and off on its own.
  auto* descLabel = new QLabel(
      m_existingLayers.isEmpty()
          ? QStringLiteral("조사구역 레이어를 만들고 바로 그리기를 시작합니다.")
          : QStringLiteral("이미 그린 조사구역에 이어서 그립니다. 구역을 따로 켜고 꺼야 할 때만 새 레이어를 만드세요."),
      this);
  descLabel->setStyleSheet(QStringLiteral("font-size: 11px; color: %1;").arg(theme.inkMuted.name()));
  descLabel->setWordWrap(true);
  mainLayout->addWidget(descLabel);

  auto* layerGroup = new QGroupBox(QStringLiteral("구역(레이어) 선택"), this);
  auto* layerLayout = new QVBoxLayout(layerGroup);
  layerLayout->setSpacing(8);
  // 첫 레이어는 번호 없이 「조사구역」, 따로 나누는 레이어부터 「조사구역 2」, 「조사구역 3」...
  const QString suggestedName = m_existingLayers.isEmpty()
                                    ? QStringLiteral("조사구역")
                                    : QStringLiteral("조사구역 %1").arg(m_existingLayers.size() + 1);
  m_radioNew = new QRadioButton(QStringLiteral("새 조사구역 레이어 만들기:"), layerGroup);
  m_radioNew->setChecked(true);
  m_nameEdit = new QLineEdit(suggestedName, layerGroup);
  m_nameEdit->setStyleSheet(QStringLiteral("padding: 6px; font-size: 12px; font-weight: bold; color: %1;")
                                .arg(theme.ink.name()));
  auto* newRowLayout = new QHBoxLayout();
  newRowLayout->addWidget(m_radioNew);
  newRowLayout->addWidget(m_nameEdit, 1);
  layerLayout->addLayout(newRowLayout);

  if (!m_existingLayers.isEmpty()) {
    m_radioExisting = new QRadioButton(QStringLiteral("기존 구역에 이어서 그리기:"), layerGroup);
    m_existingCombo = new QComboBox(layerGroup);
    m_existingCombo->setObjectName(QStringLiteral("surveyAreaExisting"));
    for (auto* vl : m_existingLayers) {
      // The layer id, not a pointer: the combo must never hand back a deleted layer.
      const QString area = areaText(vl);
      m_existingCombo->addItem(area.isEmpty()
                                   ? QStringLiteral("%1 (%2개 도형)").arg(vl->name()).arg(vl->featureCount())
                                   : QStringLiteral("%1 (%2개 도형, %3)").arg(vl->name()).arg(vl->featureCount()).arg(area),
                               vl->id());
    }
    m_existingCombo->setEnabled(false);
    auto* existRowLayout = new QHBoxLayout();
    existRowLayout->addWidget(m_radioExisting);
    existRowLayout->addWidget(m_existingCombo, 1);
    layerLayout->addLayout(existRowLayout);
    connect(m_radioNew, &QRadioButton::toggled, this, &KaSurveyAreaDialog::onModeChanged);
    connect(m_radioExisting, &QRadioButton::toggled, this, &KaSurveyAreaDialog::onModeChanged);
    // Drawing again continues the area already there; a separate layer is a deliberate choice.
    m_radioExisting->setChecked(true);
  }
  mainLayout->addWidget(layerGroup);

  auto* colorGroup = new QGroupBox(QStringLiteral("구역 외곽선 색상"), this);
  m_colorGroup = colorGroup;
  auto* colorLayout = new QGridLayout(colorGroup);
  colorLayout->setSpacing(8);
  for (int i = 0; i < m_paletteColors.size(); ++i) {
    auto* btn = new QPushButton(m_colorNames[i], colorGroup);
    btn->setObjectName(QStringLiteral("surveyAreaColor"));
    btn->setFixedHeight(34);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setProperty("colorIndex", i);
    connect(btn, &QPushButton::clicked, this, [this, i]() { onColorButtonClicked(i); });
    m_colorButtons.append(btn);
    colorLayout->addWidget(btn, i / 4, i % 4);
  }
  mainLayout->addWidget(colorGroup);

  auto* widthGroup = new QGroupBox(QStringLiteral("선 굵기 (외곽선 두께)"), this);
  m_widthGroup = widthGroup;
  auto* widthLayout = new QHBoxLayout(widthGroup);
  widthLayout->setSpacing(8);
  const QList<QPair<QString, double>> presets = {{QStringLiteral("보통 (1.0mm)"), 1.0}, {QStringLiteral("기본 (1.5mm)"), 1.5},
                                                 {QStringLiteral("굵게 (2.0mm)"), 2.0}, {QStringLiteral("강조 (3.0mm)"), 3.0}};
  for (const auto& p : presets) {
    auto* btn = new QPushButton(p.first, widthGroup);
    btn->setFixedHeight(30);
    btn->setCheckable(true);
    btn->setChecked(p.second == 1.5);
    connect(btn, &QPushButton::clicked, this, [this, w = p.second]() { onPresetWidthClicked(w); });
    m_widthPresetButtons.append(btn);
    widthLayout->addWidget(btn);
  }
  widthLayout->addSpacing(8);
  widthLayout->addWidget(new QLabel(QStringLiteral("직접입력:"), widthGroup));
  m_widthSpin = new QDoubleSpinBox(widthGroup);
  m_widthSpin->setRange(0.5, 10.0);
  m_widthSpin->setSingleStep(0.2);
  m_widthSpin->setValue(1.5);
  m_widthSpin->setSuffix(QStringLiteral(" mm"));
  m_widthSpin->setFixedWidth(80);
  connect(m_widthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
    for (auto* b : m_widthPresetButtons) b->setChecked(false);
    updatePreview();
  });
  widthLayout->addWidget(m_widthSpin);
  mainLayout->addWidget(widthGroup);

  auto* previewGroup = new QGroupBox(QStringLiteral("미리보기"), this);
  auto* prevLayout = new QVBoxLayout(previewGroup);
  prevLayout->setContentsMargins(12, 12, 12, 12);
  m_previewBox = new QFrame(previewGroup);
  m_previewBox->setFixedHeight(46);
  auto* pboxLayout = new QHBoxLayout(m_previewBox);
  m_previewLabel = new QLabel(m_previewBox);
  m_previewLabel->setAlignment(Qt::AlignCenter);
  pboxLayout->addWidget(m_previewLabel);
  prevLayout->addWidget(m_previewBox);
  mainLayout->addWidget(previewGroup);

  auto* btnLayout = new QHBoxLayout();
  btnLayout->addStretch();
  auto* btnCancel = new QPushButton(QStringLiteral("취소"), this);
  btnCancel->setFixedSize(80, 36);
  connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
  btnLayout->addWidget(btnCancel);
  auto* btnStart = new QPushButton(QStringLiteral("그리기 시작 (Enter)"), this);
  btnStart->setObjectName(QStringLiteral("surveyAreaStart"));
  btnStart->setDefault(true);
  btnStart->setMinimumSize(130, 36);
  btnStart->setMaximumHeight(36);
  btnStart->setStyleSheet(QStringLiteral(
      "QPushButton { background-color: %1; color: %2; font-weight: bold; font-size: 13px; border-radius: 4px; }"
      "QPushButton:hover { background-color: %3; }")
                              .arg(theme.sky1.name(), theme.railText.name(), theme.sky2.name()));
  connect(btnStart, &QPushButton::clicked, this, &QDialog::accept);
  btnLayout->addWidget(btnStart);
  mainLayout->addLayout(btnLayout);
  onModeChanged();
}

void KaSurveyAreaDialog::onModeChanged() {
  const bool isNew = m_radioNew->isChecked();
  m_nameEdit->setEnabled(isNew);
  if (m_existingCombo) m_existingCombo->setEnabled(!isNew);
  if (m_colorGroup) m_colorGroup->setEnabled(isNew);
  if (m_widthGroup) m_widthGroup->setEnabled(isNew);
}

void KaSurveyAreaDialog::onColorButtonClicked(int index) {
  selectColorIndex(index);
}

void KaSurveyAreaDialog::selectColorIndex(int index) {
  if (index < 0 || index >= m_paletteColors.size()) return;
  m_selectedColorIndex = index;
  m_strokeColor = m_paletteColors[index];
  m_fillColor = QColor(m_strokeColor.red(), m_strokeColor.green(), m_strokeColor.blue(), 50);
  const KaTheme::Tokens& theme = KaTheme::tokens();
  for (int i = 0; i < m_colorButtons.size(); ++i) {
    const QColor& fill = m_paletteColors[i];
    const bool selected = i == index;
    m_colorButtons[i]->setStyleSheet(
        QStringLiteral("QPushButton { background-color: %1; color: %2; font-weight: %3; "
                       "border: %4 solid %5; border-radius: 6px; }")
            .arg(fill.name(), readableTextOn(fill).name(), selected ? QStringLiteral("bold") : QStringLiteral("normal"),
                 selected ? QStringLiteral("3px") : QStringLiteral("1px"),
                 (selected ? theme.ink : theme.border).name()));
    m_colorButtons[i]->setText(selected ? QStringLiteral("✔ ") + m_colorNames[i] : m_colorNames[i]);
  }
  updatePreview();
}

void KaSurveyAreaDialog::onPresetWidthClicked(double width) {
  m_widthSpin->blockSignals(true);
  m_widthSpin->setValue(width);
  m_widthSpin->blockSignals(false);
  for (auto* b : m_widthPresetButtons) b->setChecked(false);
  if (auto* senderBtn = qobject_cast<QPushButton*>(sender())) senderBtn->setChecked(true);
  updatePreview();
}

void KaSurveyAreaDialog::updatePreview() {
  if (!m_previewBox || !m_previewLabel) return;
  const double w = strokeWidthMm();
  const int borderPx = qMax(1, qMin(6, static_cast<int>(w * 1.5)));
  m_previewBox->setStyleSheet(
      QStringLiteral("QFrame { border: %1px solid %2; background-color: rgba(%3, %4, %5, 0.20); border-radius: 6px; }")
          .arg(QString::number(borderPx), m_strokeColor.name(), QString::number(m_strokeColor.red()),
               QString::number(m_strokeColor.green()), QString::number(m_strokeColor.blue())));
  // The colour shows in the frame; the words stay in theme ink so every colour reads.
  m_previewLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 12px; color: %1; border: none; background: transparent;")
                                    .arg(KaTheme::tokens().ink.name()));
  m_previewLabel->setText(QStringLiteral("%1 외곽선 %2 mm, 반투명 채움")
                              .arg(m_colorNames.value(m_selectedColorIndex), QString::number(w, 'f', 1)));
}

bool KaSurveyAreaDialog::isNewLayer() const {
  return m_radioNew ? m_radioNew->isChecked() : true;
}

QString KaSurveyAreaDialog::layerName() const {
  const QString typed = m_nameEdit ? m_nameEdit->text().trimmed() : QString();
  return typed.isEmpty() ? QStringLiteral("조사구역") : typed;
}

QgsVectorLayer* KaSurveyAreaDialog::selectedExistingLayer() const {
  if (!m_project || !m_existingCombo || !m_existingCombo->isEnabled()) return nullptr;
  return qobject_cast<QgsVectorLayer*>(m_project->mapLayer(m_existingCombo->currentData().toString()));
}

double KaSurveyAreaDialog::strokeWidthMm() const {
  return m_widthSpin ? m_widthSpin->value() : 1.5;
}
