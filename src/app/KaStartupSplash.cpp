#include "KaStartupSplash.h"

#include "KaSplashCredits.h"
#include "KaSplashScene.h"

#include <QGuiApplication>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QTimer>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

// Honors Windows "애니메이션 효과 표시". KA_HGIS_REDUCED_MOTION=1 or 0 forces it
// on or off for QA.
bool reducedMotionRequested() {
  if (qEnvironmentVariableIsSet("KA_HGIS_REDUCED_MOTION"))
    return qEnvironmentVariableIntValue("KA_HGIS_REDUCED_MOTION") == 1;
#ifdef Q_OS_WIN
  BOOL animations = TRUE;
  if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0))
    return animations == FALSE;
#endif
  return false;
}

// Card geometry shared by painting and the interactive plan.
struct Layout {
  QRectF card;
  QRectF plan;
  QRectF header;
  QRectF block;
  QRectF bar;
  double unit = 1.0;
};

Layout layoutFor(const QRectF& bounds) {
  Layout l;
  l.card = bounds.adjusted(1, 1, -1, -1);
  l.unit = l.card.height() / 550.0;
  const double u = l.unit, margin = 24 * u;
  const double top = l.card.top() + margin, bottom = l.card.bottom() - 92 * u;
  const QRectF content(l.card.left() + margin, top, l.card.width() - 2 * margin, bottom - top);
  l.plan = QRectF(content.left(), content.top(), content.width() * 0.44, content.height());
  const double right = l.plan.right() + 20 * u;
  l.header = QRectF(right, content.top(), content.right() - right, 56 * u);
  l.block = QRectF(right, l.header.bottom() + 16 * u, content.right() - right,
                   content.bottom() - l.header.bottom() - 16 * u);
  l.bar = QRectF(content.left(), l.card.bottom() - 62 * u, content.width(), 8 * u);
  return l;
}

}  // namespace

QString KaStartupSplash::attributionText() {
  // Existing application notices (LICENSE and MainWindow::showAbout), with
  // WebEngine identified separately because its third-party terms also apply.
  return QStringLiteral(
      "QGIS  © QGIS Development Team · GNU GPL v2 이상\n"
      "Qt  © The Qt Company 및 기여자 · LGPLv3 / GPLv2 / GPLv3\n"
      "Qt WebEngine / Chromium · 구성 요소별 오픈소스 라이선스\n"
      "GDAL/OGR  © OSGeo 및 기여자 · MIT 계열\n"
      "PROJ  © PROJ contributors · MIT\n"
      "GEOS  © GEOS contributors · LGPLv2.1\n"
      "SQLite · Public domain\n"
      "지도·자료: VWorld · 국토정보플랫폼 · 국가유산 공간정보 · 흙토람 · KIGAM 등\n"
      "지도·자료 및 PROJ 데이터는 제공처의 저작권·이용조건을 따릅니다.");
}

QString KaStartupSplash::creditsText() {
  return KaSplashCredits::plainText() +
         QStringLiteral("\n자세한 저작권 안내는 앱의 도움말 → 정보에서 다시 볼 수 있습니다.");
}

KaStartupSplash::KaStartupSplash(QWidget* parent, int readingDurationMs)
    : QWidget(parent, Qt::SplashScreen | Qt::FramelessWindowHint),
      m_readingDurationMs(qMax(1, readingDurationMs)),
      m_scene(std::make_unique<KaSplashScene>()),
      m_icon(QStringLiteral(":/ka-hgis/app-icon.png")),
      m_reducedMotion(reducedMotionRequested()) {
  setObjectName(QStringLiteral("startupSplash"));
  setWindowTitle(QStringLiteral("필드고고학GIS v2 · 시작 안내"));
  setAttribute(Qt::WA_TranslucentBackground);
  setMouseTracking(true);
  setAccessibleName(QStringLiteral("필드고고학GIS v2 시작 안내"));
  setAccessibleDescription(creditsText());

  m_timer = new QTimer(this);
  m_timer->setInterval(25);
  m_timer->setTimerType(Qt::PreciseTimer);
  connect(m_timer, &QTimer::timeout, this, &KaStartupSplash::tick);
  const QRect available = QGuiApplication::primaryScreen()
                              ? QGuiApplication::primaryScreen()->availableGeometry()
                              : QRect(0, 0, 1024, 768);
  resize(qMin(780, available.width() - 32), qMin(550, available.height() - 32));
  move(available.center() - rect().center());
}

KaStartupSplash::~KaStartupSplash() = default;

void KaStartupSplash::markReady() {
  if (m_readingClock.isValid())
    return;
  m_readingClock.start();
  m_frameClock.start();
  tick();
  m_timer->start();
}

