#pragma once

#include <QColor>
#include <QRectF>
#include <QString>

class QPainter;
class QPixmap;

// 새 모양 startup notice (design canvas board 4): a paper card whose lower part is a section through
// the ground — five strata with the two notice lines on the bedrock. The app is busy until it is
// ready, so the bedrock, the name and the notices are there from the first frame; once it is ready
// the strata stack up one by one from the bedrock, a pit is dug into the section, the ground line
// is drawn from left to right and a control point lands on it (user 2026-10-04: 지층이 아래부터 한
// 겹씩 쌓이고 → 구덩이 → 지표선·기준점, 시작 화면 내내 천천히). With reduced motion the finished
// picture is shown at once.
namespace KaSplashStrata {

// Timeline, in seconds since the app became ready. Each value runs 0..1 and stays at 1.
constexpr double kTimelineSeconds = 3.4;  // the control point has landed
double stratumReveal(int index, double seconds);  // 0 = bedrock (always there) .. 4 = topsoil
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
