#include <QtTest>
#include <QFile>
#include <QTemporaryDir>
#include <cmath>
#include <gdal.h>
#include <memory>
#include <qgsapplication.h>
#include <qgscoordinatetransform.h>
#include <qgsfeature.h>
#include <qgsfillsymbol.h>
#include <qgsgeometry.h>
#include <qgsmapsettings.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>
#include <qgssinglesymbolrenderer.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorlayer.h>

#include "core/MapGeoTiffExport.h"

namespace {
QByteArray readFile(const QString& path) {
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

bool createRaster(const QString& path) {
  GDALAllRegister();
  GDALDatasetH dataset = GDALCreate(GDALGetDriverByName("GTiff"), path.toUtf8().constData(),
                                   64, 64, 3, GDT_Byte, nullptr);
  if (!dataset) return false;
  double transform[] = {200000, 1, 0, 600064, 0, -1};
  bool ok = GDALSetGeoTransform(dataset, transform) == CE_None &&
            GDALSetProjection(dataset, QgsCoordinateReferenceSystem("EPSG:5187").toWkt().toUtf8().constData()) == CE_None;
  for (int band = 1; band <= 3; ++band) {
    ok = GDALFillRaster(GDALGetRasterBand(dataset, band), band == 3 ? 255 : 0, 0) == CE_None && ok;
    GDALSetRasterColorInterpretation(GDALGetRasterBand(dataset, band),
                                    band == 1 ? GCI_RedBand : band == 2 ? GCI_GreenBand : GCI_BlueBand);
  }
  GDALClose(dataset);
  return ok;
}

std::unique_ptr<QgsVectorLayer> polygon(const QString& color) {
  auto layer = std::make_unique<QgsVectorLayer>(QStringLiteral("Polygon?crs=EPSG:5187"),
                                               QStringLiteral("조사구역"), QStringLiteral("memory"));
  QgsFeature feature(layer->fields());
  feature.setGeometry(QgsGeometry::fromWkt(QStringLiteral(
      "POLYGON((200000 600000,200032 600000,200032 600064,200000 600064,200000 600000))")));
  QgsFeatureList features{feature};
  layer->dataProvider()->addFeatures(features);
  layer->updateExtents();
  layer->setRenderer(new QgsSingleSymbolRenderer(QgsFillSymbol::createSimple(
      {{QStringLiteral("color"), color}, {QStringLiteral("outline_style"), QStringLiteral("no")}}).release()));
  return layer;
}

QgsMapSettings settingsFor(QgsMapLayer* layer) {
  QgsMapSettings settings;
  settings.setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
  settings.setExtent(QgsRectangle(200000, 600000, 200064, 600064));
  settings.setOutputSize(QSize(64, 64));
  settings.setOutputDpi(96);
  settings.setLayers({layer});
  settings.setBackgroundColor(Qt::white);
  return settings;
}
}

class MapGeoTiffExportTest : public QObject {
  Q_OBJECT
private slots:
  void renderedMapKeepsCoordinatesAndPixels_data() {
    QTest::addColumn<QString>("targetCrs");
    QTest::addColumn<double>("rotation");
    QTest::addColumn<double>("dpr");
    QTest::addColumn<double>("opacity");
    QTest::newRow("work-5187") << QStringLiteral("EPSG:5187") << 0.0 << 1.0 << 1.0;
    QTest::newRow("rotated-work-5187") << QStringLiteral("EPSG:5187") << 30.0 << 1.0 << 1.0;
    QTest::newRow("retina-work-5187") << QStringLiteral("EPSG:5187") << 0.0 << 2.0 << 1.0;
    QTest::newRow("map-reprojected-5179") << QStringLiteral("EPSG:5179") << 0.0 << 1.0 << 1.0;
    QTest::newRow("map-reprojected-5186") << QStringLiteral("EPSG:5186") << 0.0 << 1.0 << 1.0;
    QTest::newRow("map-geographic-4326") << QStringLiteral("EPSG:4326") << 0.0 << 1.0 << 1.0;
    QTest::newRow("layer-opacity") << QStringLiteral("EPSG:5187") << 0.0 << 1.0 << 0.5;
  }

