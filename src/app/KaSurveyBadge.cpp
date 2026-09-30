#include "app/KaSurveyBadge.h"

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
  paintChip(p, rect().adjusted(0, 0, -kRightGap, 0), text(), tone(), glyph());
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
