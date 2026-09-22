#pragma once

#include <QRectF>

class QPainter;
class QPixmap;
class QPointF;

// Quiet app-window drawing on the startup notice. No video file. The window
// settles once, then stays still for the rest of the reading interval.
class KaSplashScene {
public:
  void setRect(const QRectF& plan, qreal devicePixelRatio);
  QRectF rect() const { return m_rect; }
  void advance(double phase, double dt);
  bool pointerMoved(const QPointF& pos, bool pressed);
  void pointerLeft();
  void paint(QPainter& painter, double phase, const QPixmap& icon) const;
  double revealedFraction() const;
  double clock() const { return m_clock; }

private:
  QRectF m_rect;
  qreal m_dpr = 1.0;
  double m_phase = 0.0;
  double m_clock = 0.0;
};