  void renderedMapKeepsCoordinatesAndPixels() {
    QFETCH(QString, targetCrs);
    QFETCH(double, rotation);
    QFETCH(double, dpr);
    QFETCH(double, opacity);
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString rasterPath = temp.filePath(QStringLiteral("원본.tif"));
    QVERIFY(createRaster(rasterPath));
    const QByteArray original = readFile(rasterPath);
    QgsRasterLayer raster(rasterPath, QStringLiteral("배경 영상"), QStringLiteral("gdal"));
    QVERIFY(raster.isValid());
    auto red = polygon(QStringLiteral("255,0,0,255"));
    red->setOpacity(opacity);
    auto hiddenGreen = polygon(QStringLiteral("0,255,0,255"));
    QgsMapSettings settings = settingsFor(&raster);
    settings.setLayers({red.get(), &raster}); // Hidden green is deliberately not in the canvas snapshot.
    const QgsCoordinateReferenceSystem sourceCrs = settings.destinationCrs();
    const QgsCoordinateReferenceSystem target(targetCrs);
    QgsCoordinateTransform projection(sourceCrs, target, settings.transformContext());
    const QgsRectangle bounds = projection.transformBoundingBox(settings.extent());
    settings.setDestinationCrs(target);
    settings.setExtent(bounds);
    settings.setRotation(rotation);
    settings.setDevicePixelRatio(static_cast<float>(dpr));
    const QString output = temp.filePath(QStringLiteral("측량 배경.tif"));
    QString error;
    QVERIFY2(MapGeoTiffExport::write(settings, output, &error), qPrintable(error));
    QVERIFY(error.isEmpty());
    QCOMPARE(readFile(rasterPath), original);
    QCOMPARE(settings.devicePixelRatio(), static_cast<float>(dpr));
    QCOMPARE(settings.rotation(), rotation);
    QCOMPARE(settings.layers(), QList<QgsMapLayer*>({red.get(), &raster}));

    using Dataset = std::unique_ptr<void, decltype(&GDALClose)>;
    Dataset result(GDALOpen(output.toUtf8().constData(), GA_ReadOnly), GDALClose);
    QVERIFY(result);
    QCOMPARE(GDALGetRasterXSize(result.get()), 64);
    QCOMPARE(GDALGetRasterYSize(result.get()), 64);
    QCOMPARE(GDALGetRasterCount(result.get()), 3);
    for (int band = 1; band <= 3; ++band)
      QCOMPARE(GDALGetRasterDataType(GDALGetRasterBand(result.get(), band)), GDT_Byte);
    QCOMPARE(GDALGetRasterColorInterpretation(GDALGetRasterBand(result.get(), 1)), GCI_RedBand);
    QCOMPARE(GDALGetRasterColorInterpretation(GDALGetRasterBand(result.get(), 2)), GCI_GreenBand);
    QCOMPARE(GDALGetRasterColorInterpretation(GDALGetRasterBand(result.get(), 3)), GCI_BlueBand);
    const QgsCoordinateReferenceSystem saved = QgsCoordinateReferenceSystem::fromWkt(
        QString::fromUtf8(GDALGetProjectionRef(result.get())));
    QCOMPARE(saved.authid(), targetCrs);
    double transform[6];
    QCOMPARE(GDALGetGeoTransform(result.get(), transform), CE_None);
    QgsMapSettings normalized(settings);
    normalized.setDevicePixelRatio(1);
    for (const QPoint corner : {QPoint(0, 0), QPoint(64, 0), QPoint(0, 64), QPoint(64, 64)}) {
      const QgsPointXY expected = normalized.mapToPixel().toMapCoordinates(corner);
      const double x = transform[0] + corner.x() * transform[1] + corner.y() * transform[2];
      const double y = transform[3] + corner.x() * transform[4] + corner.y() * transform[5];
      QVERIFY(std::abs(x - expected.x()) < 1e-7);
      QVERIFY(std::abs(y - expected.y()) < 1e-7);
    }
    if (rotation != 0) QVERIFY(std::abs(transform[2]) > 0.1);
    // Known ground positions must sample the vector overlay and the original RGB raster.
    double inverse[6];
    QVERIFY(GDALInvGeoTransform(transform, inverse));
    for (const int sourceX : {16, 48}) {
      const QgsPointXY ground = projection.transform(QgsPointXY(200000 + sourceX, 600032));
      const int x = static_cast<int>(std::floor(inverse[0] + inverse[1] * ground.x() + inverse[2] * ground.y()));
      const int y = static_cast<int>(std::floor(inverse[3] + inverse[4] * ground.x() + inverse[5] * ground.y()));
      QVERIFY(x >= 0 && x < 64 && y >= 0 && y < 64);
      unsigned char pixel[3] = {};
      int bands[] = {1, 2, 3};
      QCOMPARE(GDALDatasetRasterIO(result.get(), GF_Read, x, y, 1, 1, pixel, 1, 1,
                                   GDT_Byte, 3, bands, 3, 3, 1), CE_None);
      const int redValue = sourceX == 16 ? qRound(255 * opacity) : 0;
      QVERIFY(std::abs(int(pixel[0]) - redValue) <= 1);
      QCOMPARE(pixel[1], static_cast<unsigned char>(0));
      QVERIFY(std::abs(int(pixel[2]) - (255 - redValue)) <= 1);
    }
    const QByteArray header = readFile(output).left(4);
    QVERIFY(header == QByteArray("II\x2a\x00", 4) || header == QByteArray("MM\x00\x2a", 4));
  }

