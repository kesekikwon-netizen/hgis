#include "KaHomeConnectionCard.h"

#include "KaIcons.h"
#include "KaTheme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace {

constexpr int kRowHeight = 48;
constexpr int kDotPx = 8;

QLabel* label(const QString& text, const char* name, QWidget* parent) {
  auto* l = new QLabel(text, parent);
  l->setObjectName(QString::fromLatin1(name));
  return l;
}

QPixmap dotPixmap(bool ok, qreal dpr) {
  QPixmap pm(QSize(kDotPx, kDotPx) * dpr);
  pm.setDevicePixelRatio(dpr);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(Qt::NoPen);
  p.setBrush(ok ? KaTheme::tokens().ok : KaTheme::tokens().borderStrong);
  p.drawEllipse(QRectF(0.5, 0.5, kDotPx - 1.0, kDotPx - 1.0));
  return pm;
}

QString dotHtml(bool ok) {
  const QColor color = ok ? KaTheme::tokens().ok : KaTheme::tokens().danger;
  return QStringLiteral("<span style=\"color:%1\">●</span>&nbsp;").arg(color.name());
}

}  // namespace

KaHomeConnectionCard::KaHomeConnectionCard(QWidget* parent) : QFrame(parent) {
  qRegisterMetaType<AccountStatus::Source>();  // signal argument for queued and spied connections
  setObjectName(QStringLiteral("startConnectionCard"));
  auto* col = new QVBoxLayout(this);
  col->setContentsMargins(16, 12, 16, 12);
  col->setSpacing(8);
  auto* head = new QHBoxLayout;
  head->setSpacing(6);
  head->addWidget(label(QStringLiteral("연결 상태"), "startRecentTitle", this));
  head->addSpacing(8);
  auto* lock = new QLabel(this);
  lock->setPixmap(KaIcons::glyphPixmap(QStringLiteral("lock"), KaTheme::tokens().inkMuted, 14, devicePixelRatioF()));
  lock->setFixedSize(14, 14);
  head->addWidget(lock, 0, Qt::AlignVCenter);
  head->addWidget(label(QStringLiteral("비밀값은 표시되지 않습니다"), "startPrivacyNote", this), 0, Qt::AlignVCenter);
  head->addStretch(1);
  m_internet = label(QStringLiteral("인터넷 · 확인 안 함"), "startStepBody", this);
  m_internet->setTextFormat(Qt::RichText);
  m_internetButton = new QPushButton(QStringLiteral("인터넷 확인"), this);
  m_internetButton->setObjectName(QStringLiteral("startEnvInternet"));
  m_internetButton->setCursor(Qt::PointingHandCursor);
  m_internetButton->setToolTip(QStringLiteral("VWorld 서버에 한 번 접속해 봅니다. 누를 때만 확인합니다."));
  connect(m_internetButton, &QPushButton::clicked, this, [this]() { checkInternet(); });
  head->addWidget(m_internet, 0, Qt::AlignVCenter);
  head->addWidget(m_internetButton, 0, Qt::AlignVCenter);
  col->addLayout(head);
  m_rows = new QHBoxLayout;  // the five sources side by side
  m_rows->setSpacing(8);
  col->addLayout(m_rows);
  m_problems = new QVBoxLayout;
  m_problems->setSpacing(6);
  col->addLayout(m_problems);
}

QString KaHomeConnectionCard::displayName(const AccountStatus::Entry& entry) {
  switch (entry.source) {
    case AccountStatus::Source::VworldKey: return QStringLiteral("VWorld 키");
    case AccountStatus::Source::TopographicAccount: return QStringLiteral("수치지형도 계정");
    case AccountStatus::Source::HeritageAccount: return QStringLiteral("문화재 인트라넷");
    default: break;
  }
  return entry.label;
}

QString KaHomeConnectionCard::stateText(bool ready) {
  return ready ? QStringLiteral("설정됨") : QStringLiteral("설정 필요");
}

void KaHomeConnectionCard::refresh() { setInputs(KaHomeStatus::collectLocal()); }

