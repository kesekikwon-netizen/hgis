#include "Terrain3dLayoutService.h"

#include "LayoutService.h"
#include "Terrain3dService.h"

#include <QColor>
#include <QGraphicsItem>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QImage>
#include <QObject>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>
#include <QVector>

#include <algorithm>
#include <cmath>

#include <qgis.h>
#include <qgscoordinatereferencesystem.h>
#include <qgslayout.h>
#include <qgslayoutitem.h>
#include <qgslayoutitemlabel.h>
#include <qgslayoutitemmap.h>
#include <qgslayoutitempage.h>
#include <qgslayoutitempicture.h>
#include <qgslayoutitemscalebar.h>
#include <qgslayoutmanager.h>
#include <qgslayoutpagecollection.h>
#include <qgslayoutsize.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgsmasterlayoutinterface.h>
#include <qgspointxy.h>
#include <qgsprintlayout.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#include <qgsfillsymbol.h>
#include <qgssinglesymbolrenderer.h>
#include <qgssymbol.h>
#include <qgstextformat.h>

namespace {

void paintNorthMark(QPainter& p, int kind) {
  const QColor ink(17, 24, 39);
  if (kind == 0) {
    QPolygonF tri;
    tri << QPointF(36, 10) << QPointF(48, 38) << QPointF(36, 32) << QPointF(24, 38);
    p.setPen(Qt::NoPen);
    p.setBrush(ink);
    p.drawPolygon(tri);
    p.setPen(ink);
    p.setFont(QFont(QStringLiteral("Malgun Gothic"), 16, QFont::Bold));
    p.drawText(QRectF(8, 40, 56, 26), Qt::AlignCenter, QStringLiteral("N"));
  } else if (kind == 1) {
    p.setPen(ink);
    p.setFont(QFont(QStringLiteral("Malgun Gothic"), 13, QFont::Bold));
    p.drawText(QRectF(8, 2, 56, 16), Qt::AlignCenter, QStringLiteral("N"));
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
    p.setPen(QPen(ink, 1.0));
    p.setBrush(ink);
    p.drawPath(leftP);
    p.setBrush(Qt::white);
    p.drawPath(rightP);
  } else if (kind == 2) {
    p.setPen(QPen(ink, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(36, 40), 20, 20);
    p.drawEllipse(QPointF(36, 40), 12, 12);
    for (int i = 0; i < 8; ++i) {
      const double a = static_cast<double>(i) * 3.141592653589793 / 4.0;
      p.drawLine(QPointF(36.0 + 12.0 * std::cos(a), 40.0 + 12.0 * std::sin(a)),
                 QPointF(36.0 + 20.0 * std::cos(a), 40.0 + 20.0 * std::sin(a)));
    }
    QPolygonF n;
    n << QPointF(36, 8) << QPointF(42, 22) << QPointF(36, 18) << QPointF(30, 22);
    p.setPen(Qt::NoPen);
    p.setBrush(ink);
    p.drawPolygon(n);
    p.setPen(ink);
    p.setFont(QFont(QStringLiteral("Malgun Gothic"), 11, QFont::Bold));
    p.drawText(QRectF(24, 26, 24, 16), Qt::AlignCenter, QStringLiteral("N"));
  } else {
    p.setPen(Qt::NoPen);
    p.setBrush(ink);
    QPolygonF major;
    major << QPointF(36, 8) << QPointF(40, 36) << QPointF(36, 64) << QPointF(32, 36);
    p.drawPolygon(major);
    QPolygonF minor;
    minor << QPointF(10, 38) << QPointF(36, 42) << QPointF(62, 38) << QPointF(36, 34);
    p.setBrush(QColor(55, 65, 81));
    p.drawPolygon(minor);
    p.setBrush(QColor(17, 24, 39));
    QPolygonF diag;
    diag << QPointF(18, 18) << QPointF(36, 40) << QPointF(54, 18) << QPointF(36, 34);
    p.drawPolygon(diag);
    p.setPen(QPen(Qt::white, 1));
    p.setFont(QFont(QStringLiteral("Malgun Gothic"), 8, QFont::Bold));
    p.drawText(QRectF(28, 10, 16, 12), Qt::AlignCenter, QStringLiteral("N"));
  }
}

QString writeNorthPng(int kind = 1) {
  const int s = 512;
  QImage img(s, s, QImage::Format_ARGB32_Premultiplied);
  img.fill(Qt::transparent);
  QPainter p(&img);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.scale(static_cast<double>(s) / 72.0, static_cast<double>(s) / 72.0);
  paintNorthMark(p, kind);
  p.end();
  const QString path = QDir::temp().filePath(QStringLiteral("ka-hgis-t3d-north-%1.png").arg(kind));
  if (!img.save(path, "PNG"))
    return {};
  return QFile::exists(path) ? path : QString();
}

QgsLayoutItem* findItemById(QgsLayout* ly, const char* id) {
  if (!ly)
    return nullptr;
  for (QGraphicsItem* gi : ly->items()) {
    if (auto* it = dynamic_cast<QgsLayoutItem*>(gi)) {
      if (it->id() == QLatin1String(id))
        return it;
    }
  }
  return nullptr;
}

QgsPrintLayout* sheetOf(QgsProject* project) {
  if (!project)
    return nullptr;
  return dynamic_cast<QgsPrintLayout*>(
      project->layoutManager()->layoutByName(QString::fromUtf8(Terrain3dLayoutService::kSheetName)));
}

LayoutService::SheetChromeRects chromeOf(QgsPrintLayout* ly) {
  const QRectF page(0.0, 0.0, 420.0, 297.0);
  QRectF mapR;
  if (auto* pic = findItemById(ly, Terrain3dLayoutService::kIdPicture))
    mapR = QRectF(pic->pos(), pic->rect().size());
  if (mapR.width() < 8.0)
    return LayoutService::standardSheetChrome(page, QRectF());
  return LayoutService::standardSheetChrome(page, mapR);
}

QgsVectorLayer* dummyExtentLayer(QObject* owner, const QgsRectangle& ext,
                                 const QgsCoordinateReferenceSystem& crs) {
  if (!owner || ext.isEmpty() || !ext.isFinite())
    return nullptr;
  const QString uri = QStringLiteral("Polygon?crs=%1")
                          .arg(crs.isValid() ? crs.authid() : QStringLiteral("EPSG:5186"));
  auto* vl = new QgsVectorLayer(uri, QStringLiteral("t3d_extent"), QStringLiteral("memory"));
  if (!vl->isValid()) {
    delete vl;
    return nullptr;
  }
  QgsPolylineXY ring;
  ring << QgsPointXY(ext.xMinimum(), ext.yMinimum()) << QgsPointXY(ext.xMaximum(), ext.yMinimum())
       << QgsPointXY(ext.xMaximum(), ext.yMaximum()) << QgsPointXY(ext.xMinimum(), ext.yMaximum())
       << QgsPointXY(ext.xMinimum(), ext.yMinimum());
  QgsFeature f(vl->fields());
  f.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
  vl->dataProvider()->addFeature(f);
  vl->updateExtents();
  if (auto fs = QgsFillSymbol::createSimple(
          {{QStringLiteral("color"), QStringLiteral("#00000000")},
           {QStringLiteral("outline_color"), QStringLiteral("#00000000")},
           {QStringLiteral("outline_width"), QStringLiteral("0")}})) {
    vl->setRenderer(new QgsSingleSymbolRenderer(fs.release()));
  }
  vl->setOpacity(0.0);
  vl->setParent(owner);
  return vl;
}

QgsPrintLayout* replaceSheet(QgsProject* project) {
  if (QgsMasterLayoutInterface* old =
          project->layoutManager()->layoutByName(QString::fromUtf8(Terrain3dLayoutService::kSheetName)))
    project->layoutManager()->removeLayout(old);
  auto* layout = new QgsPrintLayout(project);
  layout->initializeDefaults();
  layout->setName(QString::fromUtf8(Terrain3dLayoutService::kSheetName));
  layout->setUnits(Qgis::LayoutUnit::Millimeters);
  LayoutService::applySingleRasterPassRendering(layout);
  if (layout->pageCollection() && layout->pageCollection()->pageCount() > 0) {
    if (QgsLayoutItemPage* page = layout->pageCollection()->page(0))
      page->setPageSize(QgsLayoutSize(420.0, 297.0, Qgis::LayoutUnit::Millimeters));
  }
  project->layoutManager()->addLayout(layout);
  return layout;
}

}  // namespace

namespace Terrain3dLayoutService {

QString buildSheet(QgsProject* project, const SheetSpec& spec, QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("프로젝트가 없습니다.");
    return {};
  }
  if (spec.pngPath.isEmpty() || !QFile::exists(spec.pngPath)) {
    if (errorOut) *errorOut = QStringLiteral("입체지형 그림이 없습니다.");
    return {};
  }
  QgsPrintLayout* ly = replaceSheet(project);
  if (!ly) {
    if (errorOut) *errorOut = QStringLiteral("입체지형 도면을 만들지 못했습니다.");
    return {};
  }

