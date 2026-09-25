#include <cmath>

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTextStream>
#include <QDateTime>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <qgsziputils.h>

#include "core/GeorefService.h"
#include "core/KaSafeQgis.h"
#include "core/LayerOps.h"
#include "core/SurveyProjectFactory.h"

#include <qgsapplication.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#include <qgsrasterlayer.h>
#include <qgsgeometry.h>
#include <qgspointxy.h>
#include <qgsrectangle.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsfeature.h>
#include <qgsfields.h>
#include <QPainter>
#include <qgsrasterrenderer.h>
#include <qgsrastertransparency.h>
#include <qgsbrightnesscontrastfilter.h>

#include <gdal.h>

class TestGeoref : public QObject {
  Q_OBJECT
private slots:
  void twoPointRaster_flipsYAndKeepsThirdPointOnTarget();
  void helmertTranslateScale();
  void helmertRotate90();
  void affineThreePoints();
  void invertRoundTrip();
  void fitRasterToExtentWorldFile();
  void vectorAffineDoesNotNeedControlPoints();
  void domainLayersNotAlignable();
  void pngWithoutWorldFileLooksUnreferenced();
  void geotiffEmbeddedGtIsReferenced();
  void knockOutRasterPaperMakesWhiteTransparent();
  void persistAlignedRaster_keepsSameLayerMapExtentNoRebuild();
  void persistAlignedRaster_jpegGetsMapExtentOn5187();
  void styleAlignedRasterOverlay_keepsInkDarkAfterPaperKnockout();
  void styleAlignedRasterOverlay_colorGeotiffKeepsNaturalColors();
  void updateAlignOverlay_remapsDestFromMapOnEveryPaint();
  void safeReadQgisProject_missingFileReturnsFalse();
  void qgisProjectFile_duplicateSatellitesDetected();
};

static bool nearly(double a, double b, double eps = 1e-6) {
  return std::abs(a - b) <= eps;
}

void TestGeoref::twoPointRaster_flipsYAndKeepsThirdPointOnTarget() {
  // 현장 증상: 두 점을 찍어 맞추면 그 두 점은 붙는데 사진 전체는 엉뚱한 데로 간다.
  // 원인은 그림 픽셀(y가 아래로 증가)에서 지도(y가 위로 증가)로 갈 때 필요한
  // 상하 뒤집기가 2점 식에 없었던 것. 뒤집기가 없으면 두 점을 잇는 선을 축으로
  // 거울상이 된다.
  //
  // 원본 그림 1000x800 을 지도 위에 그대로 놓았다고 보면
  //   mapX = 100000 + col,  mapY = 500000 + (800 - row)
  auto toMap = [](double col, double row, double* mx, double* my) {
    *mx = 100000.0 + col;
    *my = 500000.0 + (800.0 - row);
  };

  QVector<GeorefService::Pair> p;
  for (const QPointF& src : {QPointF(100, 100), QPointF(900, 700)}) {
    GeorefService::Pair pr;
    pr.srcX = src.x();
    pr.srcY = src.y();
    toMap(pr.srcX, pr.srcY, &pr.mapX, &pr.mapY);
    p.append(pr);
  }

  const GeorefService::Affine a = GeorefService::fromPairs(p, true);
  QVERIFY2(a.valid, "두 점이면 맞춰져야 한다");

  // 그림 → 지도는 상하가 뒤집히므로 행렬식이 음수여야 한다.
  const double det = a.a * a.e - a.b * a.d;
  QVERIFY2(det < 0.0, qPrintable(QStringLiteral("상하 뒤집기가 있어야 한다 (det=%1)").arg(det)));

  // 찍지 않은 제3의 점도 제자리에 가야 진짜로 맞은 것이다.
  double gx = 0, gy = 0;
  toMap(700.0, 200.0, &gx, &gy);
  double mx = 0, my = 0;
  QVERIFY(GeorefService::transform(a, 700.0, 200.0, &mx, &my));
  QVERIFY2(std::abs(mx - gx) < 1e-6 && std::abs(my - gy) < 1e-6,
           qPrintable(QStringLiteral("제3점이 %1,%2 로 가야 하는데 %3,%4")
                          .arg(gx).arg(gy).arg(mx).arg(my)));

  // CAD·벡터(왼쪽도 지도 좌표)는 뒤집지 않는다 — 예전 동작 그대로.
  const GeorefService::Affine v = GeorefService::fromPairs(p, false);
  QVERIFY(v.valid);
  QVERIFY2(v.a * v.e - v.b * v.d > 0.0, "벡터 정합은 뒤집지 않는다");
}


