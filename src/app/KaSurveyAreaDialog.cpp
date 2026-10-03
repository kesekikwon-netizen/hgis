#include "KaSurveyAreaDialog.h"
#include "KaStylePalette.h"
#include "KaTheme.h"
#include "core/LayerOps.h"

#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <cmath>
#include <qgsproviderregistry.h>
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

QgsVectorLayer* KaSurveyAreaDialog::layerToContinue(QgsProject* project, const QString& surveyPath, QgsMapLayer* current) {
  // Only this survey's own area tables: an outside file named 조사구역 opened for tracing would be
  // written on 저장, which must never touch the user's own data.
  const QString survey = QFileInfo(surveyPath).absoluteFilePath();
  QList<QgsVectorLayer*> own;
  for (QgsVectorLayer* layer : LayerOps::surveyAreaLayers(project)) {
    const QString key = LayerOps::layerKeyOf(layer);
    const QString path = QgsProviderRegistry::instance()
                             ->decodeUri(layer->providerType(), layer->source())
                             .value(QStringLiteral("path"))
                             .toString();
    if ((key == QLatin1String("survey_area") || key.startsWith(QLatin1String("survey_area_"))) && !surveyPath.isEmpty() &&
        QFileInfo(path).absoluteFilePath().compare(survey, Qt::CaseInsensitive) == 0)
      own << layer;
  }
  auto* picked = qobject_cast<QgsVectorLayer*>(current);
  return own.contains(picked) ? picked : own.value(0);  // the area picked in the list first
}

KaSurveyAreaDialog::KaSurveyAreaDialog(QWidget* parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("조사구역 그리기 설정"));
  setModal(true);

  m_paletteColors = KaStylePalette::colors();  // 첫 조사구역은 주황
  m_colorNames = KaStylePalette::names();
  setupUi();
  selectColorIndex(1);
}

void KaSurveyAreaDialog::setupUi() {
  auto* mainLayout = new QVBoxLayout(this);
  mainLayout->setSpacing(12);
  mainLayout->setContentsMargins(16, 16, 16, 16);
  auto* form = new QFormLayout();
  form->setHorizontalSpacing(10);
  form->setVerticalSpacing(10);

  m_nameEdit = new QLineEdit(QStringLiteral("조사구역"), this);
  form->addRow(QStringLiteral("이름"), m_nameEdit);

  auto* colorRow = new QHBoxLayout();
  colorRow->setSpacing(4);
  for (int i = 0; i < m_paletteColors.size(); ++i) {
    auto* btn = new QPushButton(m_colorNames[i], this);
    btn->setObjectName(QStringLiteral("surveyAreaColor"));
    btn->setAutoDefault(false);  // Enter and the main-button paint stay with 「그리기 시작」
    btn->setFixedSize(52, 28);
    btn->setCursor(Qt::PointingHandCursor);
    connect(btn, &QPushButton::clicked, this, [this, i]() { selectColorIndex(i); });
    m_colorButtons.append(btn);
    colorRow->addWidget(btn);
  }
  form->addRow(QStringLiteral("외곽선 색"), colorRow);

  auto* widthRow = new QHBoxLayout();
  widthRow->setSpacing(4);
  const QList<QPair<QString, double>> presets = {{QStringLiteral("보통 1.0"), 1.0}, {QStringLiteral("기본 1.5"), 1.5},
                                                 {QStringLiteral("굵게 2.0"), 2.0}, {QStringLiteral("강조 3.0"), 3.0}};
  for (const auto& p : presets) {
    auto* btn = new QPushButton(p.first, this);
    btn->setFixedHeight(28);
    btn->setAutoDefault(false);
    btn->setStyleSheet(QStringLiteral("QPushButton { min-height: 0px; padding: 2px 8px; }"));
    btn->setCheckable(true);
    btn->setChecked(p.second == 1.5);
    connect(btn, &QPushButton::clicked, this, [this, w = p.second]() { selectPresetWidth(w); });
    m_widthPresetButtons.append(btn);
    widthRow->addWidget(btn);
  }
  m_widthSpin = new QDoubleSpinBox(this);
  m_widthSpin->setRange(0.5, 10.0);
  m_widthSpin->setSingleStep(0.2);
  m_widthSpin->setDecimals(1);
  m_widthSpin->setValue(1.5);
  m_widthSpin->setSuffix(QStringLiteral(" mm"));
  m_widthSpin->setMinimumWidth(96);  // 「1.5 mm」 whole, with the app's spin padding
  connect(m_widthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
    for (auto* b : m_widthPresetButtons) b->setChecked(false);
  });
  widthRow->addWidget(m_widthSpin);
  widthRow->addStretch();
  form->addRow(QStringLiteral("선 굵기"), widthRow);
  mainLayout->addLayout(form);

  auto* btnLayout = new QHBoxLayout();
  btnLayout->addStretch();
  // The main button first, then 「취소」, like the other windows' button rows.
  auto* btnStart = new QPushButton(QStringLiteral("그리기 시작 (Enter)"), this);
  btnStart->setObjectName(QStringLiteral("surveyAreaStart"));
  btnStart->setDefault(true);  // the theme's main-button rule colours it
  btnStart->setMinimumSize(130, 36);
  btnStart->setMaximumHeight(36);
  connect(btnStart, &QPushButton::clicked, this, &QDialog::accept);
  btnLayout->addWidget(btnStart);
  auto* btnCancel = new QPushButton(QStringLiteral("취소"), this);
  btnCancel->setFixedSize(80, 36);
  connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
  btnLayout->addWidget(btnCancel);
  mainLayout->addLayout(btnLayout);
}

void KaSurveyAreaDialog::selectColorIndex(int index) {
  if (index < 0 || index >= m_paletteColors.size()) return;
  m_strokeColor = m_paletteColors[index];
  m_fillColor = QColor(m_strokeColor.red(), m_strokeColor.green(), m_strokeColor.blue(), 50);
  const KaTheme::Tokens& theme = KaTheme::tokens();
  for (int i = 0; i < m_colorButtons.size(); ++i) {
    const QColor& fill = m_paletteColors[i];
    const bool selected = i == index;
    m_colorButtons[i]->setStyleSheet(
        QStringLiteral("QPushButton { background-color: %1; color: %2; font-weight: %3; font-size: 12px; "
                       "border: %4 solid %5; border-radius: 6px; min-height: 0px; padding: 0px; }")
            .arg(fill.name(), readableTextOn(fill).name(), selected ? QStringLiteral("bold") : QStringLiteral("normal"),
                 selected ? QStringLiteral("3px") : QStringLiteral("1px"),
                 (selected ? theme.ink : theme.border).name()));
    m_colorButtons[i]->setText(selected ? QStringLiteral("✔") + m_colorNames[i] : m_colorNames[i]);
  }
}

void KaSurveyAreaDialog::selectPresetWidth(double width) {
  m_widthSpin->blockSignals(true);
  m_widthSpin->setValue(width);
  m_widthSpin->blockSignals(false);
  for (auto* b : m_widthPresetButtons) b->setChecked(false);
  if (auto* senderBtn = qobject_cast<QPushButton*>(sender())) senderBtn->setChecked(true);
}

QString KaSurveyAreaDialog::layerName() const {
  const QString typed = m_nameEdit ? m_nameEdit->text().trimmed() : QString();
  return typed.isEmpty() ? QStringLiteral("조사구역") : typed;
}

double KaSurveyAreaDialog::strokeWidthMm() const {
  return m_widthSpin ? m_widthSpin->value() : 1.5;
}
