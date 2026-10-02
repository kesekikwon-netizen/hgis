// Outline glyphs for the 조사 · 기록 · 자료 받기 ribbon groups and the draw tools.
// 64-unit grid, glyphs inside 12..52, one ink, no fills but dots (KaIconsOutline.h).
#include "KaIconsOutline.h"

#include <QHash>
#include <QPainterPath>
#include <QPolygonF>

namespace KaIconsOutline {
namespace {

using KaIconMetrics::kOutlineThin;

// 새 조사: a page with a plus.
void oNew(QPainter& p) {
  stroke(p);
  p.drawRoundedRect(QRectF(18, 12, 28, 38), 4, 4);
  p.drawLine(QPointF(32, 24), QPointF(32, 40));
  p.drawLine(QPointF(24, 32), QPointF(40, 32));
}

// 열기: an open folder, the flap crease as a second line.
void oOpen(QPainter& p) {
  stroke(p);
  QPainterPath path;
  path.moveTo(12, 24);
  path.lineTo(12, 20);
  path.quadTo(12, 16, 16, 16);
  path.lineTo(28, 16);
  path.lineTo(32, 22);
  path.lineTo(48, 22);
  path.quadTo(52, 22, 52, 26);
  path.lineTo(52, 46);
  path.quadTo(52, 50, 48, 50);
  path.lineTo(16, 50);
  path.quadTo(12, 50, 12, 46);
  path.closeSubpath();
  p.drawPath(path);
  p.drawLine(QPointF(12, 30), QPointF(52, 30));
}

// 저장: a disk with the label tab above and the slot below.
void oSave(QPainter& p) {
  stroke(p);
  p.drawRoundedRect(QRectF(16, 14, 32, 36), 3, 3);
  p.drawPolyline(QPolygonF{QPointF(23, 14), QPointF(23, 24), QPointF(41, 24), QPointF(41, 14)});
  p.drawPolyline(QPolygonF{QPointF(25, 50), QPointF(25, 38), QPointF(39, 38), QPointF(39, 50)});
}

// 다른 이름: the disk with its lower right corner cut away for a pencil.
void oSaveAs(QPainter& p) {
  p.save();
  QPainterPath keep;
  keep.addRect(QRectF(0, 0, 64, 64));
  QPainterPath cut;
  cut.addEllipse(QPointF(46, 46), 12, 12);
  p.setClipPath(keep.subtracted(cut));
  oSave(p);
  p.restore();
  stroke(p, kOutlineThin);
  p.translate(46, 46);
  p.rotate(-45);
  p.drawRoundedRect(QRectF(-7, -3, 14, 6), 1.5, 1.5);
  p.drawPolyline(QPolygonF{QPointF(-7, -3), QPointF(-11, 0), QPointF(-7, 3)});
}

// 선택: the arrow cursor.
void oSelect(QPainter& p) {
  stroke(p);
  p.drawPolygon(QPolygonF{QPointF(19, 12), QPointF(19, 47), QPointF(28, 38), QPointF(37, 52), QPointF(44, 48),
                          QPointF(33, 33), QPointF(46, 33)});
}

// 측거: a tilted tape with four ticks.
void oMeasure(QPainter& p) {
  stroke(p);
  p.translate(32, 32);
  p.rotate(-45);
  p.drawRoundedRect(QRectF(-20, -7, 40, 14), 3, 3);
  stroke(p, kOutlineThin);
  for (qreal x : {-12.0, -4.0, 4.0, 12.0})
    p.drawLine(QPointF(x, -7), QPointF(x, x == -4.0 || x == 12.0 ? -3.0 : -1.0));
}

// 그리기: a pentagon with two vertex handles.
void oDrawPoly(QPainter& p) {
  stroke(p);
  p.drawPolygon(QPolygonF{QPointF(16, 46), QPointF(19, 16), QPointF(45, 13), QPointF(52, 36), QPointF(34, 52)});
  dot(p, QPointF(19, 16), 3.4);
  dot(p, QPointF(52, 36), 3.4);
}

// 그리기 · 선: three joined segments with vertex dots.
void oDrawLine(QPainter& p) {
  stroke(p);
  p.drawPolyline(QPolygonF{QPointF(14, 46), QPointF(26, 16), QPointF(40, 36), QPointF(50, 18)});
  for (const QPointF& v : {QPointF(14, 46), QPointF(26, 16), QPointF(40, 36), QPointF(50, 18)}) dot(p, v, 3.2);
}

// 그리기 · 면: a rounded square.
void oDrawArea(QPainter& p) {
  stroke(p);
  p.drawRoundedRect(QRectF(14, 16, 32, 32), 4, 4);
}

// 쉽게 그리기: a freehand polyline with its dots.
void oEasyDraw(QPainter& p) {
  stroke(p);
  p.drawPolyline(QPolygonF{QPointF(12, 46), QPointF(22, 30), QPointF(34, 38), QPointF(50, 16)});
  for (const QPointF& v : {QPointF(12, 46), QPointF(22, 30), QPointF(34, 38), QPointF(50, 16)}) dot(p, v, 3.2);
}

// 기준점: a map pin.
void oGps(QPainter& p) {
  stroke(p);
  QPainterPath pin;
  pin.moveTo(32, 52);
  pin.cubicTo(16, 36, 16, 20, 32, 14);
  pin.cubicTo(48, 20, 48, 36, 32, 52);
  p.drawPath(pin);
  p.drawEllipse(QPointF(32, 28), 5, 5);
}

// 유물 위치: a triangle.
void oArtifact(QPainter& p) {
  stroke(p);
  p.drawPolygon(QPolygonF{QPointF(32, 14), QPointF(51, 49), QPointF(13, 49)});
}

// 시굴격자: a 3 x 2 grid.
void oTrenchGrid(QPainter& p) {
  stroke(p);
  p.drawRoundedRect(QRectF(12, 18, 40, 28), 2, 2);
  stroke(p, kOutlineThin);
  p.drawLine(QPointF(25.3, 18), QPointF(25.3, 46));
  p.drawLine(QPointF(38.7, 18), QPointF(38.7, 46));
  p.drawLine(QPointF(12, 32), QPointF(52, 32));
}

// 버퍼: two rings around a point, the outer one dashed.
void oBuffer(QPainter& p) {
  stroke(p, kOutlineThin, Qt::DashLine);
  p.drawRoundedRect(QRectF(13, 13, 38, 38), 10, 10);
  stroke(p);
  p.drawRoundedRect(QRectF(22, 22, 20, 20), 6, 6);
  dot(p, QPointF(32, 32), 3.2);
}

// 지적: a block split into three parcels, one of them marked.
void oCadastral(QPainter& p) {
  stroke(p);
  p.drawPolygon(QPolygonF{QPointF(12, 16), QPointF(52, 13), QPointF(50, 51), QPointF(14, 49)});
  stroke(p, kOutlineThin);
  p.drawLine(QPointF(31, 14.6), QPointF(30, 32));
  p.drawLine(QPointF(13, 32), QPointF(51, 30.6));
  dot(p, QPointF(41, 23), 3.0);
}

// 수치지형: a sheet with two contours and a download arrow beside it.
void oTopoDownload(QPainter& p) {
  stroke(p);
  p.drawRoundedRect(QRectF(12, 13, 26, 38), 3, 3);
  stroke(p, kOutlineThin);
  p.drawEllipse(QPointF(25, 32), 8, 6.5);
  p.drawEllipse(QPointF(25, 32), 3.4, 2.6);
  stroke(p);
  p.drawLine(QPointF(46, 20), QPointF(46, 50));
  p.drawPolyline(QPolygonF{QPointF(40, 43), QPointF(46, 50), QPointF(52, 43)});
}

// 유산: a pavilion, the eaves curved.
void oHeritage(QPainter& p) {
  stroke(p);
  QPainterPath roof;
  roof.moveTo(12, 26);
  roof.quadTo(20, 26, 23, 15);
  roof.lineTo(41, 15);
  roof.quadTo(44, 26, 52, 26);
  p.drawPath(roof);
  p.drawLine(QPointF(21, 26), QPointF(21, 46));
  p.drawLine(QPointF(43, 26), QPointF(43, 46));
  p.drawLine(QPointF(15, 50), QPointF(49, 50));
}

}  // namespace

Glyph outlineGlyphA(const QString& id) {
  static const QHash<QString, Glyph> glyphs = {
      {QStringLiteral("new"), oNew},
      {QStringLiteral("open"), oOpen},
      {QStringLiteral("import"), oOpen},
      {QStringLiteral("save"), oSave},
      {QStringLiteral("save_unsaved"), oSave},
      {QStringLiteral("save_as"), oSaveAs},
      {QStringLiteral("select"), oSelect},
      {QStringLiteral("arrow"), oSelect},
      {QStringLiteral("measure"), oMeasure},
      {QStringLiteral("tape"), oMeasure},
      {QStringLiteral("draw_poly"), oDrawPoly},
      {QStringLiteral("feature_poly"), oDrawPoly},
      {QStringLiteral("polygon"), oDrawPoly},
      {QStringLiteral("survey_area"), oDrawPoly},
      {QStringLiteral("draw_line"), oDrawLine},
      {QStringLiteral("line"), oDrawLine},
      {QStringLiteral("feature_line"), oDrawLine},
      {QStringLiteral("draw_area"), oDrawArea},
      {QStringLiteral("easy_draw"), oEasyDraw},
      {QStringLiteral("gps"), oGps},
      {QStringLiteral("artifact"), oArtifact},
      {QStringLiteral("trench_grid"), oTrenchGrid},
      {QStringLiteral("trench"), oTrenchGrid},
      {QStringLiteral("buffer"), oBuffer},
      {QStringLiteral("cadastral"), oCadastral},
      {QStringLiteral("vworld_cadastral"), oCadastral},
      {QStringLiteral("topo_download"), oTopoDownload},
      {QStringLiteral("heritage"), oHeritage},
  };
  return glyphs.value(id, nullptr);
}

}  // namespace KaIconsOutline