  void invalidInputsAndCancellationPreserveDestination() {
    QTemporaryDir temp;
    auto red = polygon(QStringLiteral("255,0,0,255"));
    const QgsMapSettings valid = settingsFor(red.get());
    const QString path = temp.filePath(QStringLiteral("existing.tif"));
    const QByteArray original("existing user's file");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(original), original.size());
    file.close();
    QString error;
    auto invalid = valid;
    invalid.setDestinationCrs(QgsCoordinateReferenceSystem());
    QVERIFY(!MapGeoTiffExport::write(invalid, path, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(readFile(path), original);
    invalid = valid;
    invalid.setExtent(QgsRectangle());
    QVERIFY(!MapGeoTiffExport::write(invalid, path, &error));
    invalid = valid;
    invalid.setLayers({});
    QVERIFY(!MapGeoTiffExport::write(invalid, path, &error));
    invalid = valid;
    invalid.setOutputSize(QSize(8193, 64));
    QVERIFY(!MapGeoTiffExport::write(invalid, path, &error));
    invalid = valid;
    invalid.setOutputSize(QSize(8192, 8192));
    QVERIFY(!MapGeoTiffExport::write(invalid, path, &error));
    QVERIFY(!MapGeoTiffExport::write(valid, path, &error, [] { return true; }));
    QCOMPARE(readFile(path), original);
    for (const int cancelAt : {3, 5}) {
      int calls = 0;
      QVERIFY(!MapGeoTiffExport::write(valid, path, &error, [&calls, cancelAt] { return ++calls >= cancelAt; }));
      QVERIFY(error.contains(QStringLiteral("취소")));
      QCOMPARE(readFile(path), original);
    }
    QCOMPARE(QDir(temp.path()).entryList(QDir::Files | QDir::Hidden), QStringList{QStringLiteral("existing.tif")});
  }

  void sourceRasterCannotBeOverwritten() {
    QTemporaryDir temp;
    const QString path = temp.filePath(QStringLiteral("source.tif"));
    QVERIFY(createRaster(path));
    const QByteArray original = readFile(path);
    QgsRasterLayer raster(path, QStringLiteral("source"), QStringLiteral("gdal"));
    QString error;
    QVERIFY(!MapGeoTiffExport::write(settingsFor(&raster), path, &error));
    QVERIFY(error.contains(QStringLiteral("원본")));
    QCOMPARE(readFile(path), original);
  }

  void visibleSelectionColorIsPreserved() {
    QTemporaryDir temp;
    auto red = polygon(QStringLiteral("255,0,0,255"));
    red->selectAll();
    QgsMapSettings settings = settingsFor(red.get());
    settings.setFlag(Qgis::MapSettingsFlag::DrawSelection, true);
    settings.setSelectionColor(Qt::green);
    const QString path = temp.filePath(QStringLiteral("selected.tif"));
    QString error;
    QVERIFY2(MapGeoTiffExport::write(settings, path, &error), qPrintable(error));
    using Dataset = std::unique_ptr<void, decltype(&GDALClose)>;
    Dataset result(GDALOpen(path.toUtf8().constData(), GA_ReadOnly), GDALClose);
    QVERIFY(result);
    unsigned char pixel[3] = {};
    int bands[] = {1, 2, 3};
    QCOMPARE(GDALDatasetRasterIO(result.get(), GF_Read, 16, 32, 1, 1, pixel, 1, 1,
                                 GDT_Byte, 3, bands, 3, 3, 1), CE_None);
    QCOMPARE(pixel[0], static_cast<unsigned char>(0));
    QCOMPARE(pixel[1], static_cast<unsigned char>(255));
    QCOMPARE(pixel[2], static_cast<unsigned char>(0));
    QCOMPARE(red->selectedFeatureCount(), 1);
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH", "A:/OSGeo4W/apps/qgis-dev"), true);
  QgsApplication::initQgis();
  MapGeoTiffExportTest test;
  const int result = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis();
  return result;
}

#include "test_map_geotiff_export.moc"