void TestGeoref::helmertTranslateScale() {
  QVector<GeorefService::Pair> p;
  p.append({0, 0, 200000, 450000});
  p.append({10, 0, 200020, 450000});
  const GeorefService::Affine a = GeorefService::fromPairs(p);
  QVERIFY(a.valid);
  QCOMPARE(a.pairCount, 2);
  double mx = 0, my = 0;
  QVERIFY(GeorefService::transform(a, 5, 0, &mx, &my));
  QVERIFY2(nearly(mx, 200010, 1e-4), qPrintable(QString::number(mx)));
  QVERIFY2(nearly(my, 450000, 1e-4), qPrintable(QString::number(my)));
  QVERIFY(a.rmsMeters < 1e-6);
}

void TestGeoref::helmertRotate90() {
  QVector<GeorefService::Pair> p;
  p.append({0, 0, 0, 0});
  p.append({1, 0, 0, 1});
  const GeorefService::Affine a = GeorefService::fromPairs(p);
  QVERIFY(a.valid);
  double mx = 0, my = 0;
  QVERIFY(GeorefService::transform(a, 0, 1, &mx, &my));
  QVERIFY2(nearly(mx, -1, 1e-6), qPrintable(QString::number(mx)));
  QVERIFY2(nearly(my, 0, 1e-6), qPrintable(QString::number(my)));
}

void TestGeoref::affineThreePoints() {
  QVector<GeorefService::Pair> p;
  p.append({0, 0, 100, 200});
  p.append({10, 0, 120, 200});
  p.append({0, 10, 100, 230});
  const GeorefService::Affine a = GeorefService::fromPairs(p);
  QVERIFY(a.valid);
  QCOMPARE(a.pairCount, 3);
  double mx = 0, my = 0;
  QVERIFY(GeorefService::transform(a, 10, 10, &mx, &my));
  QVERIFY2(nearly(mx, 120, 1e-4), qPrintable(QString::number(mx)));
  QVERIFY2(nearly(my, 230, 1e-4), qPrintable(QString::number(my)));
}

void TestGeoref::invertRoundTrip() {
  QVector<GeorefService::Pair> p;
  p.append({2, 3, 500, 700});
  p.append({12, 3, 520, 705});
  p.append({2, 13, 498, 730});
  const GeorefService::Affine a = GeorefService::fromPairs(p);
  QVERIFY(a.valid);
  double mx = 0, my = 0, sx = 0, sy = 0;
  QVERIFY(GeorefService::transform(a, 7, 8, &mx, &my));
  QVERIFY(GeorefService::invert(a, mx, my, &sx, &sy));
  QVERIFY2(nearly(sx, 7, 1e-6), qPrintable(QString::number(sx)));
  QVERIFY2(nearly(sy, 8, 1e-6), qPrintable(QString::number(sy)));
}

