#include "app/KaStatusBar.h"

#include "app/KaChip.h"

#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QToolButton>

namespace {

QFrame* makeSeparator(QWidget* parent) {
  auto* sep = new QFrame(parent);
  sep->setObjectName(QStringLiteral("statusSep"));
  sep->setFrameShape(QFrame::VLine);
  sep->setFixedWidth(1);
  return sep;
}

QString shortCrs(const QString& authId) {
  QString s = authId.trimmed();
  if (s.startsWith(QLatin1String("EPSG:"), Qt::CaseInsensitive))
    s = s.mid(5);
  return s;
}

}  // namespace

KaStatusBar::KaStatusBar(QWidget* parent) : QStatusBar(parent) {
  setObjectName(QStringLiteral("kaStatusBar"));
  setSizeGripEnabled(false);

  // Strata chrome: snap state and unsaved count as chips, left of the readout. The tool
  // chip the window inserts at index 0 lands before them.
  m_snapChip = new KaChip(QStringLiteral("자석 끔"), KaChip::Tone::Neutral, this);
  m_snapChip->setObjectName(QStringLiteral("snapChip"));
  m_snapChip->setGlyph(QStringLiteral("snap"));
  m_snapChip->setToolTip(QStringLiteral("그리기 도구 줄의 자석 설정에서 바꿉니다."));
  addPermanentWidget(m_snapChip);
  m_unsavedChip = new KaChip(QString(), KaChip::Tone::Warn, this);
  m_unsavedChip->setObjectName(QStringLiteral("unsavedChip"));
  m_unsavedChip->setToolTip(QStringLiteral("아직 파일에 쓰지 않은 편집입니다. 「저장」(Ctrl+S)으로 씁니다."));
  addPermanentWidget(m_unsavedChip);

  m_xy = new QLabel(this);
  m_xy->setObjectName(QStringLiteral("xyReadout"));
  m_xy->setMinimumWidth(240);
  m_xy->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  m_xy->setToolTip(QStringLiteral("지도 위 커서의 작업 좌표입니다."));
  clearCoordinate();
  addPermanentWidget(m_xy);

  QFrame* afterXy = makeSeparator(this);
  addPermanentWidget(afterXy);

  // 입력칸이 「1:1000」으로 보이므로 라벨에는 「1:」을 다시 적지 않는다.
  auto* scaleLabel = new QLabel(QStringLiteral("축척"), this);
  scaleLabel->setObjectName(QStringLiteral("scaleLabel"));
  addPermanentWidget(scaleLabel);

  // 축척 입력칸은 하나다. 예전에는 자유 입력 QLineEdit 과 프리셋 QComboBox 두 개가
  // 나란히 있어서, 같은 축척인데도 어느 쪽으로 넣었느냐에 따라 화면이 달라 보였다.
  // 편집 가능한 콤보 하나로 합쳐 입력과 프리셋이 같은 경로를 타게 한다.
  m_scaleCombo = new QComboBox(this);
  m_scaleCombo->setObjectName(QStringLiteral("scaleCombo"));
  m_scaleCombo->setEditable(true);
  m_scaleCombo->setInsertPolicy(QComboBox::NoInsert);
  m_scaleCombo->setMinimumWidth(120);
  m_scaleCombo->setToolTip(
      QStringLiteral("축척을 고르거나 직접 입력하고 Enter를 누르세요. 1000 · 1:1000 둘 다 됩니다."));
  // 발굴·시굴 도면은 1:100~1:500 을 쓴다. 예전 목록은 1:500 이 최소여서 그 아래는
  // 프리셋으로 갈 수 없었다.
  const QList<int> presets = {100,  200,   250,   500,    1000,   2000,  5000,
                              10000, 25000, 50000, 100000, 250000, 500000};
  for (int s : presets)
    m_scaleCombo->addItem(QStringLiteral("1:%1").arg(s), s);
  const int defaultIdx = m_scaleCombo->findData(25000);
  m_scaleCombo->setCurrentIndex(defaultIdx >= 0 ? defaultIdx : 0);
  m_scaleEdit = m_scaleCombo->lineEdit();
  if (m_scaleEdit) {
    // 스타일시트(#scaleEdit)와 기존 호출부가 그대로 붙도록 이름을 물려준다.
    m_scaleEdit->setObjectName(QStringLiteral("scaleEdit"));
    m_scaleEdit->setPlaceholderText(QStringLiteral("예: 1000"));
    m_scaleEdit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  }
  addPermanentWidget(m_scaleCombo);

  QFrame* afterScale = makeSeparator(this);
  addPermanentWidget(afterScale);

  m_crsButton = new QToolButton(this);
  m_crsButton->setObjectName(QStringLiteral("crsButton"));
  m_crsButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
  m_crsButton->setCursor(Qt::PointingHandCursor);
  connect(m_crsButton, &QToolButton::clicked, this, &KaStatusBar::crsClicked);
  addPermanentWidget(m_crsButton);

  m_uploadChip = new QLabel(this);
  m_uploadChip->setObjectName(QStringLiteral("uploadCrsChip"));
  addPermanentWidget(m_uploadChip);

  QFrame* afterCrs = makeSeparator(this);
  addPermanentWidget(afterCrs);

  m_renderButton = new QToolButton(this);
  m_renderButton->setObjectName(QStringLiteral("renderToggle"));
  m_renderButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
  m_renderButton->setCheckable(true);
  m_renderButton->setChecked(true);
  m_renderButton->setText(QStringLiteral("지도갱신 켜짐"));
  m_renderButton->setCursor(Qt::PointingHandCursor);
  m_renderButton->setToolTip(
      QStringLiteral("지도 다시 그리기를 잠시 멈춥니다. 레이어를 여러 개 켜고 끌 때 편합니다."));
  connect(m_renderButton, &QToolButton::toggled, this, [this](bool on) {
    m_renderButton->setText(on ? QStringLiteral("지도갱신 켜짐") : QStringLiteral("지도갱신 멈춤"));
    emit renderingToggled(on);
  });
  addPermanentWidget(m_renderButton);
  m_mapOnly = {m_xy, afterXy, afterCrs, m_renderButton};
  m_scaleOnly = {scaleLabel, m_scaleCombo, afterScale};
  m_surveyOnly = {m_crsButton, m_uploadChip};

  setWorkCrs(QStringLiteral("EPSG:5186"));
  setUploadCrs(QStringLiteral("EPSG:5179"));
  syncChipVisibility();
}

