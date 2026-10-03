// Outline glyphs for tabs, the home page, panels, chips and the map overlay.
// 64-unit grid, glyphs inside 12..52, one ink, no fills but dots (KaIconsOutline.h).
#include "KaIconsOutline.h"

#include <QHash>
#include <QPainterPath>
#include <QPolygonF>

namespace KaIconsOutline {
namespace {

using KaIconMetrics::kOutlineThin;

// 홈: roof, walls and a door.
void oHome(QPainter& p) {
  stroke(p);
  p.drawPolyline(QPolygonF{QPointF(12, 32), QPointF(32, 13), QPointF(52, 32)});
  p.drawPolyline(QPolygonF{QPointF(18, 28), QPointF(18, 50), QPointF(46, 50), QPointF(46, 28)});
  p.drawPolyline(QPolygonF{QPointF(28, 50), QPointF(28, 38), QPointF(36, 38), QPointF(36, 50)});
}

// 지도: a folded map at the other angle from old_map, with a pin dot.
void oMap(QPainter& p) {
  stroke(p);
  p.drawPolygon(QPolygonF{QPointF(12, 20), QPointF(25, 14), QPointF(39, 20), QPointF(52, 14), QPointF(52, 44),
                          QPointF(39, 50), QPointF(25, 44), QPointF(12, 50)});
  stroke(p, kOutlineThin);
  p.drawLine(QPointF(25, 14), QPointF(25, 44));
  p.drawLine(QPointF(39, 20), QPointF(39, 50));
  dot(p, QPointF(45.5, 31), 3.0);
}

// 찾기: a magnifier.
void oSearch(QPainter& p) {
  stroke(p);
  p.drawEllipse(QPointF(27, 27), 12, 12);
  p.drawLine(QPointF(36, 36), QPointF(50, 50));
}

// 실행취소: an arc that ends in an open arrow head on the left.
void oUndo(QPainter& p) {
  stroke(p);
  QPainterPath arc;
  arc.moveTo(17, 28);
  arc.cubicTo(20, 12, 52, 10, 51, 36);
  arc.cubicTo(50, 47, 40, 52, 30, 51);
  p.drawPath(arc);
  p.drawPolyline(QPolygonF{QPointF(27, 17), QPointF(17, 28), QPointF(29, 36)});
}

// 다시실행: the mirror of undo.
void oRedo(QPainter& p) {
  p.translate(64, 0);
  p.scale(-1, 1);
  oUndo(p);
}

// 체크
void oCheck(QPainter& p) {
  stroke(p, 4.0);
  p.drawPolyline(QPolygonF{QPointF(14, 33), QPointF(27, 46), QPointF(50, 19)});
}

// 주의: a triangle with a bang.
void oWarn(QPainter& p) {
  stroke(p);
  p.drawPolygon(QPolygonF{QPointF(32, 13), QPointF(52, 49), QPointF(12, 49)});
  p.drawLine(QPointF(32, 26), QPointF(32, 37));
  dot(p, QPointF(32, 43), 2.4);
}

// 원본 없음: a circle with a cross.
void oMissing(QPainter& p) {
  stroke(p);
  p.drawEllipse(QPointF(32, 32), 19, 19);
  p.drawLine(QPointF(24, 24), QPointF(40, 40));
  p.drawLine(QPointF(40, 24), QPointF(24, 40));
}

// 조사 썸네일 대체: a survey area over cadastral lines.
void oSurveyThumb(QPainter& p) {
  stroke(p, kOutlineThin);
  p.drawLine(QPointF(12, 26), QPointF(52, 22));
  p.drawLine(QPointF(12, 42), QPointF(52, 38));
  p.drawLine(QPointF(28, 12), QPointF(26, 52));
  p.drawLine(QPointF(44, 12), QPointF(46, 52));
  stroke(p);
  p.drawPolygon(QPolygonF{QPointF(20, 20), QPointF(42, 16), QPointF(50, 34), QPointF(36, 48), QPointF(16, 40)});
}

// 시계
void oClock(QPainter& p) {
  stroke(p);
  p.drawEllipse(QPointF(32, 32), 18, 18);
  p.drawPolyline(QPolygonF{QPointF(32, 20), QPointF(32, 32), QPointF(41, 37)});
}

// 폴더: closed, with a tab.
void oFolder(QPainter& p) {
  stroke(p);
  p.drawRoundedRect(QRectF(12, 20, 40, 30), 3, 3);
  p.drawPolyline(QPolygonF{QPointF(12, 20), QPointF(16, 14), QPointF(28, 14), QPointF(32, 20)});
}

// 자물쇠
void oLock(QPainter& p) {
  stroke(p);
  p.drawRoundedRect(QRectF(17, 30, 30, 22), 3, 3);
  QPainterPath shackle;
  shackle.moveTo(23, 30);
  shackle.lineTo(23, 23);
  shackle.arcTo(QRectF(23, 14, 18, 18), 180, -180);
  shackle.lineTo(41, 30);
  p.drawPath(shackle);
  dot(p, QPointF(32, 41), 2.4);
}

// 조사카드: a card with two lines.
void oNote(QPainter& p) {
  stroke(p);
  p.drawRoundedRect(QRectF(14, 14, 36, 36), 4, 4);
  stroke(p, kOutlineThin);
  p.drawLine(QPointF(22, 27), QPointF(42, 27));
  p.drawLine(QPointF(22, 37), QPointF(36, 37));
}

// 연필: body, band and tip at 45 degrees.
void oPencil(QPainter& p) {
  stroke(p);
  p.translate(32, 32);
  p.rotate(-45);
  p.drawRoundedRect(QRectF(-16, -5, 26, 10), 2, 2);
  p.drawPolyline(QPolygonF{QPointF(-16, -5), QPointF(-22, 0), QPointF(-16, 5)});
  stroke(p, kOutlineThin);
  p.drawLine(QPointF(4, -5), QPointF(4, 5));
}

// 꺾쇠
void oChevronLeft(QPainter& p) {
  stroke(p);
  p.drawPolyline(QPolygonF{QPointF(39, 14), QPointF(22, 32), QPointF(39, 50)});
}

void oChevronRight(QPainter& p) {
  stroke(p);
  p.drawPolyline(QPolygonF{QPointF(25, 14), QPointF(42, 32), QPointF(25, 50)});
}

// 자석: a horseshoe with its pole caps.
void oSnap(QPainter& p) {
  stroke(p);
  QPainterPath magnet;
  magnet.moveTo(18, 14);
  magnet.lineTo(18, 34);
  magnet.quadTo(18, 50, 32, 50);
  magnet.quadTo(46, 50, 46, 34);
  magnet.lineTo(46, 14);
  p.drawPath(magnet);
  stroke(p, kOutlineThin);
  p.drawLine(QPointF(14, 22), QPointF(22, 22));
  p.drawLine(QPointF(42, 22), QPointF(50, 22));
}

// 레이어: one closed plate over two open ones.
void oLayer(QPainter& p) {
  stroke(p);
  p.drawPolygon(QPolygonF{QPointF(32, 13), QPointF(49, 21), QPointF(32, 29), QPointF(15, 21)});
  p.drawPolyline(QPolygonF{QPointF(15, 31), QPointF(32, 39), QPointF(49, 31)});
  p.drawPolyline(QPolygonF{QPointF(15, 41), QPointF(32, 49), QPointF(49, 41)});
}

// 위성: body, arms and two panels, with a signal arc below.
void oSatellite(QPainter& p) {
  stroke(p);
  p.save();
  p.translate(32, 29);
  p.rotate(-35);
  p.drawRoundedRect(QRectF(-7, -7, 14, 14), 2, 2);
  p.drawLine(QPointF(-11, 0), QPointF(-7, 0));
  p.drawLine(QPointF(7, 0), QPointF(11, 0));
  p.drawRect(QRectF(-21, -5, 10, 10));
  p.drawRect(QRectF(11, -5, 10, 10));
  p.restore();
  stroke(p, kOutlineThin);
  p.drawArc(QRectF(21, 40, 22, 14), 200 * 16, 140 * 16);
}

// 속성 탭: three bulleted lines.
void oList(QPainter& p) {
  stroke(p);
  for (qreal y : {19.0, 32.0, 45.0}) {
    dot(p, QPointF(16, y), 2.6);
    p.drawLine(QPointF(24, y), QPointF(50, y));
  }
}

// 스타일 탭: a brush.
void oBrush(QPainter& p) {
  stroke(p);
  p.drawLine(QPointF(46, 14), QPointF(28, 34));
  QPainterPath head;
  head.moveTo(25, 30);
  head.quadTo(13, 35, 15, 50);
  head.quadTo(30, 51, 32, 38);
  head.closeSubpath();
  p.drawPath(head);
}

}  // namespace

Glyph outlineGlyphUi(const QString& id) {
  static const QHash<QString, Glyph> glyphs = {
      {QStringLiteral("home"), oHome},
      {QStringLiteral("map"), oMap},
      {QStringLiteral("vworld_base"), oMap},
      {QStringLiteral("vworld_hybrid"), oMap},
      {QStringLiteral("hybrid"), oMap},
      {QStringLiteral("search"), oSearch},
      {QStringLiteral("undo"), oUndo},
      {QStringLiteral("redo"), oRedo},
      {QStringLiteral("check"), oCheck},
      {QStringLiteral("saveedit"), oCheck},
      {QStringLiteral("layout_activate_done"), oCheck},
      {QStringLiteral("warn"), oWarn},
      {QStringLiteral("missing"), oMissing},
      {QStringLiteral("survey_thumb"), oSurveyThumb},
      {QStringLiteral("clock"), oClock},
      {QStringLiteral("folder"), oFolder},
      {QStringLiteral("lock"), oLock},
      {QStringLiteral("note"), oNote},
      {QStringLiteral("pencil"), oPencil},
      {QStringLiteral("chevron_left"), oChevronLeft},
      {QStringLiteral("chevron_right"), oChevronRight},
      {QStringLiteral("snap"), oSnap},
      {QStringLiteral("layer"), oLayer},
      {QStringLiteral("satellite"), oSatellite},
      {QStringLiteral("vworld_sat"), oSatellite},
      {QStringLiteral("list"), oList},
      {QStringLiteral("brush"), oBrush},
  };
  return glyphs.value(id, nullptr);
}

}  // namespace KaIconsOutline
