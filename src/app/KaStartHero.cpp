#include "KaStartHero.h"

#include "KaSplashArt.h"
#include "KaSplashPalette.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
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

void KaStartHero::paintEvent(QPaintEvent*) {
  const QRectF band = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  QPainterPath outline;
  outline.addRoundedRect(band, kRadius, kRadius);
  // Darker than the notice's card, so light text keeps 4.5:1 across the band.
  QLinearGradient blue(band.topLeft(), band.bottomRight());
  blue.setColorAt(0.0, KaSplashPalette::kMid);
  blue.setColorAt(1.0, KaSplashPalette::kDeep);
  painter.fillPath(outline, blue);

  const qreal dpr = devicePixelRatioF();
  if (m_texture.isNull() || !qFuzzyCompare(m_texture.devicePixelRatioF(), dpr)) {
    const QPointF summit(band.width() * 0.60, band.height() * 0.42);
    m_texture = QPixmap::fromImage(
        KaSplashArt::contours(band.size(), dpr, summit, kHillUnit, 0.0));
  }
  painter.save();
  painter.setClipPath(outline);
  painter.drawPixmap(band.topLeft(), m_texture);
  painter.restore();
}
