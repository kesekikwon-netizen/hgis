#include "KaIcons.h"
#include "KaTheme.h"
#include <QFont>
#include <QHash>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QScopedValueRollback>

namespace {

thread_local QColor tInk;
thread_local QColor tAccent;
thread_local QIcon::Mode tMode = QIcon::Normal;
thread_local QIcon::State tState = QIcon::Off;
thread_local bool tGlossyTile = true;
// 저장·도면·인쇄처럼 자주 누르는 단추만 진한 색 타일. 나머지는 옅은 타일이라 지도가 먼저 보인다.
thread_local bool tStrongTile = false;

// t 만큼 b 쪽으로 섞는다.
QColor blend(const QColor& a, const QColor& b, qreal t) {
  return QColor::fromRgbF(float(a.redF() * (1.0 - t) + b.redF() * t), float(a.greenF() * (1.0 - t) + b.greenF() * t),
                          float(a.blueF() * (1.0 - t) + b.blueF() * t));
}

bool lightTile() { return tGlossyTile && !tStrongTile; }

QColor stateColor(const QColor& color) {
  if (tMode == QIcon::Disabled) {
    const int gray = qGray(KaTheme::iconPalette().disabled.rgb());
    return QColor(gray, gray, gray);
  }
  QColor result = tState == QIcon::On ? color.darker(115) : color;
  if (tMode == QIcon::Active) result = result.lighter(120);
  return result;
}

QColor groupColor(const QString& id) {
  const auto& palette = KaTheme::iconPalette();
  if (id == QLatin1String("new") || id == QLatin1String("open") ||
      id == QLatin1String("import") || id == QLatin1String("save") ||
      id == QLatin1String("save_as")) return palette.file;
  if (id.startsWith(QLatin1String("layout_")) || id == QLatin1String("pdf") ||
      id == QLatin1String("print") || id == QLatin1String("geotiff") ||
      id == QLatin1String("export_convert") ||
      id == QLatin1String("export") || id == QLatin1String("upload") ||
      id == QLatin1String("check") || id == QLatin1String("section") ||
      id == QLatin1String("section_layout")) return palette.output;
  // Tile colour follows the ribbon group: 버퍼 sits in 기록, 유산 in 자료 받기, and
  // 토양·지질·수계 in 배경 지도. Their glyphs keep water, earth and rock fills.
  if (id == QLatin1String("georef") || id == QLatin1String("transform") ||
      id == QLatin1String("crs")) return palette.align;
  if (id == QLatin1String("polygon") || id == QLatin1String("survey_area") ||
      id.startsWith(QLatin1String("feature_")) || id.startsWith(QLatin1String("draw_")) ||
      id == QLatin1String("line") || id == QLatin1String("gps") ||
      id == QLatin1String("measure") || id == QLatin1String("tape") ||
      id == QLatin1String("artifact") || id == QLatin1String("buffer") ||
      id == QLatin1String("trench") || id == QLatin1String("trench_grid") ||
      id == QLatin1String("easy_draw") || id == QLatin1String("saveedit") ||
      id == QLatin1String("snap") || id == QLatin1String("select") ||
      id == QLatin1String("arrow") || id == QLatin1String("stop") ||
      id == QLatin1String("undo") || id == QLatin1String("redo") ||
      id == QLatin1String("trash")) return palette.record;
  return palette.map;
}

QPixmap base(int s = 64) {
  QPixmap pm(s, s);
  pm.fill(Qt::transparent);
  return pm;
}

void prep(QPainter& p, qreal width = 3.0) {
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(QPen(tInk, qMax(3.0, width), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  if (!tGlossyTile) p.setBrush(tAccent.lighter(150));
  else if (tMode == QIcon::Disabled) p.setBrush(QColor(240, 240, 240));
  else p.setBrush(tStrongTile ? QColor(0xF1, 0xF5, 0xF7) : QColor(Qt::white));
}

void fillInk(QPainter& p) { p.setBrush(tGlossyTile ? tInk : tAccent); }

QIcon bakeIcon(void (*fn)(QPainter&), const QColor& accent) {
  auto pmAt = [&](int size, QIcon::Mode mode, QIcon::State state) {
    QScopedValueRollback<QIcon::Mode> modeGuard(tMode, mode);
    QScopedValueRollback<QIcon::State> stateGuard(tState, state);
    QScopedValueRollback<QColor> accentGuard(tAccent, stateColor(accent));
    // 옅은 타일에서는 그림 선을 묶음 색의 짙은 톤으로 그려 색으로 묶음을 알아보게 한다.
    const QColor charcoal = stateColor(KaTheme::iconPalette().ink);
    QScopedValueRollback<QColor> inkGuard(tInk, lightTile() ? blend(tAccent, charcoal, 0.45) : charcoal);
    auto pm = base(size);
    QPainter p(&pm);
    p.scale(size / 64.0, size / 64.0);
    p.setRenderHint(QPainter::Antialiasing, true);
    if (tGlossyTile) {
      // Flat tile: one fill and a hairline edge, no reflection band.
      const QRectF tile(2, 2, 60, 60);
      QPainterPath outline;
      outline.addRoundedRect(tile, 14, 14);
      p.fillPath(outline, tStrongTile ? tAccent : blend(Qt::white, tAccent, 0.17));
      p.setBrush(Qt::NoBrush);
      p.setPen(tStrongTile ? QPen(tAccent.darker(112), 0.8) : QPen(blend(Qt::white, tAccent, 0.45), 1.2));
      p.drawPath(outline);
    }
    p.save();
    if (tGlossyTile) { p.translate(5.1, 5.1); p.scale(0.84, 0.84); }
    fn(p);
    p.restore();
    if (mode == QIcon::Selected) {
      p.setRenderHint(QPainter::Antialiasing, true);
      p.setPen(QPen(KaTheme::iconPalette().selected, 2.5));
      p.setBrush(Qt::NoBrush);
      p.drawRoundedRect(QRectF(3, 3, 58, 58), 9, 9);
    }
    if (state == QIcon::On) {
      p.setPen(QPen(tInk, 2.0));
      p.setBrush(tAccent);
      p.drawEllipse(QPointF(51, 51), 9, 9);
      p.setPen(QPen(mode == QIcon::Disabled ? tAccent.lighter(175) : QColor(Qt::white),
                    2.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
      p.setBrush(Qt::NoBrush);
      p.drawLine(QPointF(46, 51), QPointF(50, 55));
      p.drawLine(QPointF(50, 55), QPointF(56, 47));
    }
    return pm;
  };
  QIcon ic;
  for (int size : {64, 128})
    for (const auto mode : {QIcon::Normal, QIcon::Active, QIcon::Selected, QIcon::Disabled})
      for (const auto state : {QIcon::Off, QIcon::On})
        ic.addPixmap(pmAt(size, mode, state), mode, state);
  return ic;
}

void dDocPlus(QPainter& p) {
  prep(p, 3.0);
  p.drawRoundedRect(QRectF(18, 12, 28, 38), 4, 4);
  p.drawLine(QPointF(32, 24), QPointF(32, 40));
  p.drawLine(QPointF(24, 32), QPointF(40, 32));
}

void dFolder(QPainter& p) {
  prep(p, 3.0);
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
}

void dSave(QPainter& p) {
  prep(p, 3.0);
  p.drawRoundedRect(QRectF(16, 14, 32, 36), 3, 3);
  p.drawRect(QRectF(22, 14, 20, 12));
  p.drawRect(QRectF(24, 34, 16, 12));
}

// 다른 이름으로 저장: 저장 그림에 연필을 얹어 「저장」과 구별한다.
void dSaveAs(QPainter& p) {
  dSave(p);
  p.save();
  p.translate(44, 42);
  p.rotate(-45);
  p.setPen(QPen(tInk, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.setBrush(tAccent);
  p.drawRect(QRectF(-9, -4.5, 20, 9));
  QPolygonF tip;
  tip << QPointF(-9, -4.5) << QPointF(-16, 0) << QPointF(-9, 4.5);
  p.setBrush(Qt::white);
  p.drawPolygon(tip);
  p.restore();
}

void dLayer(QPainter& p) {
  prep(p, 2.4);
  auto plate = [](qreal y) {
    QPolygonF a;
    a << QPointF(32, y) << QPointF(48, y + 7) << QPointF(32, y + 14) << QPointF(16, y + 7);
    return a;
  };
  p.drawPolygon(plate(16));
  p.drawPolygon(plate(26));
  p.drawPolygon(plate(36));
}

void dMap(QPainter& p) {
  prep(p, 2.6);
  p.drawRoundedRect(QRectF(14, 14, 36, 36), 4, 4);
  p.drawLine(26, 16, 26, 48);
  p.drawLine(38, 16, 38, 48);
  p.drawLine(16, 26, 48, 26);
  p.drawLine(16, 38, 48, 38);
  fillInk(p);
  p.drawEllipse(QPointF(34, 30), 3.5, 3.5);
}

void dSatellite(QPainter& p) {
  prep(p, 2.5);
  p.drawEllipse(QPointF(32, 32), 16, 16);
  p.drawEllipse(QPointF(32, 32), 7, 16);
  p.drawLine(16, 32, 48, 32);
}

void dPolygon(QPainter& p) {
  prep(p, 3.0);
  QPolygonF poly;
  poly << QPointF(18, 42) << QPointF(22, 16) << QPointF(44, 16) << QPointF(50, 34) << QPointF(34, 50);
  p.drawPolygon(poly);
}

void dLine(QPainter& p) {
  prep(p, 3.2);
  p.drawLine(16, 46, 28, 18);
  p.drawLine(28, 18, 50, 36);
  fillInk(p);
  p.drawEllipse(QPointF(16, 46), 3.2, 3.2);
  p.drawEllipse(QPointF(28, 18), 3.2, 3.2);
  p.drawEllipse(QPointF(50, 36), 3.2, 3.2);
}

void dGps(QPainter& p) {
  prep(p, 3.0);
  QPainterPath pin;
  pin.moveTo(32, 50);
  pin.cubicTo(16, 34, 16, 20, 32, 16);
  pin.cubicTo(48, 20, 48, 34, 32, 50);
  p.drawPath(pin);
  p.drawEllipse(QPointF(32, 28), 5, 5);
}

void dCheck(QPainter& p) {
  prep(p, 7.0);
  p.setBrush(Qt::NoBrush);
  QPainterPath check;
  check.moveTo(16, 34);
  check.lineTo(28, 46);
  check.lineTo(50, 18);
  p.drawPath(check);
  p.setPen(QPen(tAccent, 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.drawPath(check);
}

void dExport(QPainter& p) {
  prep(p, 3.2);
  p.drawLine(32, 14, 32, 38);
  p.drawLine(22, 28, 32, 40);
  p.drawLine(42, 28, 32, 40);
  p.drawRoundedRect(QRectF(13, 44, 38, 10), 2, 2);
}

void dPdf(QPainter& p) {
  prep(p, 2.5);
  QPainterPath page;
  page.moveTo(20, 12);
  page.lineTo(38, 12);
  page.lineTo(46, 20);
  page.lineTo(46, 52);
  page.lineTo(20, 52);
  page.closeSubpath();
  p.drawPath(page);
  p.drawLine(38, 12, 38, 20);
  p.drawLine(38, 20, 46, 20);
}

// 프린터: 위로 나온 용지, 몸통, 아래로 나오는 인쇄물.
void dPrint(QPainter& p) {
  prep(p, 2.5);
  p.drawRect(QRectF(20, 10, 24, 14));
  QPainterPath body;
  body.moveTo(20, 42);
  body.lineTo(12, 42);
  body.lineTo(12, 24);
  body.lineTo(52, 24);
  body.lineTo(52, 42);
  body.lineTo(44, 42);
  p.drawPath(body);
  p.drawRect(QRectF(20, 34, 24, 20));
  p.drawLine(QPointF(25, 42), QPointF(39, 42));
  p.drawLine(QPointF(25, 48), QPointF(35, 48));
}

void dTerrain3d(QPainter& p) {
  prep(p, 2.5);
  QPolygonF ridge;
  ridge << QPointF(10, 48) << QPointF(20, 30) << QPointF(28, 38) << QPointF(40, 14)
        << QPointF(54, 44);
  p.drawPolyline(ridge);
  p.drawLine(QPointF(10, 50), QPointF(54, 50));
  p.drawLine(QPointF(48, 10), QPointF(48, 22));
  fillInk(p);
  QPolygonF n;
  n << QPointF(48, 8) << QPointF(44.5, 16) << QPointF(51.5, 16);
  p.drawPolygon(n);
}

void dSection(QPainter& p) {
  prep(p, 2.6);
  p.drawRoundedRect(QRectF(12, 16, 40, 32), 3, 3);
  p.drawLine(QPointF(16, 40), QPointF(20, 28));
  p.drawLine(QPointF(20, 28), QPointF(28, 34));
  p.drawLine(QPointF(28, 34), QPointF(36, 22));
  p.drawLine(QPointF(36, 22), QPointF(48, 30));
  p.setPen(QPen(tAccent.darker(130), 3.0, Qt::DashLine, Qt::RoundCap));
  p.drawLine(QPointF(16, 42), QPointF(48, 42));
}

void dCrs(QPainter& p) {
  prep(p, 2.6);
  p.drawEllipse(QPointF(32, 32), 16, 16);
  p.drawEllipse(QPointF(32, 32), 8, 16);
  p.drawLine(16, 32, 48, 32);
  p.drawLine(32, 16, 32, 48);
}

void dTransform(QPainter& p) {
  prep(p, 3.0);
  p.drawLine(14, 22, 42, 22);
  p.drawLine(34, 14, 44, 22);
  p.drawLine(34, 30, 44, 22);
  p.drawLine(50, 42, 22, 42);
  p.drawLine(32, 34, 20, 42);
  p.drawLine(32, 50, 20, 42);
}

void dUpload(QPainter& p) {
  prep(p, 3.2);
  p.drawLine(32, 44, 32, 16);
  p.drawLine(20, 28, 32, 14);
  p.drawLine(44, 28, 32, 14);
  p.drawRoundedRect(QRectF(13, 47, 38, 7), 2, 2);
}

void dTrash(QPainter& p) {
  prep(p, 2.8);
  p.drawLine(18, 20, 46, 20);
  p.drawLine(26, 16, 38, 16);
  p.drawRoundedRect(QRectF(20, 20, 24, 28), 2, 2);
  p.drawLine(28, 26, 28, 40);
  p.drawLine(36, 26, 36, 40);
}

void dGeoref(QPainter& p) {
  // Two overlapping sheets share a surveyed control point.
  prep(p, 3.2);
  p.drawRoundedRect(QRectF(8, 10, 32, 34), 3, 3);
  p.setBrush(tAccent.lighter(185));
  p.drawRoundedRect(QRectF(23, 23, 32, 32), 3, 3);
  p.setPen(QPen(tAccent, 3.2, Qt::SolidLine, Qt::RoundCap));
  p.drawLine(12, 20, 24, 20);
  p.drawLine(12, 27, 19, 27);
  p.setPen(QPen(tInk, 3.2, Qt::SolidLine, Qt::RoundCap));
  p.setBrush(tAccent);
  p.drawEllipse(QPointF(36, 36), 6, 6);
  p.drawLine(36, 25, 36, 30);
  p.drawLine(36, 42, 36, 48);
  p.drawLine(25, 36, 30, 36);
  p.drawLine(42, 36, 48, 36);
}

void dPalette(QPainter& p) {
  prep(p, 2.4);
  p.drawRoundedRect(QRectF(16, 18, 10, 10), 2, 2);
  p.drawLine(32, 23, 48, 23);
  p.drawRoundedRect(QRectF(16, 36, 10, 10), 2, 2);
  p.drawLine(32, 41, 48, 41);
}

void dStop(QPainter& p) {
  prep(p, 3.0);
  p.drawRoundedRect(QRectF(20, 20, 24, 24), 3, 3);
}

void dHelp(QPainter& p) {
  prep(p, 2.5);
  p.drawEllipse(QPointF(32, 32), 16, 16);
  p.drawArc(QRectF(24, 20, 16, 16), 40 * 16, 200 * 16);
  fillInk(p);
  p.drawEllipse(QPointF(32, 42), 1.8, 1.8);
}

void dMore(QPainter& p) {
  fillInk(p);
  p.setPen(Qt::NoPen);
  p.drawEllipse(QPointF(16, 32), 3.2, 3.2);
  p.drawEllipse(QPointF(32, 32), 3.2, 3.2);
  p.drawEllipse(QPointF(48, 32), 3.2, 3.2);
}

void dSearch(QPainter& p) {
  prep(p, 2.6);
  p.drawEllipse(QPointF(28, 28), 12, 12);
  p.drawLine(QPointF(37, 37), QPointF(48, 48));
}

void dSelect(QPainter& p) {
  prep(p, 2.5);
  QPolygonF a;
  a << QPointF(20, 14) << QPointF(20, 46) << QPointF(28, 36) << QPointF(38, 50) << QPointF(44, 46)
    << QPointF(32, 32) << QPointF(44, 32);
  p.drawPolygon(a);
}

// 지적: 반듯한 격자가 아니라 크기가 제각각인 필지. 한 필지를 칠해 「지번 찾기」 느낌을 준다.
void dCadastral(QPainter& p) {
  prep(p, 2.4);
  QPolygonF block;
  block << QPointF(12, 16) << QPointF(52, 13) << QPointF(50, 51) << QPointF(14, 49);
  p.drawPolygon(block);
  QPolygonF picked;
  picked << QPointF(40, 30.6) << QPointF(51.2, 29) << QPointF(50, 51) << QPointF(35, 50.2);
  p.setBrush(tAccent);
  p.drawPolygon(picked);
  p.drawLine(QPointF(31, 14.6), QPointF(30, 32));
  p.drawLine(QPointF(13, 32), QPointF(30, 32));
  p.drawLine(QPointF(30, 32), QPointF(40, 30.6));
}

// 대동여지도: 병풍처럼 접는 분첩 지도와 산줄기.
void dFoldedMap(QPainter& p) {
  prep(p, 2.4);
  const QColor paper = stateColor(KaTheme::iconPalette().earthLight).lighter(125);
  QPolygonF left, middle, right;
  left << QPointF(8, 17) << QPointF(24, 21) << QPointF(24, 51) << QPointF(8, 47);
  middle << QPointF(24, 21) << QPointF(40, 17) << QPointF(40, 47) << QPointF(24, 51);
  right << QPointF(40, 17) << QPointF(56, 21) << QPointF(56, 51) << QPointF(40, 47);
  p.setBrush(paper);
  p.drawPolygon(left);
  p.drawPolygon(right);
  p.setBrush(paper.darker(112));
  p.drawPolygon(middle);
  QPolygonF ridge;
  ridge << QPointF(11, 39) << QPointF(16, 31) << QPointF(20, 37) << QPointF(26, 28) << QPointF(31, 36)
        << QPointF(36, 30) << QPointF(42, 37) << QPointF(47, 29) << QPointF(53, 36);
  p.setBrush(Qt::NoBrush);
  p.setPen(QPen(tInk, 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.drawPolyline(ridge);
}

// 1919 조선지형도: 모서리가 접힌 누런 옛 도엽과 등고선.
void dOldSheet(QPainter& p) {
  prep(p, 2.4);
  QPainterPath sheet;
  sheet.moveTo(12, 12);
  sheet.lineTo(42, 12);
  sheet.lineTo(52, 22);
  sheet.lineTo(52, 52);
  sheet.lineTo(12, 52);
  sheet.closeSubpath();
  p.setBrush(stateColor(KaTheme::iconPalette().earthLight).lighter(125));
  p.drawPath(sheet);
  p.setBrush(Qt::NoBrush);
  p.drawLine(QPointF(42, 12), QPointF(42, 22));
  p.drawLine(QPointF(42, 22), QPointF(52, 22));
  p.drawArc(QRectF(16, 28, 32, 26), 20 * 16, 140 * 16);
  p.drawArc(QRectF(23, 35, 18, 16), 20 * 16, 140 * 16);
}

// 수치지형도 받기: 등고선이 닫힌 도엽과 내려받기 화살표.
void dTopoDownload(QPainter& p) {
  prep(p, 2.4);
  p.drawRoundedRect(QRectF(9, 12, 32, 40), 3, 3);
  p.setBrush(Qt::NoBrush);
  p.drawEllipse(QPointF(25, 32), 10, 8.5);
  p.drawEllipse(QPointF(25, 32), 4, 3.5);
  p.setPen(QPen(tInk, 3.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.drawLine(QPointF(50, 20), QPointF(50, 46));
  p.drawLine(QPointF(43, 39), QPointF(50, 47));
  p.drawLine(QPointF(57, 39), QPointF(50, 47));
}

// GeoTIFF: 좌표 눈금이 붙은 지도 그림.
void dGeoImage(QPainter& p) {
  prep(p, 2.4);
  const QRectF frame(13, 12, 40, 34);
  p.drawRoundedRect(frame, 3, 3);
  p.save();
  p.setClipRect(frame.adjusted(1.5, 1.5, -1.5, -1.5));
  QPolygonF hills;
  hills << QPointF(12, 47) << QPointF(25, 29) << QPointF(33, 39) << QPointF(41, 31) << QPointF(55, 47);
  p.setBrush(tAccent);
  p.drawPolygon(hills);
  p.restore();
  fillInk(p);
  p.drawEllipse(QPointF(43, 21), 3.5, 3.5);
  p.setBrush(Qt::NoBrush);
  for (qreal x : {23.0, 33.0, 43.0}) p.drawLine(QPointF(x, 50), QPointF(x, 55));
  for (qreal y : {22.0, 34.0}) p.drawLine(QPointF(4, y), QPointF(9, y));
}

// 웹: 누리집 창과 지구.
void dBrowser(QPainter& p) {
  prep(p, 2.4);
  p.drawRoundedRect(QRectF(8, 12, 48, 40), 4, 4);
  p.drawLine(QPointF(8, 21), QPointF(56, 21));
  p.setBrush(Qt::NoBrush);
  p.drawEllipse(QPointF(32, 37), 10.5, 10.5);
  p.drawEllipse(QPointF(32, 37), 4.5, 10.5);
  p.drawLine(QPointF(22, 37), QPointF(42, 37));
  fillInk(p);
  p.setPen(Qt::NoPen);
  for (qreal x : {14.0, 20.0, 26.0}) p.drawEllipse(QPointF(x, 16.5), 1.9, 1.9);
}

// 국가유산: 처마가 들린 기와지붕 건물.
void dPavilion(QPainter& p) {
  prep(p, 2.6);
  p.drawRect(QRectF(13, 46, 38, 6));
  p.drawLine(QPointF(21, 28), QPointF(21, 46));
  p.drawLine(QPointF(43, 28), QPointF(43, 46));
  p.drawLine(QPointF(18, 35), QPointF(46, 35));
  QPainterPath roof;
  roof.moveTo(6, 25);
  roof.quadTo(16, 27, 22, 15);
  roof.lineTo(42, 15);
  roof.quadTo(48, 27, 58, 25);
  roof.quadTo(32, 34, 6, 25);
  p.setBrush(tAccent);
  p.drawPath(roof);
}

// 지형: 위에서 본 언덕. 한쪽으로 치우친 닫힌 등고선 세 겹과 꼭대기 삼각점.
// 두 호만 있으면 무선 신호로, 동심원이면 과녁으로 읽혔다.
void dContour(QPainter& p) {
  prep(p, 2.6);
  p.setBrush(Qt::NoBrush);
  QPainterPath outer;
  outer.moveTo(8, 42);
  outer.cubicTo(6, 24, 26, 10, 44, 12);
  outer.cubicTo(58, 14, 60, 38, 46, 48);
  outer.cubicTo(34, 56, 10, 56, 8, 42);
  p.drawPath(outer);
  QPainterPath middle;
  middle.moveTo(20, 40);
  middle.cubicTo(18, 28, 32, 19, 42, 21);
  middle.cubicTo(51, 23, 51, 37, 42, 41);
  middle.cubicTo(34, 45, 21, 47, 20, 40);
  p.drawPath(middle);
  QPainterPath inner;
  inner.moveTo(32, 33);
  inner.cubicTo(32, 27, 40, 25, 43, 29);
  inner.cubicTo(45, 33, 40, 36, 36, 36);
  inner.cubicTo(33, 36, 32, 35, 32, 33);
  p.drawPath(inner);
  QPolygonF summit;
  summit << QPointF(38, 27.5) << QPointF(41.5, 33) << QPointF(34.5, 33);
  p.setPen(Qt::NoPen);
  p.setBrush(tAccent.darker(125));
  p.drawPolygon(summit);
}

// 측량 등고선: 열린 등고선 세 줄. 닫힌 언덕(지형) 그림과 구분한다.
void dSurveyContour(QPainter& p) {
  prep(p, 2.4);
  p.setBrush(Qt::NoBrush);
  p.drawArc(QRectF(4, 16, 48, 22), 16 * 20, 16 * 150);
  p.drawArc(QRectF(8, 28, 40, 18), 16 * 20, 16 * 150);
  p.drawArc(QRectF(14, 38, 30, 14), 16 * 20, 16 * 150);
}

void dDark(QPainter& p) {
  prep(p, 2.6);
  p.drawEllipse(QRectF(16, 16, 32, 32));
  p.drawArc(QRectF(24, 14, 26, 28), 40 * 16, 200 * 16);
}

void dToolPoly(QPainter& p) {
  prep(p, 2.5);
  QPolygonF poly;
  poly << QPointF(18, 44) << QPointF(20, 18) << QPointF(44, 16) << QPointF(48, 36) << QPointF(32, 48);
  p.drawPolygon(poly);
}

void dToolLine(QPainter& p) {
  prep(p, 3.0);
  p.drawLine(14, 44, 26, 16);
  p.drawLine(26, 16, 46, 34);
  fillInk(p);
  p.drawEllipse(QPointF(14, 44), 3, 3);
  p.drawEllipse(QPointF(26, 16), 3, 3);
  p.drawEllipse(QPointF(46, 34), 3, 3);
}

void dToolArea(QPainter& p) {
  prep(p, 2.8);
  p.drawRoundedRect(QRectF(14, 16, 28, 28), 3, 3);
}

void dArtifact(QPainter& p) {
  prep(p, 2.6);
  QPolygonF tri;
  tri << QPointF(32, 14) << QPointF(50, 48) << QPointF(14, 48);
  p.drawPolygon(tri);
}

void dSnap(QPainter& p) {
  prep(p, 2.8);
  QPainterPath mag;
  mag.moveTo(18, 16);
  mag.lineTo(18, 36);
  mag.quadTo(18, 50, 32, 50);
  mag.quadTo(46, 50, 46, 36);
  mag.lineTo(46, 16);
  p.drawPath(mag);
  p.drawLine(QPointF(14, 16), QPointF(22, 16));
  p.drawLine(QPointF(42, 16), QPointF(50, 16));
  fillInk(p);
  p.drawEllipse(QPointF(18, 14), 2.4, 2.4);
  p.drawEllipse(QPointF(46, 14), 2.4, 2.4);
}

void dLayoutFrame(QPainter& p) {
  prep(p, 2.6);
  p.drawRoundedRect(QRectF(16, 12, 32, 40), 3, 3);
  p.setPen(QPen(tInk, 2.2, Qt::DashLine, Qt::RoundCap));
  p.drawRect(QRectF(20, 18, 24, 20));
}

void dLayoutSelect(QPainter& p) {
  prep(p, 2.2);
  p.setPen(QPen(tInk, 2.2, Qt::DashLine));
  p.drawRect(QRectF(16, 16, 28, 24));
  p.setPen(QPen(tInk, 2.4));
  p.drawRect(QRectF(13, 13, 7, 7));
  p.drawRect(QRectF(40, 13, 7, 7));
  p.drawRect(QRectF(13, 36, 7, 7));
  p.drawRect(QRectF(40, 36, 7, 7));
}

void dLayoutPan(QPainter& p) {
  prep(p, 2.8);
  p.drawLine(32, 14, 32, 50);
  p.drawLine(14, 32, 50, 32);
  p.drawLine(32, 14, 26, 22);
  p.drawLine(32, 14, 38, 22);
  p.drawLine(32, 50, 26, 42);
  p.drawLine(32, 50, 38, 42);
  p.drawLine(14, 32, 22, 26);
  p.drawLine(14, 32, 22, 38);
  p.drawLine(50, 32, 42, 26);
  p.drawLine(50, 32, 42, 38);
}

void dLayoutZoom(QPainter& p) {
  prep(p, 2.6);
  p.drawRoundedRect(QRectF(18, 16, 28, 32), 3, 3);
  p.drawLine(12, 12, 20, 12);
  p.drawLine(12, 12, 12, 20);
  p.drawLine(52, 12, 44, 12);
  p.drawLine(52, 12, 52, 20);
  p.drawLine(12, 52, 12, 44);
  p.drawLine(12, 52, 20, 52);
  p.drawLine(52, 52, 52, 44);
  p.drawLine(52, 52, 44, 52);
}

void dNorth(QPainter& p) {
  prep(p, 2.6);
  QPolygonF up;
  up << QPointF(32, 10) << QPointF(44, 38) << QPointF(32, 30) << QPointF(20, 38);
  p.drawPolygon(up);
  p.setFont(QFont(QStringLiteral("Malgun Gothic"), 11, QFont::Bold));
  p.drawText(QRectF(18, 40, 28, 16), Qt::AlignCenter, QStringLiteral("N"));
}

void dDem(QPainter& p) {
  // One shaded peak in the group green; the ridge line marks it as elevation data.
  prep(p, 2.8);
  const auto& colors = KaTheme::iconPalette();
  QPolygonF peak;
  peak << QPointF(7, 50) << QPointF(26, 20) << QPointF(33, 29) << QPointF(41, 13)
       << QPointF(57, 50);
  p.setBrush(stateColor(colors.map));
  p.drawPolygon(peak);
  QPolygonF shade;
  shade << QPointF(41, 13) << QPointF(57, 50) << QPointF(40, 50) << QPointF(43, 31);
  p.setBrush(stateColor(colors.map).darker(135));
  p.drawPolygon(shade);
  p.setBrush(Qt::NoBrush);
  p.drawLine(QPointF(17, 41), QPointF(47, 41));
}

void dMapGrid(QPainter& p) {
  prep(p, 2.2);
  for (int i = 0; i < 4; ++i) {
    const qreal x = 16 + i * 10;
    p.drawLine(QPointF(x, 14), QPointF(x, 50));
    p.drawLine(QPointF(14, x + 2), QPointF(50, x + 2));
  }
}

void dTrenchGrid(QPainter& p) {
  prep(p, 2.4);
  p.drawRect(QRectF(14, 16, 14, 32));
  p.drawRect(QRectF(36, 16, 14, 32));
}

void dPaleo(QPainter& p) {
  // Stepped river terraces in one sand tone with the dashed scar of an old channel.
  prep(p, 2.8);
  const auto& colors = KaTheme::iconPalette();
  QPolygonF terraces;
  terraces << QPointF(8, 52) << QPointF(8, 20) << QPointF(22, 20) << QPointF(22, 31)
           << QPointF(38, 31) << QPointF(38, 41) << QPointF(56, 41) << QPointF(56, 52);
  p.setBrush(stateColor(colors.earthLight));
  p.drawPolygon(terraces);
  QPainterPath former;
  former.moveTo(12, 15);
  former.cubicTo(30, 6, 26, 24, 44, 18);
  former.cubicTo(52, 15, 54, 26, 56, 30);
  p.setBrush(Qt::NoBrush);
  p.setPen(QPen(tInk, 2.8, Qt::DashLine, Qt::RoundCap));
  p.drawPath(former);
}

void dSoil(QPainter& p) {
  // Soil profile: three horizons in one earth hue, light to dark.
  prep(p, 2.8);
  const auto& colors = KaTheme::iconPalette();
  const QColor earth = stateColor(colors.earth);
  p.setBrush(earth.lighter(150));
  p.drawRect(QRectF(10, 12, 44, 13));
  p.setBrush(earth);
  p.drawRect(QRectF(10, 25, 44, 13));
  p.setBrush(earth.darker(140));
  p.drawRect(QRectF(10, 38, 44, 14));
}

void dGeology(QPainter& p) {
  // Dipping beds offset across one fault, in warm stone tones only.
  prep(p, 2.8);
  const auto& colors = KaTheme::iconPalette();
  const QColor rock = stateColor(colors.rock);
  p.setBrush(rock.lighter(165));
  p.drawRect(QRectF(8, 12, 48, 42));
  p.save();
  p.setClipRect(QRectF(8, 12, 48, 42));
  QPolygonF leftBed;
  leftBed << QPointF(8, 34) << QPointF(34, 19) << QPointF(31, 33) << QPointF(8, 47);
  p.setBrush(rock);
  p.drawPolygon(leftBed);
  QPolygonF rightBed;
  rightBed << QPointF(34, 31) << QPointF(56, 18) << QPointF(56, 31) << QPointF(31, 46);
  p.drawPolygon(rightBed);
  p.restore();
  p.setBrush(Qt::NoBrush);
  p.drawRect(QRectF(8, 12, 48, 42));
  p.setPen(QPen(tInk, 3.4, Qt::SolidLine, Qt::RoundCap));
  p.drawLine(QPointF(37, 9), QPointF(28, 57));
}

void dRiver(QPainter& p) {
  // Main stream wider than its tributary, water blue inside a thin bank line.
  prep(p, 2.8);
  p.setBrush(Qt::NoBrush);
  QPainterPath main;
  main.moveTo(26, 8);
  main.cubicTo(40, 18, 15, 27, 27, 38);
  main.cubicTo(39, 47, 28, 51, 33, 57);
  QPainterPath trib;
  trib.moveTo(54, 15);
  trib.cubicTo(43, 19, 47, 33, 27, 38);
  p.setPen(QPen(tInk, 6.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.drawPath(trib);
  p.setPen(QPen(tInk, 8.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.drawPath(main);
  const QColor water = stateColor(KaTheme::iconPalette().water);
  p.setPen(QPen(water, 3.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.drawPath(trib);
  p.setPen(QPen(water, 5.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.drawPath(main);
}

void dMeasureTape(QPainter& p) {
  prep(p, 2.6);
  p.drawRoundedRect(QRectF(12, 22, 40, 16), 3, 3);
  p.drawLine(QPointF(18, 22), QPointF(18, 30));
  p.drawLine(QPointF(26, 22), QPointF(26, 28));
  p.drawLine(QPointF(34, 22), QPointF(34, 30));
  p.drawLine(QPointF(42, 22), QPointF(42, 28));
  p.drawLine(QPointF(48, 22), QPointF(48, 30));
  p.drawLine(QPointF(16, 42), QPointF(48, 42));
  p.drawLine(QPointF(16, 42), QPointF(22, 48));
  p.drawLine(QPointF(16, 42), QPointF(22, 36));
}

void dScaleBar(QPainter& p) {
  prep(p, 2.4);
  p.drawRect(QRectF(12, 26, 40, 12));
  p.drawLine(22, 26, 22, 38);
  p.drawLine(32, 26, 32, 38);
  p.drawLine(42, 26, 42, 38);
}

void dScaleText(QPainter& p) {
  prep(p, 2.4);
  p.setFont(QFont(QStringLiteral("Malgun Gothic"), 11, QFont::Bold));
  p.drawText(QRectF(6, 16, 52, 32), Qt::AlignCenter, QStringLiteral("1:n"));
}

void dLegend(QPainter& p) {
  prep(p, 2.4);
  p.drawRoundedRect(QRectF(14, 16, 10, 10), 2, 2);
  p.drawRoundedRect(QRectF(14, 30, 10, 10), 2, 2);
  p.drawRoundedRect(QRectF(14, 44, 10, 8), 2, 2);
  p.drawLine(30, 21, 50, 21);
  p.drawLine(30, 35, 50, 35);
  p.drawLine(30, 48, 46, 48);
}

void dActivate(QPainter& p) {
  prep(p, 2.6);
  p.drawRoundedRect(QRectF(14, 14, 36, 36), 4, 4);
  p.drawLine(22, 40, 40, 22);
  fillInk(p);
  p.drawEllipse(QPointF(40, 22), 3, 3);
}

void dCoordPoint(QPainter& p) {
  prep(p, 2.4);
  p.drawRoundedRect(QRectF(12, 12, 28, 20), 2.5, 2.5);
  p.setPen(QPen(tInk, 1.8, Qt::SolidLine, Qt::RoundCap));
  p.drawLine(16, 18, 34, 18);
  p.drawLine(16, 24, 32, 24);
  p.setPen(QPen(tInk, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.drawLine(QPointF(28, 32), QPointF(44, 48));
  QPolygonF head;
  head << QPointF(44, 48) << QPointF(35, 46) << QPointF(42, 39);
  fillInk(p);
  p.drawPolygon(head);
}

void dCenter(QPainter& p) {
  prep(p, 2.6);
  p.drawEllipse(QPointF(32, 32), 16, 16);
  p.drawLine(32, 12, 32, 22);
  p.drawLine(32, 42, 32, 52);
  p.drawLine(12, 32, 22, 32);
  p.drawLine(42, 32, 52, 32);
  fillInk(p);
  p.drawEllipse(QPointF(32, 32), 3, 3);
}

// 버퍼: 유적을 가운데 두고 500 m·1000 m 거리 고리.
void dBuffer(QPainter& p) {
  prep(p, 2.4);
  p.setBrush(Qt::NoBrush);
  p.setPen(QPen(tInk, 2.6, Qt::DashLine, Qt::RoundCap));
  p.drawEllipse(QPointF(32, 32), 22, 22);
  p.drawEllipse(QPointF(32, 32), 13.5, 13.5);
  p.setPen(QPen(tInk, 2.6));
  p.setBrush(tAccent);
  p.drawEllipse(QPointF(32, 32), 5.5, 5.5);
}

void dUndo(QPainter& p) {
  prep(p, 3.2);
  QPainterPath arc;
  arc.moveTo(46, 20);
  arc.arcTo(QRectF(14, 16, 36, 32), 20, 220);
  p.drawPath(arc);
  fillInk(p);
  QPolygonF head;
  head << QPointF(14, 16) << QPointF(16, 34) << QPointF(32, 24);
  p.drawPolygon(head);
}

void dRedo(QPainter& p) {
  prep(p, 3.2);
  QPainterPath arc;
  arc.moveTo(18, 20);
  arc.arcTo(QRectF(14, 16, 36, 32), 160, -220);
  p.drawPath(arc);
  fillInk(p);
  QPolygonF head;
  head << QPointF(50, 16) << QPointF(48, 34) << QPointF(32, 24);
  p.drawPolygon(head);
}

void dEasyDraw(QPainter& p) {
  prep(p, 3.0);
  QPolygonF path;
  path << QPointF(12, 46) << QPointF(22, 30) << QPointF(34, 38) << QPointF(50, 16);
  p.drawPolyline(path);
  fillInk(p);
  p.drawEllipse(QPointF(12, 46), 3.6, 3.6);
  p.drawEllipse(QPointF(22, 30), 3.6, 3.6);
  p.drawEllipse(QPointF(34, 38), 3.6, 3.6);
  p.drawEllipse(QPointF(50, 16), 3.6, 3.6);
}

}  // namespace

namespace KaIcons {

QIcon appIcon() {
  static const QIcon cached(QStringLiteral(":/ka-hgis/app-icon.png"));
  return cached;
}

QIcon icon(const QString& id) {
  static QHash<QString, QIcon> cache;
  const QString cacheKey = id + (!tGlossyTile ? QStringLiteral("/flat")
                                 : tStrongTile ? QStringLiteral("/strong")
                                               : QStringLiteral("/glossy"));
  if (cache.contains(cacheKey)) return cache.value(cacheKey);

  // Solid tiles (저장·도면·인쇄) share the chrome's one blue accent; two dark tile
  // colours in one ribbon row read as two different kinds of button.
  const QColor accent = tStrongTile ? KaTheme::tokens().sky1 : groupColor(id);
  const auto bake = [&accent](void (*draw)(QPainter&)) { return bakeIcon(draw, accent); };
  QIcon ic;
  if (id == QLatin1String("new")) ic = bake(dDocPlus);
  else if (id == QLatin1String("open") || id == QLatin1String("import")) ic = bake(dFolder);
  else if (id == QLatin1String("save")) ic = bake(dSave);
  else if (id == QLatin1String("save_as")) ic = bake(dSaveAs);
  else if (id == QLatin1String("old_map")) ic = bake(dFoldedMap);
  else if (id == QLatin1String("old_topo")) ic = bake(dOldSheet);
  else if (id == QLatin1String("topo_download")) ic = bake(dTopoDownload);
  else if (id == QLatin1String("geotiff")) ic = bake(dGeoImage);
  else if (id == QLatin1String("web")) ic = bake(dBrowser);
  else if (id == QLatin1String("heritage")) ic = bake(dPavilion);
  else if (id == QLatin1String("export_convert")) ic = bake(dTransform);
  else if (id == QLatin1String("layer")) ic = bake(dLayer);
  else if (id == QLatin1String("map") || id == QLatin1String("vworld_base") ||
           id == QLatin1String("vworld_hybrid") || id == QLatin1String("hybrid"))
    ic = bake(dMap);
  else if (id == QLatin1String("satellite") || id == QLatin1String("vworld_sat")) ic = bake(dSatellite);
  else if (id == QLatin1String("vworld_cadastral") || id == QLatin1String("cadastral")) ic = bake(dCadastral);
  else if (id == QLatin1String("vworld_contour") || id == QLatin1String("contour")) ic = bake(dContour);
  else if (id == QLatin1String("survey_contour")) ic = bake(dSurveyContour);
  else if (id == QLatin1String("dark_mode")) ic = bake(dDark);
  else if (id == QLatin1String("polygon") || id == QLatin1String("survey_area")) ic = bake(dPolygon);
  else if (id == QLatin1String("line") || id == QLatin1String("feature_line")) ic = bake(dLine);
  else if (id == QLatin1String("feature_poly") || id == QLatin1String("draw_poly")) ic = bake(dToolPoly);
  else if (id == QLatin1String("gps")) ic = bake(dGps);
  else if (id == QLatin1String("check") || id == QLatin1String("saveedit") ||
           id == QLatin1String("layout_activate_done"))
    ic = bake(dCheck);
  else if (id == QLatin1String("export")) ic = bake(dExport);
  else if (id == QLatin1String("pdf")) ic = bake(dPdf);
  else if (id == QLatin1String("print")) ic = bake(dPrint);
  else if (id == QLatin1String("section") || id == QLatin1String("section_layout")) ic = bake(dSection);
  else if (id == QLatin1String("terrain_3d") || id == QLatin1String("terrain3d")) ic = bake(dTerrain3d);
  else if (id == QLatin1String("crs")) ic = bake(dCrs);
  else if (id == QLatin1String("transform")) ic = bake(dTransform);
  else if (id == QLatin1String("upload")) ic = bake(dUpload);
  else if (id == QLatin1String("trash")) ic = bake(dTrash);
  else if (id == QLatin1String("georef")) ic = bake(dGeoref);
  else if (id == QLatin1String("palette")) ic = bake(dPalette);
  else if (id == QLatin1String("stop")) ic = bake(dStop);
  else if (id == QLatin1String("help")) ic = bake(dHelp);
  else if (id == QLatin1String("more")) ic = bake(dMore);
  else if (id == QLatin1String("search")) ic = bake(dSearch);
  else if (id == QLatin1String("draw_line")) ic = bake(dToolLine);
  else if (id == QLatin1String("draw_area")) ic = bake(dToolArea);
  else if (id == QLatin1String("snap")) ic = bake(dSnap);
  else if (id == QLatin1String("easy_draw")) ic = bake(dEasyDraw);
  else if (id == QLatin1String("undo")) ic = bake(dUndo);
  else if (id == QLatin1String("redo")) ic = bake(dRedo);
  else if (id == QLatin1String("buffer")) ic = bake(dBuffer);
  else if (id == QLatin1String("artifact")) ic = bake(dArtifact);
  else if (id == QLatin1String("select") || id == QLatin1String("arrow")) ic = bake(dSelect);
  else if (id == QLatin1String("measure") || id == QLatin1String("tape")) ic = bake(dMeasureTape);
  else if (id == QLatin1String("dem") || id == QLatin1String("hillshade")) ic = bake(dDem);
  else if (id == QLatin1String("trench_grid") || id == QLatin1String("trench")) ic = bake(dTrenchGrid);
  else if (id == QLatin1String("soil")) ic = bake(dSoil);
  else if (id == QLatin1String("paleo") || id == QLatin1String("paleo_landform")) ic = bake(dPaleo);
  else if (id == QLatin1String("geology")) ic = bake(dGeology);
  else if (id == QLatin1String("river") || id == QLatin1String("hydro")) ic = bake(dRiver);
  else if (id == QLatin1String("map_grid") || id == QLatin1String("graticule")) ic = bake(dMapGrid);
  else if (id == QLatin1String("layout_map_frame")) ic = bake(dLayoutFrame);
  else if (id == QLatin1String("layout_select")) ic = bake(dLayoutSelect);
  else if (id == QLatin1String("layout_pan")) ic = bake(dLayoutPan);
  else if (id == QLatin1String("layout_zoom_full")) ic = bake(dLayoutZoom);
  else if (id == QLatin1String("layout_north")) ic = bake(dNorth);
  else if (id == QLatin1String("layout_scalebar")) ic = bake(dScaleBar);
  else if (id == QLatin1String("layout_scale")) ic = bake(dScaleText);
  else if (id == QLatin1String("layout_legend")) ic = bake(dLegend);
  else if (id == QLatin1String("layout_activate")) ic = bake(dActivate);
  else if (id == QLatin1String("layout_center")) ic = bake(dCenter);
  else if (id == QLatin1String("layout_coord_point")) ic = bake(dCoordPoint);
  else ic = bake(dDocPlus);

  cache.insert(cacheKey, ic);
  return ic;
}

QIcon strongIcon(const QString& id) {
  QScopedValueRollback<bool> tileGuard(tGlossyTile, true);
  QScopedValueRollback<bool> strongGuard(tStrongTile, true);
  return icon(id);
}

QIcon icon(const QString& id, const QColor& ink) {
  // Monochrome utility consumers need the glyph, not a solid tinted tile.
  QScopedValueRollback<bool> tileGuard(tGlossyTile, !ink.isValid());
  const QIcon source = icon(id);
  if (!ink.isValid()) return source;
  QIcon tinted;
  for (const auto mode : {QIcon::Normal, QIcon::Active, QIcon::Selected, QIcon::Disabled}) {
    for (const auto state : {QIcon::Off, QIcon::On}) {
      // Apply disabled opacity once, after deriving the mask from the normal artwork.
      const auto maskMode = mode == QIcon::Disabled ? QIcon::Normal : mode;
      const QImage src = source.pixmap(QSize(64, 64), maskMode, state).toImage();
      QImage out(src.size(), QImage::Format_ARGB32);
      out.fill(Qt::transparent);
      // Preserve internal outlines instead of flattening a filled icon into a silhouette.
      const qreal inkRange = qMax(1, 255 - qGray(KaTheme::iconPalette().ink.rgb()));
      for (int y = 0; y < src.height(); ++y) {
        for (int x = 0; x < src.width(); ++x) {
          const QColor pixel = src.pixelColor(x, y);
          const qreal shade = qMin(1.0, (255 - qGray(pixel.rgb())) / inkRange);
          QColor tint = ink;
          const qreal disabledOpacity = mode == QIcon::Disabled ? 0.45 : 1.0;
          tint.setAlpha(qRound(ink.alphaF() * pixel.alpha() * shade * disabledOpacity));
          out.setPixelColor(x, y, tint);
        }
      }
      tinted.addPixmap(QPixmap::fromImage(out), mode, state);
    }
  }
  return tinted;
}

}  // namespace KaIcons