void TestGeoref::fitRasterToExtentWorldFile() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_georef_wf_")
                                            + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QVERIFY(QDir().mkpath(dir));
  const QString png = dir + QStringLiteral("/scan.png");
  QImage img(100, 50, QImage::Format_RGB32);
  img.fill(Qt::white);
  QVERIFY(img.save(png));

  const QgsRectangle ext(200000, 450000, 200200, 450100);
  const GeorefService::Affine a = GeorefService::fitRasterToExtent(100, 50, ext);
  QVERIFY(a.valid);
  QString err;
  QVERIFY2(GeorefService::writeWorldFile(png, a, &err), qPrintable(err));
  const QString pgw = dir + QStringLiteral("/scan.pgw");
  QVERIFY(QFile::exists(pgw));

  const QgsCoordinateReferenceSystem crs(QStringLiteral("EPSG:5186"));
  QVERIFY(GeorefService::writeSidecarPrj(png, crs, &err));
  QVERIFY(QFile::exists(GeorefService::prjPathFor(png)));

  auto* rl = new QgsRasterLayer(png, QStringLiteral("scan"), QStringLiteral("gdal"));
  QVERIFY2(rl->isValid(), qPrintable(rl->error().message()));
  QVERIFY(GeorefService::applyWorldFileToRaster(rl, a, crs, &err));
  QCOMPARE(rl->crs().authid(), QStringLiteral("EPSG:5186"));
  const QgsRectangle got = rl->extent();
  QVERIFY2(nearly(got.xMinimum(), 200000, 1.0), qPrintable(QString::number(got.xMinimum())));
  QVERIFY2(nearly(got.yMaximum(), 450100, 1.0), qPrintable(QString::number(got.yMaximum())));
  delete rl;
}

void TestGeoref::vectorAffineDoesNotNeedControlPoints() {
  auto* vl = new QgsVectorLayer(QStringLiteral("LineString?crs=EPSG:5186"),
                                QStringLiteral("cad"), QStringLiteral("memory"));
  QVERIFY(vl->isValid());
  QVERIFY(vl->startEditing());
  QgsFeature f(vl->fields());
  QgsPolylineXY line;
  line << QgsPointXY(0, 0) << QgsPointXY(10, 0) << QgsPointXY(10, 10);
  f.setGeometry(QgsGeometry::fromPolylineXY(line));
  QVERIFY(vl->addFeature(f));
  QVERIFY(vl->commitChanges());

  QHash<qint64, QgsGeometry> originals;
  QgsFeatureIterator it = vl->getFeatures();
  QgsFeature g;
  while (it.nextFeature(g)) originals.insert(g.id(), g.geometry());
  QVERIFY(!originals.isEmpty());

  QVector<GeorefService::Pair> p;
  p.append({0, 0, 200000, 450000});
  p.append({10, 0, 200100, 450000});
  const GeorefService::Affine a = GeorefService::fromPairs(p);
  QVERIFY(a.valid);
  QString err;
  QVERIFY2(GeorefService::applyAffineToVector(
               vl, a, originals, QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")), &err),
           qPrintable(err));

  QgsFeature out;
  QVERIFY(vl->getFeatures().nextFeature(out));
  const QgsPointXY start = out.geometry().asPolyline().first();
  QVERIFY2(nearly(start.x(), 200000, 1e-3), qPrintable(QString::number(start.x())));
  QVERIFY2(nearly(start.y(), 450000, 1e-3), qPrintable(QString::number(start.y())));
  vl->updateExtents();
  const QgsRectangle moved = vl->extent();
  QVERIFY2(moved.xMinimum() > 199990.0, qPrintable(QString::number(moved.xMinimum())));
  QVERIFY2(moved.yMinimum() > 449990.0, qPrintable(QString::number(moved.yMinimum())));
  QVERIFY(!GeorefService::isDomainSurveyLayer(vl));
  QVERIFY(GeorefService::isAlignableLayer(vl));
  delete vl;
}

void TestGeoref::domainLayersNotAlignable() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_georef_dom_")
                                            + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QVERIFY(QDir().mkpath(dir));
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("g"), &err);
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));
  auto* sa = new QgsVectorLayer(QStringLiteral("%1|layername=survey_area").arg(gpkg),
                                QStringLiteral("survey_area"), QStringLiteral("ogr"));
  QVERIFY(sa->isValid());
  LayerOps::markSurveyLayer(sa, QStringLiteral("survey_area"));
  QVERIFY(GeorefService::isDomainSurveyLayer(sa));
  QVERIFY(!GeorefService::isAlignableLayer(sa));
  delete sa;
}

