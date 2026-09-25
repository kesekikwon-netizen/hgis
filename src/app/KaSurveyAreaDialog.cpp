#include "KaSurveyAreaDialog.h"
#include "core/LayerOps.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

KaSurveyAreaDialog::KaSurveyAreaDialog(QWidget* parent, QgsProject* project, const QString& gpkgPath)
    : QDialog(parent), m_project(project), m_gpkgPath(gpkgPath) {
  setWindowTitle(QStringLiteral("조사구역 그리기 설정"));
  setMinimumWidth(460);
  setModal(true);

  if (m_project) {
    m_existingLayers = LayerOps::surveyAreaLayers(m_project);
  }

  // 직관적인 8가지 표준 색상 팔레트 (빨, 주, 노, 초, 파, 남, 보, 갈)
  m_paletteColors = {
      QColor(QStringLiteral("#DC2626")), // 빨강
      QColor(QStringLiteral("#EA580C")), // 주황
      QColor(QStringLiteral("#CA8A04")), // 노랑
      QColor(QStringLiteral("#16A34A")), // 초록
      QColor(QStringLiteral("#2563EB")), // 파랑
      QColor(QStringLiteral("#1E3A8A")), // 남색
      QColor(QStringLiteral("#7C3AED")), // 보라
      QColor(QStringLiteral("#92400E")), // 갈색
  };

  // 기존 레이어 개수에 따라 기본 추천 색상 선택 (첫 번째: 주황, 두 번째: 파랑, 세 번째: 빨강 ...)
  int defaultColorIdx = 1; // 주황
  if (m_existingLayers.size() == 1) defaultColorIdx = 4; // 파랑
  else if (m_existingLayers.size() == 2) defaultColorIdx = 0; // 빨강
  else if (m_existingLayers.size() == 3) defaultColorIdx = 3; // 초록
  else if (m_existingLayers.size() >= 4) defaultColorIdx = (m_existingLayers.size() + 1) % m_paletteColors.size();

  m_selectedColorIndex = defaultColorIdx;
  m_strokeColor = m_paletteColors[m_selectedColorIndex];
  m_fillColor = QColor(m_strokeColor.red(), m_strokeColor.green(), m_strokeColor.blue(), 50);

  setupUi();
  selectColorIndex(m_selectedColorIndex);
  updatePreview();
}

