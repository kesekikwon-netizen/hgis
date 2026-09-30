#include "KaDrawingNorthArrow.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QPolygonF>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSvgGenerator>
#include <cmath>

#include <qgsapplication.h>
#include <qgslayoutitempicture.h>

namespace {
const QColor kInk(17, 24, 39);

// Glyph outline centred in box. Outlines keep the SVG independent of fonts.
void drawGlyph(QPainter& p, const QRectF& box, const QString& text, int pixelSize,
               const QColor& color) {
  QFont font(QStringLiteral("Malgun Gothic"));
  font.setPixelSize(pixelSize);
  font.setBold(true);
  QPainterPath path;
  path.addText(0.0, 0.0, font, text);
  const QRectF bounds = path.boundingRect();
  if (bounds.isEmpty()) return;
  path.translate(box.center() - bounds.center());
  p.save();
  p.setPen(Qt::NoPen);
  p.setBrush(color);
  p.drawPath(path);
  p.restore();
}

QString northDir() {
  QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  if (dir.isEmpty()) dir = QDir::tempPath();
  dir = QDir(dir).filePath(QStringLiteral("north"));
  if (!QDir().mkpath(dir)) dir = QDir::tempPath();
  return dir;
}
}  // namespace

namespace KaDrawingNorth {

int kindFromRel(const QString& rel) {
  if (rel.contains(QLatin1String("WindRose"), Qt::CaseInsensitive) ||
      rel.contains(QLatin1String("rose"), Qt::CaseInsensitive))
    return 3;
  if (rel.contains(QLatin1String("compass"), Qt::CaseInsensitive) ||
      rel.contains(QLatin1String("NorthArrow_04")) || rel.contains(QLatin1String("NorthArrow_03")))
    return 2;
  if (rel.contains(QLatin1String("NorthArrow")) || rel.contains(QLatin1String("arrow"), Qt::CaseInsensitive))
    return 1;
  return 0;
}

void paintMark(QPainter& p, int kind) {
  const QString n = QStringLiteral("N");
  if (kind == 0) {
    QPolygonF tri;
    tri << QPointF(36, 10) << QPointF(48, 38) << QPointF(36, 32) << QPointF(24, 38);
    p.setPen(Qt::NoPen);
    p.setBrush(kInk);
    p.drawPolygon(tri);
    drawGlyph(p, QRectF(8, 40, 56, 26), n, 21, kInk);
  } else if (kind == 1) {
    // Survey style: N above a half-filled needle (left dark, right white).
    drawGlyph(p, QRectF(8, 2, 56, 16), n, 17, kInk);
    const QPointF tip(36, 20);
    const QPointF tail(36, 58);
    const double halfW = 6.5;
    QPainterPath leftP;
    leftP.moveTo(tip);
    leftP.lineTo(tail.x() - halfW, 64);
    leftP.lineTo(tail);
    leftP.closeSubpath();
    QPainterPath rightP;
    rightP.moveTo(tip);
    rightP.lineTo(tail.x() + halfW, 64);
    rightP.lineTo(tail);
    rightP.closeSubpath();
    p.setPen(QPen(kInk, 1.0));
    p.setBrush(kInk);
    p.drawPath(leftP);
    p.setBrush(Qt::white);
    p.drawPath(rightP);
  } else if (kind == 2) {
    p.setPen(QPen(kInk, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(36, 40), 20, 20);
    p.drawEllipse(QPointF(36, 40), 12, 12);
    for (int i = 0; i < 8; ++i) {
      const double a = static_cast<double>(i) * 3.141592653589793 / 4.0;
      p.drawLine(QPointF(36.0 + 12.0 * std::cos(a), 40.0 + 12.0 * std::sin(a)),
                 QPointF(36.0 + 20.0 * std::cos(a), 40.0 + 20.0 * std::sin(a)));
    }
    QPolygonF needle;
    needle << QPointF(36, 8) << QPointF(42, 22) << QPointF(36, 18) << QPointF(30, 22);
    p.setPen(Qt::NoPen);
    p.setBrush(kInk);
    p.drawPolygon(needle);
    drawGlyph(p, QRectF(24, 26, 24, 16), n, 15, kInk);
  } else {
    p.setPen(Qt::NoPen);
    p.setBrush(kInk);
    QPolygonF major;
    major << QPointF(36, 8) << QPointF(40, 36) << QPointF(36, 64) << QPointF(32, 36);
    p.drawPolygon(major);
    QPolygonF minor;
    minor << QPointF(10, 38) << QPointF(36, 42) << QPointF(62, 38) << QPointF(36, 34);
    p.setBrush(QColor(55, 65, 81));
    p.drawPolygon(minor);
    p.setBrush(kInk);
    QPolygonF diag;
    diag << QPointF(18, 18) << QPointF(36, 40) << QPointF(54, 18) << QPointF(36, 34);
    p.drawPolygon(diag);
    drawGlyph(p, QRectF(28, 10, 16, 12), n, 11, Qt::white);
  }
}

QString writeSvg(int kind) {
  const QString path = QDir::fromNativeSeparators(
      QDir(northDir()).filePath(QStringLiteral("ka-hgis-north-%1.svg").arg(kind)));
  {
    QSvgGenerator generator;
    generator.setFileName(path);
    generator.setSize(QSize(72, 72));
    generator.setViewBox(QRectF(0, 0, 72, 72));
    generator.setTitle(QStringLiteral("North arrow (grid north)"));
    QPainter painter;
    if (!painter.begin(&generator)) return {};
    painter.setRenderHint(QPainter::Antialiasing, true);
    paintMark(painter, kind);
    painter.end();
  }
  const QFileInfo info(path);
  return info.exists() && info.size() > 0 ? path : QString();
}

QIcon previewIcon(int kind) {
  QPixmap pm(72, 72);
  pm.fill(QColor(255, 255, 255));
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(QPen(kInk, 1));
  p.setBrush(Qt::NoBrush);
  p.drawRoundedRect(QRectF(1, 1, 70, 70), 6, 6);
  paintMark(p, kind);
  p.end();
  return QIcon(pm);
}

QString qgisArrowSvg() {
  const QString rel = QStringLiteral("arrows/NorthArrow_02.svg");
  const QStringList roots = QgsApplication::svgPaths();
  for (const QString& root : roots) {
    const QString path = QDir::fromNativeSeparators(QDir(root).absoluteFilePath(rel));
    if (QFile::exists(path)) return path;
  }
  return {};
}

bool pictureNeedsRebuild(QgsLayoutItem* item) {
  auto* pic = dynamic_cast<QgsLayoutItemPicture*>(item);
  if (!pic) return false;
  if (pic->isMissingImage()) return true;
  const QString path = pic->evaluatedPath();
  if (path.isEmpty() || path.startsWith(QLatin1String(":/"))) return true;
  return !QFileInfo::exists(path);
}

int generatedKind(const QgsLayoutItem* item) {
  const auto* pic = dynamic_cast<const QgsLayoutItemPicture*>(item);
  if (!pic) return -1;
  static const QRegularExpression name(QStringLiteral(R"(ka-hgis-north-([0-3])\.(svg|png)$)"),
                                       QRegularExpression::CaseInsensitiveOption);
  const QRegularExpressionMatch match = name.match(QFileInfo(pic->picturePath()).fileName());
  return match.hasMatch() ? match.captured(1).toInt() : -1;
}

QString relForKind(int kind) {
  switch (kind) {
    case 1: return QStringLiteral("arrows/NorthArrow_02.svg");
    case 2: return QStringLiteral("arrows/NorthArrow_04.svg");
    case 3: return QStringLiteral("wind_roses/WindRose_01.svg");
    default: return QString();
  }
}

bool isLegacyRaster(const QgsLayoutItem* item) {
  const auto* pic = dynamic_cast<const QgsLayoutItemPicture*>(item);
  return pic && pic->mode() == Qgis::PictureFormat::Raster && generatedKind(item) >= 0;
}

QString fallbackLabel() { return QStringLiteral("N\n↑\n도북"); }

QString gridNorthNote() {
  return QStringLiteral("방위표는 좌표계의 북쪽(도북)을 가리킵니다. 진북·자북과는 조금 다를 수 있습니다.");
}

}  // namespace KaDrawingNorth
