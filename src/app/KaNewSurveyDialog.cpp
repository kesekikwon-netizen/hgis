#include "KaNewSurveyDialog.h"

#include "KaTheme.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStringList>

#include <cmath>

namespace {

// Survey files also get "_복사본", recovery and generation names next to them; keep the
// base short enough that the full Windows path stays well under MAX_PATH.
constexpr int kMaxNameLength = 80;

bool isReservedDeviceName(const QString& name) {
  // Windows treats "CON", "nul.txt" and "COM1 .gpkg" alike: the part before the first dot,
  // without trailing spaces, is what makes a name reserved.
  const QString stem = name.section(QLatin1Char('.'), 0, 0).trimmed().toUpper();
  static const QStringList kFixed = {QStringLiteral("CON"), QStringLiteral("PRN"),
                                     QStringLiteral("AUX"), QStringLiteral("NUL")};
  if (kFixed.contains(stem)) return true;
  if (stem.size() != 4 || !(stem.startsWith(QLatin1String("COM")) || stem.startsWith(QLatin1String("LPT"))))
    return false;
  const char16_t last = stem.at(3).unicode();
  return (last >= u'0' && last <= u'9') || last == u'¹' || last == u'²' || last == u'³';
}

QString zoneLabel(const QString& authId) {
  return authId.endsWith(QLatin1String("5186")) ? QStringLiteral("중부원점(5186)")
                                                : QStringLiteral("동부원점(5187)");
}

}  // namespace

KaNewSurveyDialog::KaNewSurveyDialog(const QString& currentWorkCrs, double mapLongitude,
                                     QWidget* parent)
    : QDialog(parent), m_suggested(suggestedWorkCrs(mapLongitude)), m_longitude(mapLongitude) {
  setWindowTitle(QStringLiteral("새 조사"));
  setMinimumWidth(460);
  auto* form = new QFormLayout(this);
  form->setSpacing(12);
  form->setContentsMargins(20, 20, 20, 16);

  m_name = new QLineEdit(this);
  m_name->setObjectName(QStringLiteral("newSurveyName"));
  m_name->setPlaceholderText(QStringLiteral("예: 병산동"));
  m_name->setMinimumHeight(36);
  m_name->setMaxLength(kMaxNameLength + 20);  // room to show the length message
  m_nameError = new QLabel(this);
  m_nameError->setObjectName(QStringLiteral("newSurveyNameError"));
  m_nameError->setWordWrap(true);
  m_nameError->setStyleSheet(QStringLiteral("color: %1;").arg(KaTheme::tokens().danger.name()));
  m_nameError->hide();

  auto* crsRow = new QHBoxLayout();
  m_btn5186 = new QPushButton(QStringLiteral("5186  중부원점"), this);
  m_btn5187 = new QPushButton(QStringLiteral("5187  동부원점"), this);
  m_btn5186->setObjectName(QStringLiteral("newSurveyCrs5186"));
  m_btn5187->setObjectName(QStringLiteral("newSurveyCrs5187"));
  auto* origins = new QButtonGroup(this);
  origins->setExclusive(true);
  for (auto* button : {m_btn5186, m_btn5187}) {
    button->setCheckable(true);
    button->setAutoDefault(false);
    button->setMinimumHeight(40);
    button->setCursor(Qt::PointingHandCursor);
    origins->addButton(button);
    crsRow->addWidget(button, 1);
  }
  // The caller's origin stays selected (5187 unless a 5186 survey was open); no auto-switch.
  const bool use5187 = currentWorkCrs.contains(QLatin1String("5187"));
  m_btn5186->setChecked(!use5187);
  m_btn5187->setChecked(use5187);

  auto* regions = new QLabel(regionHint(), this);
  regions->setObjectName(QStringLiteral("newSurveyCrsHint"));
  regions->setWordWrap(true);
  m_suggestion = new QLabel(this);
  m_suggestion->setObjectName(QStringLiteral("newSurveyCrsSuggestion"));
  m_suggestion->setWordWrap(true);
  m_suggestion->setVisible(!m_suggested.isEmpty());
  auto* tip = new QLabel(
      QStringLiteral("작업 좌표계는 나중에 아래 상태줄의 「작업 5186/5187」 단추를 눌러 바꿀 수 "
                     "있습니다. 제출 파일은 작업 좌표계와 상관없이 5179로 변환됩니다."),
      this);
  tip->setObjectName(QStringLiteral("newSurveyTip"));
  tip->setWordWrap(true);

  form->addRow(QStringLiteral("조사명"), m_name);
  form->addRow(QString(), m_nameError);
  form->addRow(QStringLiteral("작업 좌표계"), crsRow);
  form->addRow(regions);
  form->addRow(m_suggestion);
  form->addRow(tip);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  m_ok = buttons->button(QDialogButtonBox::Ok);
  m_ok->setText(QStringLiteral("다음"));
  buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("취소"));
  form->addRow(buttons);
  connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
    if (nameProblem(m_name->text()).isEmpty()) accept();
  });
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(m_name, &QLineEdit::textChanged, this, [this]() { updateState(); });
  connect(origins, &QButtonGroup::buttonClicked, this, [this]() { updateState(); });
  m_name->setFocus();
  updateState();
}