void TestGeoref::pngWithoutWorldFileLooksUnreferenced() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_georef_png_")
                                            + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QVERIFY(QDir().mkpath(dir));
  const QString png = dir + QStringLiteral("/scan.png");
  QImage img(80, 40, QImage::Format_RGB32);
  img.fill(Qt::white);
  QVERIFY(img.save(png));
  auto* rl = new QgsRasterLayer(png, QStringLiteral("scan"), QStringLiteral("gdal"));
  QVERIFY2(rl->isValid(), qPrintable(rl->error().message()));
  QVERIFY(GeorefService::looksUnreferencedRaster(rl));
  QVERIFY(!QFile::exists(GeorefService::worldFilePathFor(png)));
  delete rl;
}

void TestGeoref::geotiffEmbeddedGtIsReferenced() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_georef_gt_")
                                            + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QVERIFY(QDir().mkpath(dir));
  const QString tif = dir + QStringLiteral("/wall.tif");
  GDALAllRegister();
  GDALDriverH drv = GDALGetDriverByName("GTiff");
  QVERIFY2(drv, "GTiff driver missing");
  GDALDatasetH ds = GDALCreate(drv, qUtf8Printable(tif), 8, 6, 3, GDT_Byte, nullptr);
  QVERIFY2(ds, "GeoTIFF create failed");
  double gt[6] = {194880.0, 0.01, 0.0, 574000.0, 0.0, -0.01};
  GDALSetGeoTransform(ds, gt);
  GDALClose(ds);
  QVERIFY(!QFile::exists(GeorefService::worldFilePathFor(tif)));
  auto* rl = new QgsRasterLayer(tif, QStringLiteral("wall"), QStringLiteral("gdal"));
  QVERIFY2(rl->isValid(), qPrintable(rl->error().message()));
  QVERIFY2(!GeorefService::looksUnreferencedRaster(rl),
           "embedded map GT must not be treated as a raw scan");
  delete rl;
}

void TestGeoref::persistAlignedRaster_keepsSameLayerMapExtentNoRebuild() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_georef_persist_")
                                            + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QVERIFY(QDir().mkpath(dir));
  const QString png = dir + QStringLiteral("/scan.png");
  QImage img(80, 40, QImage::Format_RGB32);
  img.fill(Qt::white);
  QVERIFY(img.save(png));

  auto* rl = new QgsRasterLayer(png, QStringLiteral("scan"), QStringLiteral("gdal"));
  QVERIFY2(rl->isValid(), qPrintable(rl->error().message()));
  QVERIFY(GeorefService::looksUnreferencedRaster(rl));

  QgsProject* proj = QgsProject::instance();
  QVERIFY(proj);
  proj->addMapLayer(rl, false);
  const QString id = rl->id();

  const QgsRectangle dest(200000.0, 450000.0, 200200.0, 450100.0);
  const GeorefService::Affine a = GeorefService::fitRasterToExtent(80, 40, dest);
  QVERIFY(a.valid);
  const QgsCoordinateReferenceSystem crs(QStringLiteral("EPSG:5186"));
  QString err;
  const bool persisted = GeorefService::persistAlignedRaster(rl, a, crs, &err);
  QVERIFY2(persisted,
           qPrintable(err + QStringLiteral(" extent=") + rl->extent().toString(3)
                      + QStringLiteral(" crs=") + rl->crs().authid()));

  QCOMPARE(rl->id(), id);
  QVERIFY2(proj->mapLayer(id) == rl, "이동 후 같은 레이어를 프로젝트에서 빼면 안 됨");
  QVERIFY2(!GeorefService::looksUnreferencedRaster(rl),
           "월드파일 적용 후 픽셀 평면에 남으면 안 됨");
  QVERIFY2(!GeorefService::mustRebuildRasterAfterWorldFile(rl),
           "적용된 래스터는 remove+재생성 없이 써야 함");
  const QgsRectangle e = rl->extent();
  QVERIFY(e.xMinimum() > 1000.0);
  QVERIFY(e.yMinimum() > 1000.0);
  QVERIFY(QFile::exists(GeorefService::worldFilePathFor(png)));

  proj->removeMapLayer(id);
}

