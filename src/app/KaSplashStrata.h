#pragma once

#include <QColor>
#include <QRectF>
#include <QString>

class QPainter;
class QPixmap;

// 새 모양 startup notice (design canvas board 4): a paper card whose lower part is a section through
// the ground — five strata with the two notice lines on the bedrock. The app is busy until it is
// ready, so the strata, the name and the notices are there from the first frame; once it is ready a
// pit appears in the section, the ground line is drawn from left to right and a control point lands
// on it. With reduced motion the finished picture is shown at once.
namespace KaSplashStrata {

// Timeline, in seconds since the app became ready. Each value runs 0..1 and stays at 1.
double pitReveal(double seconds);
double groundLineReveal(double seconds);
double markerReveal(double seconds);

QColor paper();      // the card
QColor bedrock();    // the lowest stratum, under the notice lines
QColor noticeInk();  // the copyright line's ink on the bedrock

struct Frame {
  double seconds = 0.0;   // since readiness; 0 while the app is still starting
  bool still = false;     // reduced motion: the finished picture
  double progress = 0.0;  // 0..1 of the reading interval
  QString status;
  QString version;
};

// Paints the whole card into card (the shadow around it is the caller's).
void paint(QPainter& painter, const QRectF& card, double radius, const QPixmap& icon, const Frame& frame);

}  // namespace KaSplashStrata
