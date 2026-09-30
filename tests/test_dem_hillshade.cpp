#include "core/DemAnalyzer.h"
#include "core/DemPresentation.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <cmath>
#include <functional>
#include <memory>
#include <vector>

#include <cpl_conv.h>
#include <gdal_priv.h>
#include <ogr_spatialref.h>
#include <qgsapplication.h>
#include <qgsrasterlayer.h>

namespace {

// 1 m cells in EPSG:5186 with the top-left corner at (200000, 500000).
bool writeDem(const QString& path, int w, int h, const std::function<float(int, int)>& value) {
  GDALAllRegister();
  GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
  std::unique_ptr<GDALDataset, decltype(&GDALClose)> dataset(
      driver ? driver->Create(path.toUtf8().constData(), w, h, 1, GDT_Float32, nullptr) : nullptr, GDALClose);
  if (!dataset) return false;
  double transform[] = {200000.0, 1.0, 0.0, 500000.0, 0.0, -1.0};
  OGRSpatialReference crs;
  if (crs.importFromEPSG(5186) != OGRERR_NONE || dataset->SetSpatialRef(&crs) != CE_None ||
      dataset->SetGeoTransform(transform) != CE_None || dataset->GetRasterBand(1)->SetNoDataValue(-9999.0) != CE_None)
    return false;
  std::vector<float> z(static_cast<size_t>(w) * h);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) z[static_cast<size_t>(y) * w + x] = value(x, y);
  return dataset->GetRasterBand(1)->RasterIO(GF_Write, 0, 0, w, h, z.data(), w, h, GDT_Float32, 0, 0) == CE_None;
}

struct Shade {
  int width = 0;
  int height = 0;
  bool hasNoData = false;
  double noData = -1;
  std::vector<std::uint8_t> gray;
  std::uint8_t at(int x, int y) const { return gray[static_cast<size_t>(y) * width + x]; }
};

Shade readShade(const QString& path) {
  Shade shade;
  std::unique_ptr<GDALDataset, decltype(&GDALClose)> dataset(
      static_cast<GDALDataset*>(GDALOpen(path.toUtf8().constData(), GA_ReadOnly)), GDALClose);
  if (!dataset) return shade;
  shade.width = dataset->GetRasterXSize();
  shade.height = dataset->GetRasterYSize();
  int has = 0;
  shade.noData = dataset->GetRasterBand(1)->GetNoDataValue(&has);
  shade.hasNoData = has != 0;
  shade.gray.resize(static_cast<size_t>(shade.width) * shade.height);
  if (dataset->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, shade.width, shade.height, shade.gray.data(), shade.width,
                                          shade.height, GDT_Byte, 0, 0) != CE_None)
    shade.gray.clear();
  return shade;
}

}  // namespace

class TestDemHillshade : public QObject {
  Q_OBJECT
private slots:
  void fullyShadedCellsStayVisible();
  void stripsMatchTheWholeRaster();
  void noDataNeighboursStayTransparent();
  void outputNeverOverwrites();
  void sampleRangeReadsTheWindow();
};

void TestDemHillshade::fullyShadedCellsStayVisible() {
  QTemporaryDir dir;
  const QString dem = dir.filePath(QStringLiteral("steep.tif"));
  QVERIFY(writeDem(dem, 20, 20, [](int x, int) { return static_cast<float>(10.0 * x); }));
  // Find the light that puts this steep face in full shadow (gray 0 from the Horn kernel).
  DemAnalyzer::RasterInfo info;
  std::vector<float> z;
  QString error;
  QVERIFY2(DemAnalyzer::readFloatBand(dem, &z, &info, &error), qPrintable(error));
  DemAnalyzer::Options options;
  options.hillshade = DemAnalyzer::HillshadeMode::Single;
  options.altitudeDeg = 10;
  bool shadowed = false;
  for (double azimuth : {90.0, 270.0}) {
    options.azimuthDeg = azimuth;
    std::vector<std::uint8_t> gray;
    DemAnalyzer::hillshadeHorn(z, info, options, &gray);
    if (gray[static_cast<size_t>(10) * 20 + 10] == 0) {
      shadowed = true;
      break;
    }
  }
  QVERIFY(shadowed);
  const QString out = dir.filePath(QStringLiteral("shade.tif"));
  QVERIFY2(DemAnalyzer::runHillshadeFile(dem, out, options, &error), qPrintable(error));
  const Shade shade = readShade(out);
  QVERIFY(!shade.gray.empty());
  QVERIFY(shade.hasNoData && shade.noData == 0.0);
  for (int y = 1; y < 19; ++y)
    for (int x = 1; x < 19; ++x) QVERIFY2(shade.at(x, y) >= 1, qPrintable(QStringLiteral("%1,%2").arg(x).arg(y)));
  QCOMPARE(shade.at(0, 0), std::uint8_t(0));
  QCOMPARE(shade.at(19, 10), std::uint8_t(0));
}