QString KaNewSurveyDialog::surveyName() const { return m_name->text().trimmed(); }

QString KaNewSurveyDialog::workCrs() const {
  return m_btn5187->isChecked() ? QStringLiteral("EPSG:5187") : QStringLiteral("EPSG:5186");
}

void KaNewSurveyDialog::updateState() {
  const QString problem = nameProblem(m_name->text());
  // An empty field only disables 「다음」; the red reason appears once something is typed.
  const bool typed = !m_name->text().trimmed().isEmpty();
  m_nameError->setText(problem);
  m_nameError->setVisible(typed && !problem.isEmpty());
  m_ok->setEnabled(problem.isEmpty());
  if (m_suggested.isEmpty()) return;
  QString text = QStringLiteral("지금 지도 중심(동경 %1°)은 %2 구역입니다.")
                     .arg(m_longitude, 0, 'f', 1)
                     .arg(zoneLabel(m_suggested));
  if (std::abs(m_longitude - 128.0) < 0.15)
    text += QStringLiteral(" 경계에 가까우니 발주처 기준을 확인하세요.");
  if (workCrs() != m_suggested)
    text += QStringLiteral(" 추천: %1. 맞으면 위 단추로 바꾸세요.").arg(zoneLabel(m_suggested));
  m_suggestion->setText(text);
}

QString KaNewSurveyDialog::nameProblem(const QString& raw) {
  const QString name = raw.trimmed();
  if (name.isEmpty()) return QStringLiteral("조사명을 입력하세요.");
  static const QString kForbidden = QStringLiteral("\\/:*?\"<>|");
  for (const QChar c : name) {
    if (c.unicode() < 0x20 || kForbidden.contains(c))
      return QStringLiteral("조사명에는 \\ / : * ? \" < > | 와 제어 문자를 쓸 수 없습니다.");
  }
  if (name.endsWith(QLatin1Char('.')))
    return QStringLiteral("조사명은 마침표(.)로 끝날 수 없습니다. Windows가 파일 이름 끝의 점을 지웁니다.");
  if (isReservedDeviceName(name))
    return QStringLiteral("「%1」은(는) Windows가 장치 이름으로 쓰는 예약 이름이라 조사명으로 쓸 수 없습니다.")
        .arg(name);
  if (name.size() > kMaxNameLength)
    return QStringLiteral("조사명은 %1자 이하로 줄여 주세요.").arg(kMaxNameLength);
  return {};
}

QString KaNewSurveyDialog::suggestedWorkCrs(double longitude) {
  // Korean TM zones (GRS80): 서부 124–126°, 중부 126–128°, 동부 128–130°, 동해 130–132°.
  // Only the two origins this program offers are suggested; 서부·동해 get no suggestion.
  if (!std::isfinite(longitude)) return {};
  if (longitude >= 126.0 && longitude < 128.0) return QStringLiteral("EPSG:5186");
  if (longitude >= 128.0 && longitude < 130.0) return QStringLiteral("EPSG:5187");
  return {};
}

QString KaNewSurveyDialog::regionHint() {
  return QStringLiteral(
      "동경 128° 서쪽(서울·경기·충청·전라·제주, 춘천·원주 등)은 중부원점 5186, 동쪽(대구·경북·"
      "부산·울산·경남 동부, 강릉·삼척 등)은 동부원점 5187을 씁니다. 경계 가까운 곳은 발주처·측량 "
      "성과의 원점을 따르세요.");
}