void KaHomeConnectionCard::clear(QLayout* layout) {
  while (QLayoutItem* item = layout->takeAt(0)) {
    delete item->widget();
    delete item;
  }
}

void KaHomeConnectionCard::setInputs(const KaHomeStatus::Inputs& inputs) {
  clear(m_rows);
  clear(m_problems);
  const qreal dpr = devicePixelRatioF();
  for (const AccountStatus::Entry& entry : inputs.accounts) {
    auto* row = new QFrame(this);
    row->setObjectName(QStringLiteral("startConnRow"));
    row->setMinimumHeight(kRowHeight);
    auto* line = new QHBoxLayout(row);
    line->setContentsMargins(10, 4, 4, 4);
    line->setSpacing(8);
    auto* dot = new QLabel(row);
    dot->setFixedSize(kDotPx, kDotPx);
    dot->setPixmap(dotPixmap(entry.ready, dpr));
    line->addWidget(dot, 0, Qt::AlignVCenter);
    // The name over its state, so a 250 px cell (1366 px window) still shows both whole.
    auto* words = new QVBoxLayout;
    words->setSpacing(0);
    auto* name = label(displayName(entry), "startConnName", row);
    name->setToolTip(QStringLiteral("%1 — %2 · %3").arg(displayName(entry), entry.usedFor, entry.menuPath));
    name->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);  // a narrower window clips the name, never the button
    words->addWidget(name);
    auto* state = label(stateText(entry.ready), "startConnState", row);
    state->setProperty("ready", entry.ready ? QStringLiteral("true") : QStringLiteral("false"));
    state->setProperty("kaStatusOk", entry.ready);
    state->setToolTip(entry.ready ? QString() : QStringLiteral("%1에서 넣습니다.").arg(entry.menuPath));
    words->addWidget(state);
    line->addLayout(words, 1);
    auto* setup = new QPushButton(QStringLiteral("설정"), row);
    setup->setObjectName(QStringLiteral("startConnectionSetup"));
    setup->setCursor(Qt::PointingHandCursor);
    setup->setToolTip(entry.menuPath);
    const AccountStatus::Source source = entry.source;
    connect(setup, &QPushButton::clicked, this, [this, source]() { emit configureRequested(source); });
    line->addWidget(setup, 0, Qt::AlignVCenter);
    m_rows->addWidget(row, 1);
  }
  // proj.db and the survey folder speak only when something is wrong.
  const QVector<KaHomeStatus::Line> lines = KaHomeStatus::describe(inputs);
  for (int i = inputs.accounts.size(); i < lines.size(); ++i) {
    if (lines.at(i).ok) continue;
    auto* problem = label(lines.at(i).hint.isEmpty() ? lines.at(i).text
                                                     : QStringLiteral("%1 — %2").arg(lines.at(i).text, lines.at(i).hint),
                          "startConnProblem", this);
    problem->setWordWrap(true);
    problem->setProperty("kaStatusOk", false);
    m_problems->addWidget(problem);
  }
}

void KaHomeConnectionCard::checkInternet() {
  if (!m_network) m_network = new QNetworkAccessManager(this);
  m_internetButton->setEnabled(false);
  m_internet->setText(QStringLiteral("인터넷 · 확인 중…"));
  QNetworkRequest request(QUrl(QStringLiteral("https://api.vworld.kr/")));
  request.setTransferTimeout(6000);
  QNetworkReply* reply = m_network->head(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    reply->deleteLater();
    // Any HTTP answer means the network and the server are reachable.
    const bool reached = reply->error() == QNetworkReply::NoError ||
                         reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() > 0;
    m_internet->setText(dotHtml(reached) + (reached ? QStringLiteral("인터넷 · 연결됨")
                                                    : QStringLiteral("인터넷 · 연결 안 됨")));
    m_internet->setToolTip(reached ? QString()
                                   : QStringLiteral("오프라인이어도 조사 열기·그리기·저장은 됩니다. "
                                                    "배경 지도와 자료 받기만 인터넷이 필요합니다."));
    m_internetButton->setEnabled(true);
  });
}