void TestGeoref::persistAlignedRaster_jpegGetsMapExtentOn5187() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_georef_jpg_")
                                            + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QVERIFY(QDir().mkpath(dir));
  const QString jpg = dir + QStringLiteral("/plan.jpg");
  QImage img(120, 80, QImage::Format_RGB32);
  img.fill(QColor(250, 250, 248));
  QVERIFY(img.save(jpg, "JPEG"));

  auto* rl = new QgsRasterLayer(jpg, QStringLiteral("plan"), QStringLiteral("gdal"));
  QVERIFY2(rl->isValid(), qPrintable(rl->error().message()));
  QVERIFY(GeorefService::looksUnreferencedRaster(rl));

  const QgsRectangle dest(194800.0, 574700.0, 195040.0, 574860.0);
  const GeorefService::Affine a = GeorefService::fitRasterToExtent(120, 80, dest);
  QVERIFY(a.valid);
  const QgsCoordinateReferenceSystem crs(QStringLiteral("EPSG:5187"));
  QString err;
  QVERIFY2(GeorefService::persistAlignedRaster(rl, a, crs, &err), qPrintable(err));
  QVERIFY2(!GeorefService::looksUnreferencedRaster(rl),
           qPrintable(QStringLiteral("jpeg still pixel-plane extent=") + rl->extent().toString(2)));
  const QgsRectangle e = rl->extent();
  QVERIFY2(e.xMinimum() > 190000.0 && e.yMinimum() > 570000.0,
           "현장 JPG는 이동 후 5187 지적 좌표에 있어야 함");
  QVERIFY(QFile::exists(GeorefService::worldFilePathFor(jpg)));
  delete rl;
}

void TestGeoref::knockOutRasterPaperMakesWhiteTransparent() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_georef_ko_")
                                            + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QVERIFY(QDir().mkpath(dir));
  const QString tif = dir + QStringLiteral("/paper.tif");
  QImage img(16, 12, QImage::Format_RGB32);
  img.fill(Qt::white);
  img.setPixelColor(4, 4, QColor(80, 60, 40));
  QVERIFY(img.save(tif, "TIFF"));
  auto* rl = new QgsRasterLayer(tif, QStringLiteral("paper"), QStringLiteral("gdal"));
  QVERIFY2(rl->isValid(), qPrintable(rl->error().message()));
  LayerOps::knockOutRasterPaper(rl);
  QVERIFY(rl->renderer());
  QVERIFY(rl->renderer()->rasterTransparency());
  const auto list = rl->renderer()->rasterTransparency()->transparentThreeValuePixelList();
  bool knocked = false;
  for (const auto& px : list) {
    if (std::abs(px.red - 255.0) < 1e-6 && px.opacity < 0.01)
      knocked = true;
  }
  QVERIFY2(knocked, "white paper must be transparent");
  delete rl;
}

