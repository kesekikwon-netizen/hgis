#include "KaHomeGuideCard.h"

#include "KaChip.h"
#include "KaTheme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPolygonF>
#include <QVBoxLayout>

namespace {

constexpr int kBadgePx = 26;

QLabel* label(const QString& text, const char* name, QWidget* parent) {
  auto* l = new QLabel(text, parent);
  l->setObjectName(QString::fromLatin1(name));
  return l;
}

// The step badge: done = ok disc with a white check, next = accent disc with the number,
// todo = borderStrong ring with the number in muted ink.
QPixmap badgePixmap(int number, bool done, bool isNext, const QFont& font, qreal dpr) {
  const auto& t = KaTheme::tokens();
  QPixmap pm(QSize(kBadgePx, kBadgePx) * dpr);
  pm.setDevicePixelRatio(dpr);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  const QRectF disc(1.0, 1.0, kBadgePx - 2.0, kBadgePx - 2.0);
  if (done) {
    p.setPen(Qt::NoPen);
    p.setBrush(t.ok);
    p.drawEllipse(disc);
    p.setPen(QPen(Qt::white, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    const QPointF c = disc.center();
    p.drawPolyline(QPolygonF{c + QPointF(-5.5, 0.5), c + QPointF(-1.5, 4.5), c + QPointF(6.0, -4.0)});
    return pm;
  }
  if (isNext) {
    p.setPen(Qt::NoPen);
    p.setBrush(t.accent);
  } else {
    p.setPen(QPen(t.borderStrong, 1.5));
    p.setBrush(Qt::NoBrush);
  }
  p.drawEllipse(disc);
  QFont f = font;
  f.setPixelSize(13);
  f.setBold(true);
  p.setFont(f);
  p.setPen(isNext ? t.surface : t.inkMuted);
  p.drawText(disc, Qt::AlignCenter, QString::number(number));
  return pm;
}

}  // namespace

KaHomeGuideCard::KaHomeGuideCard(QWidget* parent) : QFrame(parent) {
  setObjectName(QStringLiteral("startGuideCard2"));
  auto* col = new QVBoxLayout(this);
  col->setContentsMargins(16, 16, 16, 16);
  col->setSpacing(12);
  auto* head = new QHBoxLayout;
  head->setSpacing(8);
  head->addWidget(label(QStringLiteral("작업 순서"), "startRecentTitle", this));
  head->addStretch(1);
  m_basis = new KaChip(QString(), KaChip::Tone::Neutral, this);
  m_basis->setObjectName(QStringLiteral("startGuideBasis"));
  m_basis->hide();
  head->addWidget(m_basis);
  col->addLayout(head);
  struct Text { const char* title; const char* body; };
  const Text texts[] = {
      {"새 조사", "조사 이름과 좌표계(5186·5187)를 정하면 조사 파일(GPKG)이 만들어집니다."},
      {"조사구역 그리기", "지도 탭에서 위성·지적을 보며 「그리기」로 구역과 유구를 그립니다."},
      {"도면 · 제출", "「도면」으로 종이에 옮기고, 「검수·제출」에서 검수한 뒤 제출 꾸러미"
                      "(5179 SHP·조사도면.pdf·MANIFEST)를 만듭니다."},
  };
  for (int i = 0; i < 3; ++i) {
    auto* row = new QHBoxLayout;
    row->setSpacing(12);
    m_steps[i].badge = label(QString(), "startStepTodo", this);
    m_steps[i].badge->setFixedSize(kBadgePx, kBadgePx);
    row->addWidget(m_steps[i].badge, 0, Qt::AlignTop);
    auto* text = new QVBoxLayout;
    text->setSpacing(4);
    auto* titleRow = new QHBoxLayout;
    titleRow->setSpacing(8);
    titleRow->addWidget(label(QString::fromUtf8(texts[i].title), "startStepTitle", this));
    m_steps[i].next = new KaChip(QStringLiteral("다음"), KaChip::Tone::Accent, this);
    m_steps[i].next->setObjectName(QStringLiteral("startStepNextChip"));
    m_steps[i].next->hide();
    titleRow->addWidget(m_steps[i].next);
    titleRow->addStretch(1);
    text->addLayout(titleRow);
    auto* body = label(QString::fromUtf8(texts[i].body), "startStepBody", this);
    body->setWordWrap(true);
    text->addWidget(body);
    if (i == 2) {
      m_ready = new QFrame(this);
      m_ready->setObjectName(QStringLiteral("startSubmitReady"));
      auto* ready = new QHBoxLayout(m_ready);
      ready->setContentsMargins(0, 0, 0, 0);
      ready->setSpacing(8);
      ready->addWidget(label(QStringLiteral("제출 준비:"), "startStepBody", m_ready));
      m_errors = new KaChip(QString(), KaChip::Tone::Ok, m_ready);
      m_errors->setObjectName(QStringLiteral("startSubmitErrors"));
      ready->addWidget(m_errors);
      ready->addWidget(label(QStringLiteral("·"), "startStepBody", m_ready));
      m_warnings = new KaChip(QString(), KaChip::Tone::Ok, m_ready);
      m_warnings->setObjectName(QStringLiteral("startSubmitWarnings"));
      ready->addWidget(m_warnings);
      ready->addStretch(1);
      text->addWidget(m_ready);
      m_notChecked = label(QString(), "startStepBody", this);
      m_notChecked->setObjectName(QStringLiteral("startSubmitUnchecked"));
      m_notChecked->setWordWrap(true);
      text->addWidget(m_notChecked);
    }
    row->addLayout(text, 1);
    col->addLayout(row);
  }
  setFacts(QString(), SurveyFacts::Facts{});
}

void KaHomeGuideCard::setFacts(const QString& targetName, const SurveyFacts::Facts& facts) {
  const bool hasTarget = !targetName.isEmpty();
  const bool done[3] = {hasTarget, facts.hasCounts() && facts.areas > 0 && facts.features > 0,
                        facts.known && facts.packagedAtMs > 0};
  m_done = 0;
  m_next = 0;
  for (int i = 0; i < 3; ++i) {
    if (done[i]) ++m_done;
    else if (m_next == 0) m_next = i + 1;
  }
  for (int i = 0; i < 3; ++i) applyStep(i, done[i], m_next == i + 1);
  m_basis->setText(QStringLiteral("%1 기준").arg(targetName));
  m_basis->setVisible(hasTarget);
  m_submitLine = SurveyFacts::summaryLine(facts);
  const bool checked = facts.hasCheck();
  m_ready->setVisible(checked);
  m_notChecked->setVisible(!checked);
  m_notChecked->setText(checked ? QString() : m_submitLine);
  if (!checked) return;
  m_errors->setText(QStringLiteral("오류 %1").arg(qMax(0, facts.errors)));
  m_errors->setTone(facts.errors > 0 ? KaChip::Tone::Danger : KaChip::Tone::Ok);
  m_warnings->setText(QStringLiteral("경고 %1").arg(qMax(0, facts.warnings)));
  m_warnings->setTone(facts.warnings > 0 ? KaChip::Tone::Warn : KaChip::Tone::Ok);
}

void KaHomeGuideCard::applyStep(int index, bool done, bool isNext) {
  Step& step = m_steps[index];
  step.badge->setObjectName(done ? QStringLiteral("startStepDone")
                            : isNext ? QStringLiteral("startStepNext")
                                     : QStringLiteral("startStepTodo"));
  step.badge->setPixmap(badgePixmap(index + 1, done, isNext, font(), devicePixelRatioF()));
  step.badge->setToolTip(done ? QStringLiteral("끝났습니다") : isNext ? QStringLiteral("다음 할 일") : QString());
  step.next->setVisible(isNext);
}