  const QRectF page(0.0, 0.0, 420.0, 297.0);
  const auto chrome = LayoutService::standardSheetChrome(page, QRectF());
  const QRectF picR = chrome.map;

  QgsCoordinateReferenceSystem crs = spec.crs;
  if (!crs.isValid())
    crs = QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186"));

  QgsLayoutItemMap* map = nullptr;
  if (!spec.groundExtent.isEmpty() && spec.groundExtent.isFinite()) {
    map = new QgsLayoutItemMap(ly);
    map->setId(QString::fromUtf8(kIdMap));
    map->attemptSetSceneRect(picR);
    map->setCrs(crs);
    map->setFrameEnabled(false);
    map->setBackgroundEnabled(true);
    map->setBackgroundColor(QColor(255, 255, 255));
    map->setItemOpacity(0.0);
    if (QgsVectorLayer* dummy = dummyExtentLayer(ly, spec.groundExtent, crs))
      map->setLayers({dummy});
    map->zoomToExtent(spec.groundExtent);
    if (spec.visibleWidthM > 1.0 && picR.width() > 1.0) {
      const double denom = spec.visibleWidthM / (picR.width() / 1000.0);
      if (std::isfinite(denom) && denom > 1.0)
        map->setScale(denom, true);
    }
    ly->addLayoutItem(map);
  }