void KaStatusBar::setSnapState(bool on) {
  m_snapKnown = true;
  m_snapOn = on;
  m_snapChip->setText(on ? QStringLiteral("자석 켬") : QStringLiteral("자석 끔"));
  m_snapChip->setTone(on ? KaChip::Tone::Accent : KaChip::Tone::Neutral);
  syncChipVisibility();
}

void KaStatusBar::setUnsavedCount(int features, bool projectDirty) {
  m_unsavedWanted = features > 0 || projectDirty;
  if (features > 0)
    m_unsavedChip->setText(QStringLiteral("저장 안 됨 %1건").arg(features));
  else if (projectDirty)
    m_unsavedChip->setText(QStringLiteral("저장 안 됨"));
  m_unsavedChip->setTone(KaChip::Tone::Warn);
  syncChipVisibility();
}

void KaStatusBar::syncChipVisibility() {
  if (m_snapChip) m_snapChip->setVisible(m_mapReadoutVisible && m_snapKnown);
  if (m_unsavedChip) m_unsavedChip->setVisible(m_surveyChipsVisible && m_unsavedWanted);
}

void KaStatusBar::setCoordinate(double x, double y) {
  const QLocale loc;
  m_xy->setText(QStringLiteral("X %1    Y %2")
                    .arg(loc.toString(x, 'f', 3), loc.toString(y, 'f', 3)));
}

void KaStatusBar::clearCoordinate() {
  m_xy->setText(QStringLiteral("X —    Y —"));
}

void KaStatusBar::setWorkCrs(const QString& authId) {
  m_workCrs = authId;
  refreshCrsText();
}

void KaStatusBar::setUploadCrs(const QString& authId) {
  m_uploadCrs = authId;
  refreshCrsText();
}

void KaStatusBar::refreshCrsText() {
  const QString work = shortCrs(m_workCrs);
  const QString upload = shortCrs(m_uploadCrs);
  m_crsButton->setText(QStringLiteral("작업 %1").arg(work));
  m_crsButton->setToolTip(
      QStringLiteral("지금 그리는 좌표계는 %1입니다. 눌러서 바꿀 수 있습니다.").arg(m_workCrs));
  m_uploadChip->setText(QStringLiteral("→ 제출 %1").arg(upload));
  m_uploadChip->setToolTip(
      QStringLiteral("제출용 파일은 작업 좌표계와 상관없이 항상 %1로 변환되어 나갑니다.")
          .arg(m_uploadCrs));
}

void KaStatusBar::setRenderingEnabled(bool on) {
  if (m_renderButton->isChecked() != on)
    m_renderButton->setChecked(on);
}

bool KaStatusBar::isRenderingEnabled() const {
  return m_renderButton->isChecked();
}

void KaStatusBar::setMapInstrumentsVisible(bool visible) {
  setInstrumentsVisible(visible, visible);
}

void KaStatusBar::setInstrumentsVisible(bool mapReadout, bool scale) {
  m_mapReadoutVisible = mapReadout;
  for (QWidget* widget : std::as_const(m_mapOnly))
    widget->setVisible(mapReadout);
  for (QWidget* widget : std::as_const(m_scaleOnly))
    widget->setVisible(scale);
  syncChipVisibility();
}

void KaStatusBar::setCrsChipsVisible(bool visible) {
  m_surveyChipsVisible = visible;
  for (QWidget* widget : std::as_const(m_surveyOnly))
    widget->setVisible(visible);
  syncChipVisibility();
}
