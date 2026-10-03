#include "KaStartupSplash.h"

#include "KaSplashArt.h"
#include "KaSplashCredits.h"
#include "KaSplashPalette.h"
#include "KaSplashStrata.h"

#include <QCoreApplication>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QScreen>
#include <QTimer>

#include <algorithm>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using namespace KaSplashPalette;

namespace {

// Transparent margin around the card; the lower edge is wider for the shadow.
constexpr double kSide = 16, kTopMargin = 10, kBottomMargin = 24;
constexpr double kCardWidth = 700, kCardHeight = 400, kRadius = 16;

const QString kPreparing = QStringLiteral("앱을 준비하고 있습니다");
const QString kOpening = QStringLiteral("잠시 후 홈 화면이 열립니다");

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

struct Layout {
  QRectF card;
  QRectF icon;
  QPointF title;
  QPointF status;
  QRectF dots;
  QRectF notices;
  double unit = 1.0;
};

Layout layoutFor(const QRectF& bounds) {
  Layout l;
  l.card = bounds.adjusted(kSide, kTopMargin, -kSide, -kBottomMargin);
  const double u = l.unit = std::max(0.6, l.card.height() / kCardHeight);
  const double left = l.card.left() + 40 * u, width = l.card.width() - 80 * u;
  const double side = 76 * u;
  const QPointF summit(l.card.left() + l.card.width() * 0.78, l.card.top() + l.card.height() * 0.36);
  l.icon = QRectF(summit.x() - side / 2, summit.y() - side / 2, side, side);
  l.title = QPointF(left, l.card.bottom() - 106 * u);
  l.status = QPointF(left, l.card.bottom() - 80 * u);
  const QFontMetricsF metrics(uiFont(12.5 * u));
  const double text = std::max(metrics.horizontalAdvance(kPreparing), metrics.horizontalAdvance(kOpening));
  l.dots = QRectF(left + text + 16 * u, l.status.y() - 9.5 * u, 96 * u, 9 * u);
  l.notices = QRectF(left, l.card.bottom() - 61 * u, width, 36 * u);
  return l;
}

}  // namespace

QString KaStartupSplash::attributionText() {
  return QStringLiteral(
      "QGIS  © QGIS Development Team · GNU GPL v2 이상\n"
      "Qt  © The Qt Company 및 기여자 · LGPLv3 / GPLv2 / GPLv3\n"
      "Qt WebEngine / Chromium · 구성 요소별 오픈소스 라이선스\n"
      "GDAL/OGR  © OSGeo 및 기여자 · MIT 계열\n"
      "PROJ  © PROJ contributors · MIT\n"
      "GEOS  © GEOS contributors · LGPLv2.1\n"
      "SQLite · Public domain\n"
      "지도·자료: VWorld · 국토정보플랫폼 · 국가유산 공간정보 · 흙토람 · KIGAM · 국사편찬위원회 등\n"
      "지도·자료 및 PROJ 데이터는 제공처의 저작권·이용조건을 따릅니다.");
}

QString KaStartupSplash::creditsText() {
  return KaSplashCredits::plainText() +
         QStringLiteral("\n자세한 저작권 안내는 앱의 더보기 → 정보에서 다시 볼 수 있습니다.");
}

KaStartupSplash::KaStartupSplash(QWidget* parent, int readingDurationMs, bool strata)
    : QWidget(parent, Qt::SplashScreen | Qt::FramelessWindowHint),
      m_readingDurationMs(qMax(1, readingDurationMs)),
      m_icon(strata ? QStringLiteral(":/ka-hgis/app-icon-paper.png") : QStringLiteral(":/ka-hgis/app-icon.png")),
      m_reducedMotion(reducedMotionRequested()),
      m_strata(strata) {
  setObjectName(QStringLiteral("startupSplash"));
  setWindowTitle(QStringLiteral("Strata · 필드고고학 GIS 시작 안내"));
  setAttribute(Qt::WA_TranslucentBackground);
  setAccessibleName(QStringLiteral("Strata 필드고고학 GIS 시작 안내"));
  setAccessibleDescription(creditsText());

  m_timer = new QTimer(this);
  m_timer->setInterval(16);
  m_timer->setTimerType(Qt::PreciseTimer);
  connect(m_timer, &QTimer::timeout, this, &KaStartupSplash::tick);
  placeWindow();
  m_readingClock.start();
}

KaStartupSplash::~KaStartupSplash() = default;

void KaStartupSplash::placeWindow() {
  const QRect available = QGuiApplication::primaryScreen()
                              ? QGuiApplication::primaryScreen()->availableGeometry()
                              : QRect(0, 0, 1024, 768);
  const double w = kCardWidth + 2 * kSide, h = kCardHeight + kTopMargin + kBottomMargin;
  // Small screens shrink the whole notice evenly instead of cropping it.
  const double scale = std::min({1.0, (available.width() - 24) / w, (available.height() - 24) / h});
  setFixedSize(int(w * scale), int(h * scale));
  move(available.center() - rect().center());
}

