#include "KaStartHero.h"

#include "KaIcons.h"
#include "KaSplashArt.h"
#include "KaSplashPalette.h"
#include "KaTheme.h"

#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QResizeEvent>

namespace {

constexpr double kRadius = 14.0;
// Hills are sized against this length, so the mound keeps the notice's scale
// however wide the window is.
constexpr double kHillUnit = 420.0;

}  // namespace

KaStartHero::KaStartHero(QWidget* parent) : QWidget(parent) {
  setObjectName(QStringLiteral("startHero"));
  setMinimumHeight(220);
}

void KaStartHero::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  m_texture = QPixmap();
}

// A look chosen in 「화면 보기」 arrives as a palette change: the band, its contour lines, the logo and
// the two start buttons' glyphs are painted from tokens, so they are redone here.
void KaStartHero::changeEvent(QEvent* event) {
  QWidget::changeEvent(event);
  if (event->type() != QEvent::PaletteChange)
    return;
  m_texture = QPixmap();
  const auto& t = KaTheme::tokens();
  if (auto* logo = findChild<QLabel*>(QStringLiteral("startHeroLogo")))
    logo->setPixmap(KaIcons::appIcon().pixmap(QSize(48, 48), devicePixelRatioF()));
  if (auto* start = findChild<QPushButton*>(QStringLiteral("startNewBtn")))
    start->setIcon(KaIcons::icon(QStringLiteral("new"), t.heroButtonText));
  if (auto* open = findChild<QPushButton*>(QStringLiteral("startOpenBtn")))
    open->setIcon(KaIcons::icon(QStringLiteral("open"), t.railText));
  update();
}

void KaStartHero::paintEvent(QPaintEvent*) {
  const QRectF band = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  QPainterPath outline;
  outline.addRoundedRect(band, kRadius, kRadius);
  const bool paper = KaTheme::displayOptions().paperLook;
  if (paper) {
    painter.fillPath(outline, KaTheme::tokens().rail);  // a paper band with slate text
  } else {
    // Darker than the notice's card, so light text keeps 4.5:1 across the band.
    QLinearGradient blue(band.topLeft(), band.bottomRight());
    blue.setColorAt(0.0, KaSplashPalette::kMid);
    blue.setColorAt(1.0, KaSplashPalette::kDeep);
    painter.fillPath(outline, blue);
  }

  const qreal dpr = devicePixelRatioF();
  if (m_texture.isNull() || !qFuzzyCompare(m_texture.devicePixelRatioF(), dpr)) {
    const QPointF summit(band.width() * 0.60, band.height() * 0.42);
    m_texture = QPixmap::fromImage(
        KaSplashArt::contours(band.size(), dpr, summit, kHillUnit, 0.0, paper ? KaTheme::tokens().ink : QColor(Qt::white)));
  }
  painter.save();
  painter.setClipPath(outline);
  painter.drawPixmap(band.topLeft(), m_texture);
  painter.restore();
}