void TestGeoref::styleAlignedRasterOverlay_keepsInkDarkAfterPaperKnockout() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_georef_ink_")
                                            + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QVERIFY(QDir().mkpath(dir));
  const QString tif = dir + QStringLiteral("/ink.tif");
  QImage img(16, 12, QImage::Format_RGB32);
  img.fill(Qt::white);
  img.setPixelColor(4, 4, QColor(40, 40, 40));
  QVERIFY(img.save(tif, "TIFF"));
  auto* rl = new QgsRasterLayer(tif, QStringLiteral("ink"), QStringLiteral("gdal"));
  QVERIFY2(rl->isValid(), qPrintable(rl->error().message()));
  GeorefService::styleAlignedRasterOverlay(rl);
  QVERIFY2(rl->opacity() >= 0.98, "흰 종이만 빼고 먹선은 흐리면 안 됨");
  QCOMPARE(static_cast<int>(rl->blendMode()),
           static_cast<int>(QPainter::CompositionMode_Multiply));
  QVERIFY(rl->brightnessFilter());
  QVERIFY2(rl->brightnessFilter()->contrast() >= 20, "연한 스캔 선을 진하게");
  QVERIFY2(rl->brightnessFilter()->brightness() <= 0, "먹선이 밝아지면 안 됨");
  QVERIFY(rl->renderer());
  QVERIFY(rl->renderer()->rasterTransparency());
  const auto list = rl->renderer()->rasterTransparency()->transparentThreeValuePixelList();
  bool paper = false;
  for (const auto& px : list) {
    if (std::abs(px.red - 255.0) < 1e-6 && px.opacity < 0.01) {
      paper = true;
      QVERIFY2(px.fuzzyToleranceRed <= 10.0, "흰 여백 허용이 넓으면 연한 먹선까지 지움");
    }
    QVERIFY2(!(std::abs(px.red - 248.0) < 1.0 && px.fuzzyToleranceRed > 8.0),
             "미색 여백 광범위 투명은 스캔 선을 지움");
  }
  QVERIFY2(paper, "흰 배경은 그대로 빼야 함");
  delete rl;
}

void TestGeoref::styleAlignedRasterOverlay_colorGeotiffKeepsNaturalColors() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_georef_air_")
                                            + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QVERIFY(QDir().mkpath(dir));
  const QString tif = dir + QStringLiteral("/aerial.tif");
  QImage img(16, 12, QImage::Format_RGB32);
  for (int y = 0; y < 12; ++y) {
    for (int x = 0; x < 16; ++x)
      img.setPixelColor(x, y, QColor(48 + x * 6, 92 + y * 5, 54 + ((x + y) % 7) * 4));
  }
  QVERIFY(img.save(tif, "TIFF"));
  auto* rl = new QgsRasterLayer(tif, QStringLiteral("aerial"), QStringLiteral("gdal"));
  QVERIFY2(rl->isValid(), qPrintable(rl->error().message()));
  GeorefService::styleAlignedRasterOverlay(rl);
  QCOMPARE(static_cast<int>(rl->blendMode()),
           static_cast<int>(QPainter::CompositionMode_SourceOver));
  QVERIFY(rl->brightnessFilter());
  QVERIFY2(std::abs(rl->brightnessFilter()->contrast()) < 2,
           "항공 GeoTIFF에 먹선 대비를 주면 색이 바뀜");
  QVERIFY2(std::abs(rl->brightnessFilter()->brightness()) < 2,
           "항공 GeoTIFF 밝기를 내리면 다른 색이 됨");
  QVERIFY2(std::abs(rl->brightnessFilter()->gamma() - 1.0) < 0.05,
           "항공 GeoTIFF 감마를 바꾸면 색이 바뀜");
  if (rl->renderer() && rl->renderer()->rasterTransparency()) {
    const auto list = rl->renderer()->rasterTransparency()->transparentThreeValuePixelList();
    for (const auto& px : list) {
      QVERIFY2(!(std::abs(px.red - 255.0) < 1e-6 && px.opacity < 0.5),
               "항공사진 흰 픽셀을 투명이면 색이 빠짐");
    }
  }
  delete rl;
}