  auto* pic = new QgsLayoutItemPicture(ly);
  pic->setId(QString::fromUtf8(kIdPicture));
  pic->setPicturePath(spec.pngPath, Qgis::PictureFormat::Raster);
  pic->setMode(Qgis::PictureFormat::Raster);
  pic->setResizeMode(QgsLayoutItemPicture::Stretch);
  pic->setFrameEnabled(true);
  pic->attemptSetSceneRect(picR);
  pic->refreshPicture();
  ly->addLayoutItem(pic);
  if (map)
    pic->setZValue(map->zValue() + 1.0);

  const QString northPng = writeNorthPng(1);
  if (!northPng.isEmpty()) {
    auto* north = new QgsLayoutItemPicture(ly);
    north->setId(QString::fromUtf8(kIdNorth));
    north->setPicturePath(northPng, Qgis::PictureFormat::Raster);
    north->setMode(Qgis::PictureFormat::Raster);
    north->setResizeMode(QgsLayoutItemPicture::Zoom);
    north->setNorthMode(QgsLayoutItemPicture::GridNorth);
    north->setNorthOffset(spec.yawDegFromNorth);
    if (map)
      north->setLinkedMap(map);
    north->setFrameEnabled(false);
    north->attemptSetSceneRect(chrome.north);
    north->refreshPicture();
    ly->addLayoutItem(north);
  }

  if (map) {
    auto* bar = new QgsLayoutItemScaleBar(ly);
    bar->setId(QString::fromUtf8(kIdScale));
    bar->setLinkedMap(map);
    bar->setStyle(QStringLiteral("Double Box"));
    bar->setUnits(Qgis::DistanceUnit::Meters);
    bar->setUnitLabel(QStringLiteral("m"));
    bar->setNumberOfSegments(2);
    bar->setNumberOfSegmentsLeft(0);
    const double span = spec.visibleWidthM > 1.0 ? spec.visibleWidthM : spec.groundExtent.width();
    bar->setUnitsPerSegment(Terrain3dService::scaleBarSegmentM(span));
    bar->attemptSetSceneRect(chrome.scaleBar);
    bar->setFrameEnabled(false);
    LayoutService::applySheetScaleBarInk(bar);
    bar->update();
    ly->addLayoutItem(bar);

    auto* scaleLbl = new QgsLayoutItemLabel(ly);
    scaleLbl->setId(QString::fromUtf8(kIdScaleLabel));
    const int denom = map->scale() > 1.0 ? int(std::lround(map->scale())) : 0;
    scaleLbl->setText(denom > 0 ? QStringLiteral("축척 1 : %1").arg(denom)
                                : QStringLiteral("축척 1 : —"));
    scaleLbl->setFont(QFont(QStringLiteral("Malgun Gothic"), 8));
    scaleLbl->attemptSetSceneRect(chrome.scaleLabel);
    scaleLbl->setFrameEnabled(false);
    ly->addLayoutItem(scaleLbl);
  }

  auto* crsLbl = new QgsLayoutItemLabel(ly);
  crsLbl->setId(QString::fromUtf8(kIdCrs));
  crsLbl->setText(spec.crsLabel.isEmpty() ? QStringLiteral("좌표계 —")
                                          : QStringLiteral("좌표계 %1").arg(spec.crsLabel));
  crsLbl->setFont(QFont(QStringLiteral("Malgun Gothic"), 8));
  crsLbl->attemptSetSceneRect(chrome.crs);
  crsLbl->setFrameEnabled(false);
  ly->addLayoutItem(crsLbl);