void KaSurveyAreaDialog::setupUi() {
  auto* mainLayout = new QVBoxLayout(this);
  mainLayout->setSpacing(14);
  mainLayout->setContentsMargins(18, 18, 18, 18);

  // 1. 안내 헤더
  auto* titleLabel = new QLabel(QStringLiteral("<b>조사구역 레이어 및 스타일 지정</b>"), this);
  titleLabel->setStyleSheet(QStringLiteral("font-size: 14px; color: #1E293B;"));
  mainLayout->addWidget(titleLabel);

  // One survey-area layer holds every area polygon, as in any GIS; a new layer is only
  // for an area that must be switched on and off on its own.
  auto* descLabel = new QLabel(
      m_existingLayers.isEmpty()
          ? QStringLiteral("조사구역 레이어를 만들고 바로 그리기를 시작합니다.")
          : QStringLiteral("이미 그린 조사구역에 이어서 그립니다. 구역을 따로 켜고 꺼야 할 때만 새 레이어를 만드세요."),
      this);
  descLabel->setStyleSheet(QStringLiteral("font-size: 11px; color: #64748B;"));
  descLabel->setWordWrap(true);
  mainLayout->addWidget(descLabel);

  // 2. 레이어 선택 그룹
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
  m_nameEdit->setStyleSheet(QStringLiteral("padding: 6px; font-size: 12px; font-weight: bold;"));

  auto* newRowLayout = new QHBoxLayout();
  newRowLayout->addWidget(m_radioNew);
  newRowLayout->addWidget(m_nameEdit, 1);
  layerLayout->addLayout(newRowLayout);

  if (!m_existingLayers.isEmpty()) {
    m_radioExisting = new QRadioButton(QStringLiteral("기존 구역에 이어서 그리기:"), layerGroup);
    m_existingCombo = new QComboBox(layerGroup);
    for (auto* vl : m_existingLayers) {
      m_existingCombo->addItem(QStringLiteral("%1 (%2개 도형)").arg(vl->name()).arg(vl->featureCount()),
                               QVariant::fromValue(static_cast<void*>(vl)));
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

  // 3. 직관적인 색상 팔레트 그룹 (빨, 주, 노, 초, 파, 남, 보, 갈)
  auto* colorGroup = new QGroupBox(QStringLiteral("구역 외곽선 색상"), this);
  m_colorGroup = colorGroup;
  auto* colorLayout = new QGridLayout(colorGroup);
  colorLayout->setSpacing(8);

  const QStringList colorNames = {
      QStringLiteral("빨강"), QStringLiteral("주황"), QStringLiteral("노랑"), QStringLiteral("초록"),
      QStringLiteral("파랑"), QStringLiteral("남색"), QStringLiteral("보라"), QStringLiteral("갈색")
  };

  for (int i = 0; i < m_paletteColors.size(); ++i) {
    const QColor& c = m_paletteColors[i];
    auto* btn = new QPushButton(colorNames[i], colorGroup);
    btn->setFixedHeight(34);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setProperty("colorIndex", i);
    connect(btn, &QPushButton::clicked, this, [this, i]() { onColorButtonClicked(i); });
    m_colorButtons.append(btn);

    int row = i / 4;
    int col = i % 4;
    colorLayout->addWidget(btn, row, col);
  }
  mainLayout->addWidget(colorGroup);

  // 4. 선 굵기(외곽선) 그룹
  auto* widthGroup = new QGroupBox(QStringLiteral("선 굵기 (외곽선 두께)"), this);
  m_widthGroup = widthGroup;
  auto* widthLayout = new QHBoxLayout(widthGroup);
  widthLayout->setSpacing(8);

  const struct PresetWidth { QString label; double width; } presets[] = {
      {QStringLiteral("보통 (1.0mm)"), 1.0},
      {QStringLiteral("기본 (1.5mm)"), 1.5},
      {QStringLiteral("굵게 (2.0mm)"), 2.0},
      {QStringLiteral("강조 (3.0mm)"), 3.0},
  };

  for (const auto& p : presets) {
    auto* btn = new QPushButton(p.label, widthGroup);
    btn->setFixedHeight(30);
    btn->setCheckable(true);
    btn->setChecked(p.width == 1.5);
    connect(btn, &QPushButton::clicked, this, [this, w = p.width]() { onPresetWidthClicked(w); });
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
  connect(m_widthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val) {
    for (auto* b : m_widthPresetButtons) {
      b->setChecked(false);
    }
    updatePreview();
  });
  widthLayout->addWidget(m_widthSpin);
  mainLayout->addWidget(widthGroup);

  // 5. 실시간 미리보기 박스
  auto* previewGroup = new QGroupBox(QStringLiteral("미리보기"), this);
  auto* prevLayout = new QVBoxLayout(previewGroup);
  prevLayout->setContentsMargins(12, 12, 12, 12);

  m_previewBox = new QFrame(previewGroup);
  m_previewBox->setFixedHeight(46);
  auto* pboxLayout = new QHBoxLayout(m_previewBox);
  m_previewLabel = new QLabel(m_previewBox);
  m_previewLabel->setAlignment(Qt::AlignCenter);
  m_previewLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 12px;"));
  pboxLayout->addWidget(m_previewLabel);

  prevLayout->addWidget(m_previewBox);
  mainLayout->addWidget(previewGroup);

  // 6. 하단 확인/취소 버튼
  auto* btnLayout = new QHBoxLayout();
  btnLayout->addStretch();

  auto* btnCancel = new QPushButton(QStringLiteral("취소"), this);
  btnCancel->setFixedWidth(80);
  btnCancel->setFixedHeight(36);
  connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
  btnLayout->addWidget(btnCancel);

  auto* btnStart = new QPushButton(QStringLiteral("그리기 시작 (Enter)"), this);
  btnStart->setDefault(true);
  btnStart->setMinimumWidth(130);
  btnStart->setFixedHeight(36);
  btnStart->setStyleSheet(QStringLiteral(
      "background-color: #0284C7; color: #FFFFFF; font-weight: bold; font-size: 13px; border-radius: 4px;"));
  connect(btnStart, &QPushButton::clicked, this, &QDialog::accept);
  btnLayout->addWidget(btnStart);

  mainLayout->addLayout(btnLayout);
  onModeChanged();
}

void KaSurveyAreaDialog::onModeChanged() {
  const bool isNew = m_radioNew->isChecked();
  m_nameEdit->setEnabled(isNew);
  if (m_existingCombo) {
    m_existingCombo->setEnabled(!isNew);
  }
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

  for (int i = 0; i < m_colorButtons.size(); ++i) {
    QPushButton* b = m_colorButtons[i];
    const QColor& c = m_paletteColors[i];
    if (i == index) {
      b->setStyleSheet(QStringLiteral(
          "QPushButton { background-color: %1; color: #FFFFFF; font-weight: bold; "
          "border: 3px solid #000000; border-radius: 6px; }")
                           .arg(c.name()));
      b->setText(QStringLiteral("✔ %1").arg(b->text().remove(QStringLiteral("✔ ")).trimmed()));
    } else {
      b->setStyleSheet(QStringLiteral(
          "QPushButton { background-color: %1; color: #FFFFFF; font-weight: normal; "
          "border: 1px solid #CBD5E1; border-radius: 6px; }")
                           .arg(c.name()));
      b->setText(b->text().remove(QStringLiteral("✔ ")).trimmed());
    }
  }
  updatePreview();
}

void KaSurveyAreaDialog::onPresetWidthClicked(double width) {
  m_widthSpin->blockSignals(true);
  m_widthSpin->setValue(width);
  m_widthSpin->blockSignals(false);

  for (auto* b : m_widthPresetButtons) {
    b->setChecked(false);
  }
  auto* senderBtn = qobject_cast<QPushButton*>(sender());
  if (senderBtn) senderBtn->setChecked(true);

  updatePreview();
}

void KaSurveyAreaDialog::updatePreview() {
  if (!m_previewBox || !m_previewLabel) return;
  const double w = strokeWidthMm();
  int borderPx = qMax(1, qMin(6, static_cast<int>(w * 1.5)));

  m_previewBox->setStyleSheet(QStringLiteral(
      "QFrame { border: %1px solid %2; background-color: rgba(%3, %4, %5, 0.20); border-radius: 6px; }")
                                  .arg(borderPx)
                                  .arg(m_strokeColor.name())
                                  .arg(m_strokeColor.red())
                                  .arg(m_strokeColor.green())
                                  .arg(m_strokeColor.blue()));

  m_previewLabel->setStyleSheet(QStringLiteral("color: %1;").arg(m_strokeColor.name()));
  m_previewLabel->setText(
      QStringLiteral("조사구역 폴리곤 외곽선 (%1, %2mm) — 반투명 20% 채움")
          .arg(m_strokeColor.name())
          .arg(QString::number(w, 'f', 1)));
}

bool KaSurveyAreaDialog::isNewLayer() const {
  return m_radioNew ? m_radioNew->isChecked() : true;
}

QString KaSurveyAreaDialog::layerName() const {
  if (m_nameEdit) {
    QString txt = m_nameEdit->text().trimmed();
    if (!txt.isEmpty()) return txt;
  }
  return QStringLiteral("조사구역");
}

QgsVectorLayer* KaSurveyAreaDialog::selectedExistingLayer() const {
  if (m_existingCombo && m_existingCombo->isEnabled()) {
    void* ptr = m_existingCombo->currentData().value<void*>();
    return static_cast<QgsVectorLayer*>(ptr);
  }
  return nullptr;
}

double KaSurveyAreaDialog::strokeWidthMm() const {
  return m_widthSpin ? m_widthSpin->value() : 1.5;
}