void TestGeoref::updateAlignOverlay_remapsDestFromMapOnEveryPaint() {
  QFile f(QStringLiteral("src/app/MainWindow.cpp"));
  QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text), "MainWindow.cpp");
  QString src = QString::fromUtf8(f.readAll());
  QFile alignSrc(QStringLiteral("src/app/MainWindowAlign.cpp"));
  QVERIFY2(alignSrc.open(QIODevice::ReadOnly | QIODevice::Text), "MainWindowAlign.cpp");
  src += QString::fromUtf8(alignSrc.readAll());
  const int fn = src.indexOf(QLatin1String("void MainWindow::updateAlignOverlay"));
  QVERIFY2(fn >= 0, "updateAlignOverlay");
  const int next = src.indexOf(QLatin1String("void MainWindow::"), fn + 10);
  QVERIFY2(next > fn, "updateAlignOverlay body");
  const QString body = src.mid(fn, next - fn);
  QVERIFY2(body.contains(QLatin1String("pairs[i].mapX")),
           "오른쪽 끝점은 매번 지도 좌표에서 다시 찍어야 함");
  QVERIFY2(!body.contains(QLatin1String("m_alignDstScreen[i]")),
           "화면 좌표 캐시는 줌하면 화살표가 지적에서 떨어짐");
  QVERIFY2(body.contains(QLatin1String("mapToPixel")),
           "QgsMapMouseEvent와 같은 mapToPixel을 써야 함");
  // m_alignOverlay는 m_mapSplitter의 자식이라 캔버스·그림뷰의 조상이 아니라
  // 형제다. QWidget::mapTo는 대상이 조상일 때만 유효하고, 형제를 주면 Qt가
  // "parent must be in parent hierarchy" 경고를 내고 최상위 창 좌표를 돌려줘
  // 화살표가 스플리터 원점만큼 통째로 밀린다. 전역 좌표를 거쳐야 한다.
  QVERIFY2(body.contains(QLatin1String("mapToGlobal")) &&
               body.contains(QLatin1String("mapFromGlobal")),
           "뷰포트→오버레이는 전역 좌표를 거쳐야 함(형제 위젯이라 mapTo는 못 씀)");
  QVERIFY2(!body.contains(QLatin1String("mapTo(m_alignOverlay")),
           "형제 위젯에 mapTo를 쓰면 화살표가 스플리터 원점만큼 밀린다");
  QVERIFY2(!body.contains(QLatin1String("mapFromScene")),
           "mapToPixel output is viewport pixels; extra scene mapping throws arrows");
  QVERIFY2(!body.contains(QLatin1String("devicePixelRatioF")),
           "DPR을 따로 나누면 줌할 때 화살표가 마커에서 떨어짐");

  QFile tool(QStringLiteral("src/app/KaAlignMapTool.cpp"));
  QVERIFY2(tool.open(QIODevice::ReadOnly | QIODevice::Text), "KaAlignMapTool.cpp");
  const QString toolSrc = QString::fromUtf8(tool.readAll());
  const int press = toolSrc.indexOf(QLatin1String("void KaAlignMapTool::canvasPressEvent"));
  QVERIFY2(press >= 0, "canvasPressEvent");
  const int pressEnd = toolSrc.indexOf(QLatin1String("void KaAlignMapTool::"), press + 10);
  const QString pressBody = toolSrc.mid(press, pressEnd > press ? pressEnd - press : 800);
  QVERIFY2(pressBody.contains(QLatin1String("mapPointFromEvent")),
           "오른쪽 클릭은 캔버스 지도 점을 써야 함");
  QVERIFY2(!pressBody.contains(QLatin1String("m_hasHint")),
           "커서 힌트가 클릭 점을 덮으면 이상한 곳으로 간다");

  const int ext = src.lastIndexOf(QLatin1String("extentsChanged"));
  QVERIFY2(ext >= 0, "extentsChanged");
  const int align = src.indexOf(QLatin1String("m_subToolsMode == QLatin1String(\"align\")"), ext);
  QVERIFY2(align > ext, "extentsChanged align");
  const QString handler = src.mid(ext, align + 220 - ext);
  const int upd = handler.indexOf(QLatin1String("updateAlignOverlay"));
  const int draw = handler.indexOf(QLatin1String("isDrawing()"));
  QVERIFY2(upd >= 0, "오른쪽 줌·팬마다 화살표를 다시 맞춤");
  QVERIFY2(draw < 0 || upd < draw, "지적 그리는 중에도 오버레이는 갱신해야 함");
  QVERIFY2(src.contains(QLatin1String("renderComplete")),
           "타일 그린 뒤에도 화살표를 다시 맞춤");
  QVERIFY2(src.contains(QLatin1String("connect(m_alignLeftCanvas, &QgsMapCanvas::extentsChanged")),
           "CAD 왼쪽 줌에도 연결선을 다시 맞춤");
  QVERIFY2(src.contains(QLatin1String("KaImageView::viewChanged")),
           "왼쪽 그림 팬에도 연결선을 다시 맞춤");
}