QRectF KaStartupSplash::cardRect() const {
  return layoutFor(QRectF(rect())).card;
}

QRectF KaStartupSplash::dotsRect() const {
  return layoutFor(QRectF(rect())).dots;
}

QRectF KaStartupSplash::noticesRect() const {
  return layoutFor(QRectF(rect())).notices;
}

void KaStartupSplash::paintBackdrop(QPainter& painter, qreal dpr) const {
  const Layout l = layoutFor(QRectF(rect()));
  painter.setRenderHint(QPainter::Antialiasing);
  KaSplashArt::paintShadow(painter, l.card, kRadius);
  KaSplashArt::paintCard(painter, l.card, kRadius,
                         KaSplashArt::contours(l.card.size(), dpr, l.icon.center() - l.card.topLeft()));
  KaSplashArt::paintIcon(painter, l.icon, m_icon);
}

QImage KaStartupSplash::backdropImage() const {
  QImage image(size(), QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  paintBackdrop(painter, 1.0);
  return image;
}

void KaStartupSplash::markReady() {
  if (m_ready)
    return;
  m_ready = true;
  m_readyAtMs = m_readingClock.elapsed();
  update();  // the status line changes
  m_timer->start();
}

QString KaStartupSplash::statusText() const {
  if (!m_ready) return kPreparing;
  if (m_completed) return QStringLiteral("준비 완료");
  return kOpening;
}

void KaStartupSplash::tick() {
  if (!m_ready || m_completed)
    return;
  const qint64 elapsed = m_readingClock.elapsed();
  m_progress = int(qMin<qint64>(1000, elapsed * 1000 / m_readingDurationMs));
  if (!m_reducedMotion && m_strata) {
    update();  // the section draws itself in, then the progress line fills
  } else if (!m_reducedMotion) {
    const Layout l = layoutFor(QRectF(rect()));
    update(l.dots.toAlignedRect().adjusted(-4, -4, 4, 4));
  }
  if (elapsed < m_readingDurationMs)
    return;
  m_completed = true;
  m_timer->stop();
  emit readyToShow();
}

const QPixmap& KaStartupSplash::staticLayer() {
  const qreal dpr = devicePixelRatioF();
  const QSize pixels = (QSizeF(size()) * dpr).toSize();
  if (m_static.size() == pixels && qFuzzyCompare(m_static.devicePixelRatioF(), dpr))
    return m_static;
  m_static = QPixmap(pixels);
  m_static.setDevicePixelRatio(dpr);
  m_static.fill(Qt::transparent);
  const Layout l = layoutFor(QRectF(rect()));
  QPainter painter(&m_static);
  painter.setRenderHint(QPainter::TextAntialiasing);
  paintBackdrop(painter, dpr);
  KaSplashCredits::paintTitle(painter, l.title, l.unit, QCoreApplication::applicationVersion());
  KaSplashCredits::paintNotices(painter, l.notices, l.unit);
  return m_static;
}

// 새 모양: the whole card is one picture (KaSplashStrata). Nothing is cached, because the section
// draws itself in once the app is ready.
void KaStartupSplash::paintStrata() {
  const Layout l = layoutFor(QRectF(rect()));
  QPainter painter(this);
  painter.setCompositionMode(QPainter::CompositionMode_Source);
  painter.fillRect(rect(), Qt::transparent);
  painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
  painter.setRenderHint(QPainter::Antialiasing);
  KaSplashArt::paintShadow(painter, l.card, kRadius);
  KaSplashStrata::Frame frame;
  frame.seconds = flowSeconds();
  frame.still = m_reducedMotion;
  frame.progress = m_progress / 1000.0;
  frame.status = statusText();
  frame.version = QCoreApplication::applicationVersion();
  KaSplashStrata::paint(painter, l.card, kRadius, m_icon, frame);
}

void KaStartupSplash::paintEvent(QPaintEvent*) {
  if (m_strata) {
    paintStrata();
    return;
  }
  const QPixmap& layer = staticLayer();
  const Layout l = layoutFor(QRectF(rect()));
  QPainter painter(this);
  painter.setCompositionMode(QPainter::CompositionMode_Source);
  painter.drawPixmap(0, 0, layer);
  painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setFont(uiFont(12.5 * l.unit));
  painter.setPen(withAlpha(kInk, 0.90));
  painter.drawText(l.status, statusText());
  const bool still = m_reducedMotion || !m_ready;
  KaSplashArt::paintFlowDots(painter, l.dots, flowSeconds(), still);
}
