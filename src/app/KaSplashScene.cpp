#include "KaSplashScene.h"

#include "KaSplashPalette.h"

#include <QPointF>

void KaSplashScene::setRect(const QRectF& plan, qreal devicePixelRatio) {
  const qreal dpr = devicePixelRatio > 0 ? devicePixelRatio : 1.0;
  m_rect = plan;
  m_dpr = dpr;
}

void KaSplashScene::advance(double phase, double dt) {
  m_phase = KaSplashPalette::clamp01(phase);
  if (dt > 0.0) m_clock += dt;
}

bool KaSplashScene::pointerMoved(const QPointF&, bool) {
  return false;
}

void KaSplashScene::pointerLeft() {}

double KaSplashScene::revealedFraction() const {
  return KaSplashPalette::easeOut(m_phase);
}