void TestGeoref::safeReadQgisProject_missingFileReturnsFalse() {
  QVERIFY(!kaSafeReadQgisProject(nullptr, QStringLiteral("missing.qgz")));
  QVERIFY(!kaSafeReadQgisProject(QgsProject::instance(), QString()));
  QVERIFY(!kaSafeReadQgisProject(QgsProject::instance(),
                                QStringLiteral("Z:/ka-hgis-no-such-project.qgz")));
  QVERIFY(!kaSafeClearQgisProject(nullptr));
}

void TestGeoref::qgisProjectFile_duplicateSatellitesDetected() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString oneQgs = dir.filePath(QStringLiteral("one.qgs"));
  {
    QFile f(oneQgs);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write("<?xml version='1.0'?>\n<qgis><projectlayers>"
            "<maplayer><layername>VWorld 위성</layername></maplayer>"
            "<maplayer><layername>지적 본번</layername></maplayer>"
            "</projectlayers></qgis>\n");
  }
  QCOMPARE(kaCountSatelliteLayersInQgisProjectFile(oneQgs), 1);
  QVERIFY(!kaQgisProjectFileHasDuplicateSatellites(oneQgs));

  const QString dupQgs = dir.filePath(QStringLiteral("dup.qgs"));
  {
    QFile f(dupQgs);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write("<?xml version='1.0'?>\n<qgis><projectlayers>"
            "<maplayer><layername>VWorld 위성</layername></maplayer>"
            "<maplayer><layername>VWorld 위성</layername></maplayer>"
            "<maplayer><layername>위성</layername></maplayer>"
            "<maplayer><layername>위성</layername></maplayer>"
            "</projectlayers>"
            "<layer-tree-group>"
            "<layer-tree-layer name=\"위성\"/>"
            "<layer-tree-layer name=\"위성\"/>"
            "<layer-tree-layer name=\"위성\"/>"
            "<layer-tree-layer name=\"위성\"/>"
            "</layer-tree-group></qgis>\n");
  }
  QCOMPARE(kaCountSatelliteLayersInQgisProjectFile(dupQgs), 4);
  QVERIFY(kaQgisProjectFileHasDuplicateSatellites(dupQgs));

  const QString qgz = dir.filePath(QStringLiteral("dup.qgz"));
  QVERIFY(QgsZipUtils::zip(qgz, QStringList{dupQgs}));
  QCOMPARE(kaCountSatelliteLayersInQgisProjectFile(qgz), 4);
  QVERIFY(kaQgisProjectFileHasDuplicateSatellites(qgz));
  QVERIFY(!kaQgisProjectFileIsUnsafeToRead(qgz));
  QVERIFY(!kaQgisProjectFileHasDuplicateSatellites(QStringLiteral("Z:/no-such.qgz")));
  QVERIFY(!kaQgisProjectFileIsUnsafeToRead(oneQgs));
  kaMarkQgisProjectUnsafeToRead(oneQgs);
  QVERIFY(kaQgisProjectFileIsUnsafeToRead(oneQgs));
}

#include "test_georef.moc"

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  const QString prefix = qEnvironmentVariable(
      "QGIS_PREFIX_PATH", QFile::exists(QStringLiteral("D:/OSGeo4W/apps/qgis-dev"))
                              ? QStringLiteral("D:/OSGeo4W/apps/qgis-dev")
                              : QStringLiteral("C:/OSGeo4W/apps/qgis-dev"));
  QgsApplication::setPrefixPath(prefix, true);
  QgsApplication::setPluginPath(prefix + QStringLiteral("/plugins"));
  QgsApplication::initQgis();
  TestGeoref tc;
  const int rc = QTest::qExec(&tc, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
