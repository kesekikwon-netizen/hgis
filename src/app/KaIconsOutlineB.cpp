// Outline glyphs for the 배경 지도 · 정합 · 내보내기 · 기타 ribbon groups.
// 64-unit grid, glyphs inside 12..52, one ink, no fills but dots (KaIconsOutline.h).
#include "KaIconsOutline.h"

#include <QHash>
#include <QPainterPath>
#include <QPolygonF>

namespace KaIconsOutline {
namespace {

using KaIconMetrics::kOutlineThin;

// 지형: two nested closed contours, off-centre, with the summit as a dot.
void oContour(QPainter& p) {
  stroke(p);
  QPainterPath outer;
  outer.moveTo(13, 40);
  outer.cubicTo(12, 26, 27, 14, 42, 15);
  outer.cubicTo(54, 16, 55, 36, 45, 45);
  outer.cubicTo(36, 52, 15, 52, 13, 40);
  p.drawPath(outer);
  QPainterPath middle;
  middle.moveTo(22, 39);
  middle.cubicTo(21, 29, 32, 22, 41, 23);
  middle.cubicTo(49, 25, 49, 36, 42, 40);
  middle.cubicTo(35, 44, 23, 45, 22, 39);
  p.drawPath(middle);
  dot(p, QPointF(37, 31), 2.6);
}

// DEM: a two-peak ridge closed on its base line.
void oDem(QPainter& p) {
  stroke(p);
  p.drawPolygon(QPolygonF{QPointF(12, 50), QPointF(24, 24), QPointF(31, 34), QPointF(40, 15), QPointF(52, 50)});
  stroke(p, kOutlineThin);
  p.drawLine(QPointF(36, 24), QPointF(44, 24));
}

// 등고선: one open contour and four survey points.
void oSurveyContour(QPainter& p) {
  stroke(p);
  QPainterPath curve;
  curve.moveTo(12, 42);
  curve.cubicTo(22, 14, 40, 50, 52, 20);
  p.drawPath(curve);
  for (const QPointF& v : {QPointF(17, 19), QPointF(45, 14), QPointF(24, 51), QPointF(50, 46)}) dot(p, v, 2.9);
}

// 토양: three horizon lines, gently waved.
void oSoil(QPainter& p) {
  stroke(p);
  for (qreal y : {20.0, 32.0, 44.0}) {
    QPainterPath horizon;
    horizon.moveTo(12, y);
    horizon.cubicTo(24, y - 5, 40, y + 5, 52, y);
    p.drawPath(horizon);
  }
}

// 고지형: stepped terraces with the dashed scar of an old channel above them.
void oPaleo(QPainter& p) {
  stroke(p);
  p.drawPolygon(QPolygonF{QPointF(12, 51), QPointF(12, 27), QPointF(24, 27), QPointF(24, 35), QPointF(38, 35),
                          QPointF(38, 43), QPointF(52, 43), QPointF(52, 51)});
  stroke(p, kOutlineThin, Qt::DashLine);
  QPainterPath former;
  former.moveTo(13, 19);
  former.cubicTo(26, 12, 30, 24, 44, 17);
  former.cubicTo(48, 15, 50, 17, 52, 20);
  p.drawPath(former);
}

// 지질: dipping beds offset across one fault, inside a frame.
void oGeology(QPainter& p) {
  stroke(p);
  const QRectF frame(12, 14, 40, 36);
  p.drawRoundedRect(frame, 2, 2);
  p.save();
  p.setClipRect(frame.adjusted(1, 1, -1, -1));
  stroke(p, kOutlineThin);
  for (qreal y : {28.0, 38.0, 48.0}) p.drawLine(QPointF(12, y), QPointF(31, y - 10));
  for (qreal y : {34.0, 44.0, 54.0}) p.drawLine(QPointF(33, y), QPointF(52, y - 10));
  stroke(p);
  p.drawLine(QPointF(35, 12), QPointF(30, 52));
  p.restore();
}

// 수계: the main stream drawn heavier than its tributary.
void oRiver(QPainter& p) {
  stroke(p, 4.4);
  QPainterPath main;
  main.moveTo(26, 12);
  main.cubicTo(40, 20, 16, 28, 28, 38);
  main.cubicTo(40, 46, 30, 50, 34, 52);
  p.drawPath(main);
  stroke(p, kOutlineThin);
  QPainterPath tributary;
  tributary.moveTo(50, 16);
  tributary.cubicTo(42, 20, 46, 32, 28, 38);
  p.drawPath(tributary);
}

// 옛 지도: a three-leaf folded map.
void oOldMap(QPainter& p) {
  stroke(p);
  p.drawPolygon(QPolygonF{QPointF(12, 18), QPointF(25, 22), QPointF(38, 18), QPointF(52, 22), QPointF(52, 50),
                          QPointF(38, 46), QPointF(25, 50), QPointF(12, 46)});
  stroke(p, kOutlineThin);
  p.drawLine(QPointF(25, 22), QPointF(25, 50));
  p.drawLine(QPointF(38, 18), QPointF(38, 46));
}

// 정합: a target, ring and cross.
void oGeoref(QPainter& p) {
  stroke(p);
  p.drawEllipse(QPointF(32, 32), 15, 15);
  p.drawLine(QPointF(32, 12), QPointF(32, 21));
  p.drawLine(QPointF(32, 43), QPointF(32, 52));
  p.drawLine(QPointF(12, 32), QPointF(21, 32));
  p.drawLine(QPointF(43, 32), QPointF(52, 32));
  dot(p, QPointF(32, 32), 2.6);
}

// 도면: a page with a folded ear and two text lines.
void oPdf(QPainter& p) {
  stroke(p);
  p.drawPolygon(QPolygonF{QPointF(17, 12), QPointF(38, 12), QPointF(48, 22), QPointF(48, 52), QPointF(17, 52)});
  p.drawPolyline(QPolygonF{QPointF(38, 12), QPointF(38, 22), QPointF(48, 22)});
  stroke(p, kOutlineThin);
  p.drawLine(QPointF(24, 33), QPointF(41, 33));
  p.drawLine(QPointF(24, 41), QPointF(41, 41));
}

// 인쇄: paper above, body, the print coming out below.
void oPrint(QPainter& p) {
  stroke(p);
  p.drawPolyline(QPolygonF{QPointF(22, 22), QPointF(22, 12), QPointF(42, 12), QPointF(42, 22)});
  p.drawRoundedRect(QRectF(12, 22, 40, 20), 3, 3);
  p.drawPolyline(QPolygonF{QPointF(22, 42), QPointF(22, 52), QPointF(42, 52), QPointF(42, 42)});
  stroke(p, kOutlineThin);
  p.drawLine(QPointF(27, 47), QPointF(37, 47));
}

// 단면도: a profile over a dashed datum with one depth line.
void oSection(QPainter& p) {
  stroke(p);
  p.drawPolyline(QPolygonF{QPointF(12, 42), QPointF(20, 26), QPointF(28, 34), QPointF(38, 17), QPointF(52, 30)});
  stroke(p, kOutlineThin, Qt::DashLine);
  p.drawLine(QPointF(12, 50), QPointF(52, 50));
  stroke(p, kOutlineThin);
  p.drawLine(QPointF(38, 17), QPointF(38, 50));
}

// GeoTIFF: a framed picture with two hills and a sun.
void oGeotiff(QPainter& p) {
  stroke(p);
  const QRectF frame(12, 14, 40, 36);
  p.drawRoundedRect(frame, 3, 3);
  p.save();
  p.setClipRect(frame.adjusted(1, 1, -1, -1));
  p.drawPolyline(QPolygonF{QPointF(13, 47), QPointF(24, 31), QPointF(32, 39), QPointF(40, 29), QPointF(51, 47)});
  p.restore();
  dot(p, QPointF(42, 23), 3.0);
}

// 검수·제출: two arrows chasing each other.
void oExportConvert(QPainter& p) {
  stroke(p);
  const QRectF ring(14, 14, 36, 36);
  QPainterPath top;
  top.arcMoveTo(ring, 150);
  top.arcTo(ring, 150, -120);
  p.drawPath(top);
  QPainterPath bottom;
  bottom.arcMoveTo(ring, 330);
  bottom.arcTo(ring, 330, -120);
  p.drawPath(bottom);
  p.drawPolyline(QPolygonF{QPointF(41, 17), QPointF(47.6, 23), QPointF(40, 26)});
  p.drawPolyline(QPolygonF{QPointF(23, 47), QPointF(16.4, 41), QPointF(24, 38)});
}

// 더보기: three dots. The only glyph made of dots alone, so they are the largest dots
// in the set: 4.6 units keeps three of them visible (>= 40 px) at 32 px.
void oMore(QPainter& p) {
  stroke(p);
  dot(p, QPointF(16.6, 32), 4.6);
  dot(p, QPointF(32, 32), 4.6);
  dot(p, QPointF(47.4, 32), 4.6);
}

}  // namespace

Glyph outlineGlyphB(const QString& id) {
  static const QHash<QString, Glyph> glyphs = {
      {QStringLiteral("contour"), oContour},
      {QStringLiteral("vworld_contour"), oContour},
      {QStringLiteral("dem"), oDem},
      {QStringLiteral("hillshade"), oDem},
      {QStringLiteral("survey_contour"), oSurveyContour},
      {QStringLiteral("soil"), oSoil},
      {QStringLiteral("paleo"), oPaleo},
      {QStringLiteral("paleo_landform"), oPaleo},
      {QStringLiteral("geology"), oGeology},
      {QStringLiteral("river"), oRiver},
      {QStringLiteral("hydro"), oRiver},
      {QStringLiteral("old_map"), oOldMap},
      {QStringLiteral("georef"), oGeoref},
      {QStringLiteral("pdf"), oPdf},
      {QStringLiteral("print"), oPrint},
      {QStringLiteral("section"), oSection},
      {QStringLiteral("section_layout"), oSection},
      {QStringLiteral("geotiff"), oGeotiff},
      {QStringLiteral("export_convert"), oExportConvert},
      {QStringLiteral("transform"), oExportConvert},
      {QStringLiteral("more"), oMore},
  };
  return glyphs.value(id, nullptr);
}

}  // namespace KaIconsOutline
