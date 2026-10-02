#include "app/KaSurveyBadge.h"
#include "app/KaTheme.h"

#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

namespace {
constexpr int kRightGap = 8;    // breathing room before the tab bar's right edge
constexpr int kMaxNamePx = 200;  // long survey names are elided in the middle
}  // namespace

KaSurveyBadge::KaSurveyBadge(QWidget* parent) : KaChip(QString(), Tone::Ok, parent) {
  setObjectName(QStringLiteral("surveyBadge"));
  setCursor(Qt::PointingHandCursor);
  setFocusPolicy(Qt::TabFocus);
  setToolTip(QStringLiteral("열려 있는 조사입니다. 누르면 지도 탭으로 갑니다."));
  hide();
}

QString KaSurveyBadge::textFor(const QString& name, bool unsaved) {
  const QString trimmed = name.trimmed();
  if (trimmed.isEmpty()) return QString();
  return unsaved ? QStringLiteral("%1 · 저장 안 됨").arg(trimmed) : QStringLiteral("조사 열림 · %1").arg(trimmed);
}

QString KaSurveyBadge::windowTitleFor(const QString& name, bool unsaved) {
  const QString star = unsaved ? QStringLiteral(" *") : QString();
  const QString trimmed = name.trimmed();
  return trimmed.isEmpty() ? QStringLiteral("Strata") + star : trimmed + star + QStringLiteral(" - Strata");
}

void KaSurveyBadge::setSurvey(const QString& name, bool unsaved) {
  m_name = name.trimmed();
  m_unsaved = unsaved;
  if (m_name.isEmpty()) {
    setText(QString());
    hide();
    return;
  }
  const QFontMetrics fm(chipFont(font()));
  const QString shown = fm.elidedText(m_name, Qt::ElideMiddle, kMaxNamePx);
  setTone(unsaved ? Tone::Warn : Tone::Ok);
  setGlyph(unsaved ? QStringLiteral("warn") : QStringLiteral("check"));
  setText(textFor(shown, unsaved));
  updateGeometry();
  show();
}

QSize KaSurveyBadge::sizeHint() const {
  const QSize chip = KaChip::sizeHint();
  return QSize(chip.width() + kRightGap, chip.height());
}

void KaSurveyBadge::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event);
  QPainter p(this);
  p.setFont(font());
  const QRect pill = rect().adjusted(0, 0, -kRightGap, 0);
  paintChip(p, pill, text(), tone(), glyph());
  // 목업의 조사 칩은 톤 색 테두리가 또렷한 알약이다(공용 칩의 옅은 가장자리 위에 한 번 더).
  const int h = KaTheme::buttonMetrics().chipHeight, radius = KaTheme::buttonMetrics().chipRadius;
  QColor edge = toneInk(tone());
  edge.setAlpha(150);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(QPen(edge, 1.0));
  p.setBrush(Qt::NoBrush);
  p.drawRoundedRect(QRectF(pill.left(), pill.top() + (pill.height() - h) / 2, pill.width(), h).adjusted(0.5, 0.5, -0.5, -0.5),
                    radius - 0.5, radius - 0.5);
}

void KaSurveyBadge::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    m_pressed = true;
    event->accept();
    return;
  }
  KaChip::mousePressEvent(event);
}

void KaSurveyBadge::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton && m_pressed) {
    m_pressed = false;
    event->accept();
    if (rect().contains(event->pos())) emit clicked();
    return;
  }
  KaChip::mouseReleaseEvent(event);
}

void KaSurveyBadge::keyPressEvent(QKeyEvent* event) {
  if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    event->accept();
    emit clicked();
    return;
  }
  KaChip::keyPressEvent(event);
}