void TestDemHillshade::stripsMatchTheWholeRaster() {
  QTemporaryDir dir;
  const QString dem = dir.filePath(QStringLiteral("hills.tif"));
  QVERIFY(writeDem(dem, 37, 23, [](int x, int y) {
    return static_cast<float>(50.0 + 8.0 * std::sin(x * 0.4) + 5.0 * std::cos(y * 0.3) + 0.2 * x * y);
  }));
  QString error;
  const QString whole = dir.filePath(QStringLiteral("whole.tif"));
  const QString strips = dir.filePath(QStringLiteral("strips.tif"));
  QVERIFY2(DemAnalyzer::runHillshadeFile(dem, whole, DemAnalyzer::Options{}, &error), qPrintable(error));
  QVERIFY2(DemAnalyzer::runHillshadeFile(dem, strips, DemAnalyzer::Options{}, &error, 5), qPrintable(error));
  const Shade a = readShade(whole);
  const Shade b = readShade(strips);
  QVERIFY(!a.gray.empty());
  QVERIFY(a.gray == b.gray);
}

void TestDemHillshade::noDataNeighboursStayTransparent() {
  QTemporaryDir dir;
  const QString dem = dir.filePath(QStringLiteral("hole.tif"));
  QVERIFY(writeDem(dem, 20, 20, [](int x, int y) {
    return x == 10 && y == 10 ? -9999.0f : static_cast<float>(20.0 + 0.5 * x + 0.3 * y);
  }));
  QString error;
  const QString out = dir.filePath(QStringLiteral("hole_shade.tif"));
  QVERIFY2(DemAnalyzer::runHillshadeFile(dem, out, DemAnalyzer::Options{}, &error, 4), qPrintable(error));
  const Shade shade = readShade(out);
  QVERIFY(!shade.gray.empty());
  QCOMPARE(shade.at(9, 10), std::uint8_t(0));
  QCOMPARE(shade.at(11, 11), std::uint8_t(0));
  QVERIFY(shade.at(5, 5) >= 1);
}

void TestDemHillshade::outputNeverOverwrites() {
  QTemporaryDir dir;
  const QString dem = QDir(dir.filePath(QStringLiteral("dem"))).filePath(QStringLiteral("site.tif"));
  const QString survey = QDir(dir.filePath(QStringLiteral("조사"))).filePath(QStringLiteral("survey.gpkg"));
  QCOMPARE(DemAnalyzer::hillshadeOutputPath(dem, QString()),
           QDir(dir.filePath(QStringLiteral("dem"))).filePath(QStringLiteral("site_hillshade.tif")));
  const QString first = DemAnalyzer::hillshadeOutputPath(dem, survey);
  QCOMPARE(first, QDir(dir.filePath(QStringLiteral("조사/지형분석"))).filePath(QStringLiteral("site_hillshade.tif")));
  QVERIFY(QDir().mkpath(QFileInfo(dem).absolutePath()));
  QVERIFY(writeDem(dem, 8, 8, [](int x, int) { return static_cast<float>(x); }));
  QString error;
  QVERIFY2(DemAnalyzer::runHillshadeFile(dem, first, DemAnalyzer::Options{}, &error), qPrintable(error));
  QVERIFY(QFile::exists(first));
  const QString second = DemAnalyzer::hillshadeOutputPath(dem, survey);
  QVERIFY(second != first);
  QVERIFY(second.endsWith(QStringLiteral("site_hillshade_2.tif")));
}

void TestDemHillshade::sampleRangeReadsTheWindow() {
  QTemporaryDir dir;
  const QString dem = dir.filePath(QStringLiteral("ramp.tif"));
  QVERIFY(writeDem(dem, 64, 32, [](int x, int) { return static_cast<float>(0.5 * x); }));
  QgsRasterLayer layer(dem, QStringLiteral("DEM"), QStringLiteral("gdal"));
  QVERIFY(layer.isValid());
  double low = 0, high = 0;
  QVERIFY(DemPresentation::sampleRange(&layer, QgsRectangle(200010.0, 499980.0, 200021.0, 499995.0), &low, &high));
  QVERIFY2(low >= 4.0 && high <= 11.0 && high - low >= 4.0, qPrintable(QStringLiteral("%1 %2").arg(low).arg(high)));
  QVERIFY(!DemPresentation::sampleRange(&layer, QgsRectangle(0, 0, 10, 10), &low, &high));
}

int main(int argc, char** argv) {
  CPLSetConfigOption("GDAL_PAM_ENABLED", "NO");
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  int code = 0;
  {
    TestDemHillshade test;
    code = QTest::qExec(&test, argc, argv);
  }
  QgsApplication::exitQgis();
  return code;
}

#include "test_dem_hillshade.moc"