  return QString::fromUtf8(kSheetName);
}

bool ensureLegend(QgsProject* project, const QString& title, int pointSize, bool bold, bool italic,
                  QString* errorOut, bool createIfMissing) {
  QgsPrintLayout* ly = sheetOf(project);
  if (!ly) {
    if (errorOut) *errorOut = QStringLiteral("먼저 입체지형 도면출력을 하세요.");
    return false;
  }
  const auto chrome = chromeOf(ly);
  const QRectF legendR(chrome.map.right() - 52.0, chrome.map.top() + 6.0, 48.0, 74.0);
  auto* legend = dynamic_cast<QgsLayoutItemLabel*>(findItemById(ly, kIdLegend));
  if (!legend && !createIfMissing)
    return false;
  if (!legend) {
    legend = new QgsLayoutItemLabel(ly);
    legend->setId(QString::fromUtf8(kIdLegend));
    legend->setFrameEnabled(true);
    legend->setBackgroundEnabled(true);
    legend->setBackgroundColor(QColor(255, 255, 255, 240));
    legend->attemptSetSceneRect(legendR);
    ly->addLayoutItem(legend);
  }
  QFont f(QStringLiteral("Malgun Gothic"), std::max(7, pointSize), bold ? QFont::Bold : QFont::Normal);
  f.setItalic(italic);
  legend->setFont(f);
  legend->setHAlign(Qt::AlignLeft);
  legend->setText(title.isEmpty() ? QStringLiteral("범례") : title);
  if (auto* pic = findItemById(ly, kIdPicture))
    legend->setZValue(pic->zValue() + 1.0);
  legend->update();
  return true;
}

bool applyNorth(QgsProject* project, int kind, double sizeMm, QString* errorOut) {
  QgsPrintLayout* ly = sheetOf(project);
  if (!ly) {
    if (errorOut) *errorOut = QStringLiteral("먼저 입체지형 도면출력을 하세요.");
    return false;
  }
  double yaw = 0;
  QgsLayoutItemMap* map = dynamic_cast<QgsLayoutItemMap*>(findItemById(ly, kIdMap));
  QRectF r = chromeOf(ly).north;
  if (auto* old = dynamic_cast<QgsLayoutItemPicture*>(findItemById(ly, kIdNorth))) {
    yaw = old->northOffset();
    const QRectF keep = QRectF(old->pos(), old->rect().size());
    if (keep.width() > 8.0)
      r = keep;
    ly->removeLayoutItem(old);
  }
  const QString png = writeNorthPng(std::clamp(kind, 0, 3));
  if (png.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("방위 그림을 만들지 못했습니다.");
    return false;
  }
  if (sizeMm >= 12.0) {
    r.setWidth(sizeMm);
    r.setHeight(sizeMm);
  }
  auto* north = new QgsLayoutItemPicture(ly);
  north->setId(QString::fromUtf8(kIdNorth));
  north->setPicturePath(png, Qgis::PictureFormat::Raster);
  north->setMode(Qgis::PictureFormat::Raster);
  north->setResizeMode(QgsLayoutItemPicture::Zoom);
  north->setNorthMode(QgsLayoutItemPicture::GridNorth);
  north->setNorthOffset(yaw);
  if (map)
    north->setLinkedMap(map);
  north->setFrameEnabled(false);
  north->attemptSetSceneRect(r);
  north->refreshPicture();
  ly->addLayoutItem(north);
  return true;
}

bool applyScale(QgsProject* project, int denominator, QString* errorOut) {
  QgsPrintLayout* ly = sheetOf(project);
  if (!ly) {
    if (errorOut) *errorOut = QStringLiteral("먼저 입체지형 도면출력을 하세요.");
    return false;
  }
  auto* map = dynamic_cast<QgsLayoutItemMap*>(findItemById(ly, kIdMap));
  if (!map) {
    if (errorOut) *errorOut = QStringLiteral("축척 맵이 없습니다.");
    return false;
  }
  const int denom = std::max(10, denominator);
  double widthMm = 0;
  if (auto* pic = findItemById(ly, kIdPicture))
    widthMm = pic->rect().width();
  if (widthMm < 8.0)
    widthMm = map->rect().width();
  QgsRectangle ext = map->extent();
  const QgsRectangle next =
      LayoutService::extentForPaperScale(ext, widthMm, static_cast<double>(denom));
  if (next.isFinite() && next.width() > 0.0)
    map->zoomToExtent(next);
  map->setScale(double(denom), true);
  if (auto* lbl = dynamic_cast<QgsLayoutItemLabel*>(findItemById(ly, kIdScaleLabel))) {
    lbl->setText(QStringLiteral("축척 1 : %1").arg(denom));
    lbl->update();
  }
  if (auto* bar = dynamic_cast<QgsLayoutItemScaleBar*>(findItemById(ly, kIdScale))) {
    bar->setUnitsPerSegment(LayoutService::niceScaleBarSegmentMeters(widthMm, double(denom), 2));
    LayoutService::applySheetScaleBarInk(bar);
    bar->update();
  }
  return true;
}

bool replacePicture(QgsProject* project, const QString& pngPath, QString* errorOut) {
  QgsPrintLayout* ly = sheetOf(project);
  auto* pic = ly ? dynamic_cast<QgsLayoutItemPicture*>(findItemById(ly, kIdPicture)) : nullptr;
  if (!pic || pngPath.isEmpty() || !QFile::exists(pngPath)) {
    if (errorOut) *errorOut = QStringLiteral("입체지형 그림이 없습니다.");
    return false;
  }
  pic->setPicturePath(pngPath, Qgis::PictureFormat::Raster);
  pic->setResizeMode(QgsLayoutItemPicture::Stretch);
  pic->refreshPicture();
  return true;
}

double pictureWidthMm(QgsProject* project) {
  QgsPrintLayout* ly = sheetOf(project);
  if (auto* pic = ly ? findItemById(ly, kIdPicture) : nullptr)
    return pic->rect().width();
  return 0;
}

bool applyScaleBarStyle(QgsProject* project, const QString& style, QString* errorOut) {
  QgsPrintLayout* ly = sheetOf(project);
  if (!ly) {
    if (errorOut) *errorOut = QStringLiteral("먼저 입체지형 도면출력을 하세요.");
    return false;
  }
  auto* bar = dynamic_cast<QgsLayoutItemScaleBar*>(findItemById(ly, kIdScale));
  if (!bar) {
    if (errorOut) *errorOut = QStringLiteral("축척자가 없습니다.");
    return false;
  }
  bar->setStyle(style);
  LayoutService::applySheetScaleBarInk(bar);
  bar->update();
  return true;
}

bool ensureScaleLabel(QgsProject* project, QString* errorOut) {
  QgsPrintLayout* ly = sheetOf(project);
  if (!ly) {
    if (errorOut) *errorOut = QStringLiteral("먼저 입체지형 도면출력을 하세요.");
    return false;
  }
  if (findItemById(ly, kIdScaleLabel))
    return true;
  auto* map = dynamic_cast<QgsLayoutItemMap*>(findItemById(ly, kIdMap));
  auto* scaleLbl = new QgsLayoutItemLabel(ly);
  scaleLbl->setId(QString::fromUtf8(kIdScaleLabel));
  const int denom = map && map->scale() > 1.0 ? int(std::lround(map->scale())) : 0;
  scaleLbl->setText(denom > 0 ? QStringLiteral("축척 1 : %1").arg(denom)
                              : QStringLiteral("축척 1 : —"));
  scaleLbl->setFont(QFont(QStringLiteral("Malgun Gothic"), 8));
  scaleLbl->attemptSetSceneRect(chromeOf(ly).scaleLabel);
  scaleLbl->setFrameEnabled(false);
  ly->addLayoutItem(scaleLbl);
  return true;
}

bool ensureCrsLabel(QgsProject* project, QString* errorOut) {
  QgsPrintLayout* ly = sheetOf(project);
  if (!ly) {
    if (errorOut) *errorOut = QStringLiteral("먼저 입체지형 도면출력을 하세요.");
    return false;
  }
  if (findItemById(ly, kIdCrs))
    return true;
  auto* crsLbl = new QgsLayoutItemLabel(ly);
  crsLbl->setId(QString::fromUtf8(kIdCrs));
  QString auth;
  if (auto* map = dynamic_cast<QgsLayoutItemMap*>(findItemById(ly, kIdMap)))
    auth = map->crs().authid();
  crsLbl->setText(auth.isEmpty() ? QStringLiteral("좌표계 —")
                                 : QStringLiteral("좌표계 %1").arg(auth));
  crsLbl->setFont(QFont(QStringLiteral("Malgun Gothic"), 8));
  crsLbl->attemptSetSceneRect(chromeOf(ly).crs);
  crsLbl->setFrameEnabled(false);
  ly->addLayoutItem(crsLbl);
  return true;
}

}  // namespace Terrain3dLayoutService