double KaStartupSplash::phase() const {
  if (!m_readingClock.isValid()) return 0.0;
  return m_reducedMotion ? 1.0 : m_progress / 1000.0;
}

QString KaStartupSplash::statusText() const {
  if (!m_readingClock.isValid()) return QStringLiteral("앱을 준비하고 있습니다…");
  if (m_completed) return QStringLiteral("준비 완료");
  if (!m_reducedMotion && !m_scene->interacted())  // invite the user to dig
    return QStringLiteral("도면 위에서 마우스로 흙을 걷어 보세요 · 잠시 후 홈 화면이 열립니다.");
  return QStringLiteral("안내를 확인해 주세요. 잠시 후 홈 화면이 열립니다.");
}

QString KaStartupSplash::secondsText() const {
  if (!m_readingClock.isValid())
    return QStringLiteral("안내 시간 %1초").arg((m_readingDurationMs + 999) / 1000);
  const qint64 left = qMax<qint64>(0, m_readingDurationMs - m_readingClock.elapsed());
  return QStringLiteral("%1초 남음").arg((left + 999) / 1000);
}

void KaStartupSplash::tick() {
  if (!m_readingClock.isValid() || m_completed)
    return;
  const qint64 elapsed = m_readingClock.elapsed();
  m_progress = int(qMin<qint64>(1000, elapsed * 1000 / m_readingDurationMs));
  const double dt = qMin(0.1, m_frameClock.restart() / 1000.0);
  layoutScene();
  m_scene->advance(phase(), m_reducedMotion ? 0.0 : dt);
  update();
  if (elapsed < m_readingDurationMs)
    return;
  m_completed = true;
  m_timer->stop();
  emit readyToShow();
}

void KaStartupSplash::layoutScene() {
  m_scene->setRect(layoutFor(QRectF(rect())).plan, devicePixelRatioF());
}

QRectF KaStartupSplash::planRect() const {
  return layoutFor(QRectF(rect())).plan;
}

double KaStartupSplash::revealedFraction() const {
  return m_scene->revealedFraction();
}

void KaStartupSplash::resizeEvent(QResizeEvent*) {
  layoutScene();
}

void KaStartupSplash::trackPointer(const QPointF& pos, bool pressed) {
  if (m_reducedMotion || !m_readingClock.isValid() || m_completed) return;
  layoutScene();
  const bool inside = m_scene->pointerMoved(pos, pressed);
  if (inside)
    setCursor(m_icon.isNull() ? Qt::CrossCursor : Qt::BlankCursor);
  else
    unsetCursor();
  update();
}

void KaStartupSplash::mouseMoveEvent(QMouseEvent* event) {
  trackPointer(event->position(), event->buttons() & Qt::LeftButton);
}

void KaStartupSplash::mousePressEvent(QMouseEvent* event) {
  // A click digs deeper; it never closes the notice early.
  trackPointer(event->position(), true);
}

void KaStartupSplash::leaveEvent(QEvent*) {
  m_scene->pointerLeft();
  unsetCursor();
  update();
}

void KaStartupSplash::paintEvent(QPaintEvent*) {
  layoutScene();
  const Layout l = layoutFor(QRectF(rect()));
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);
  QPainterPath outline;
  outline.addRoundedRect(l.card, 24, 24);
  QLinearGradient blue(0, 0, width() * 0.7, height());
  blue.setColorAt(0, QColor(QStringLiteral("#267abb")));
  blue.setColorAt(0.4, QColor(QStringLiteral("#12558d")));
  blue.setColorAt(1, QColor(QStringLiteral("#092f56")));
  painter.fillPath(outline, blue);

  painter.save();
  painter.setClipPath(outline);  // nothing escapes the rounded corners
  QLinearGradient gloss(0, 0, 0, height() * 0.45);
  gloss.setColorAt(0, QColor(255, 255, 255, 76));
  gloss.setColorAt(1, QColor(255, 255, 255, 0));
  painter.setPen(Qt::NoPen);
  painter.setBrush(gloss);
  painter.drawEllipse(QRectF(-width() * 0.3, -height() * 0.65, width() * 1.7, height()));

  const double ph = phase();
  m_scene->paint(painter, ph, m_icon);
  KaSplashCredits::paintHeader(painter, l.header, m_icon, m_reducedMotion ? 0.0 : (ph - 0.3) * 4.0);
  KaSplashCredits::paintTitleBlock(painter, l.block, m_reducedMotion ? 0.0 : ph * 4.0);
  KaSplashCredits::paintFooter(painter, l.bar, l.card, l.unit, m_progress / 1000.0,
                               statusText(), secondsText());
  painter.restore();

  painter.setPen(QPen(QColor(182, 225, 255, 185), 1));
  painter.setBrush(Qt::NoBrush);
  painter.drawPath(outline);
}
