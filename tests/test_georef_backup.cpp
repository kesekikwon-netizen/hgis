#include <cmath>

#include <QtTest>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>

#include "core/GeorefBackup.h"
#include "core/GeorefPixelWindow.h"
#include "core/GeorefService.h"

#include <qgsapplication.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>
#include <qgsrectangle.h>

#include <gdal.h>

// Raster alignment backs up what it overwrites (world file, .prj, .aux.xml, a GeoTIFF's
// own geotransform) into the survey folder, and 「되돌리기」 restores exactly that.
class TestGeorefBackup : public QObject {
  Q_OBJECT
private slots:
  void plainScan_moveThenRestore_leavesNoTrace();
  void geotiff_restoreBringsBackEmbeddedCoordinates();
  void pamWithGeoref_isBackedUpAndRestored();
  void uniqueOutputPath_neverReusesAnExistingFile();
  void backupRoot_isInsideTheSurveyFolder();
  void readPixelWindow_readsOriginalPixels();

private:
  void pamRoundTrip(const QString& fileName, const char* format);
};

namespace {

QByteArray fileBytes(const QString& path) {
  QFile f(path);
  return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

QByteArray fileHash(const QString& path) {
  return QCryptographicHash::hash(fileBytes(path), QCryptographicHash::Sha1);
}

void writeText(const QString& path, const QByteArray& text) {
  QFile f(path);
  QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
  f.write(text);
}

bool internalGt(const QString& path, double gt[6]) {
  const char* const options[] = {"GEOREF_SOURCES=INTERNAL", nullptr};
  GDALDatasetH ds = GDALOpenEx(qUtf8Printable(path), GDAL_OF_RASTER | GDAL_OF_READONLY, nullptr,
                               options, nullptr);
  if (!ds) return false;
  const bool ok = GDALGetGeoTransform(ds, gt) == CE_None;
  GDALClose(ds);
  return ok;
}

const QgsRectangle kTarget(200000.0, 450000.0, 200160.0, 450080.0);

}  // namespace

void TestGeorefBackup::plainScan_moveThenRestore_leavesNoTrace() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString tif = dir.filePath(QStringLiteral("scan.tif"));
  QImage img(80, 40, QImage::Format_RGB32);
  img.fill(Qt::white);
  QVERIFY(img.save(tif, "TIFF"));
  // Statistics only: no georeferencing, so it must survive the move untouched.
  const QString aux = tif + QStringLiteral(".aux.xml");
  writeText(aux, "<PAMDataset><Metadata><MDI key=\"NOTE\">stats</MDI></Metadata></PAMDataset>\n");
  const QByteArray tifBefore = fileHash(tif);
  const QByteArray auxBefore = fileHash(aux);

  QgsRasterLayer rl(tif, QStringLiteral("scan"), QStringLiteral("gdal"));
  QVERIFY2(rl.isValid(), qPrintable(rl.error().message()));
  QVERIFY(GeorefService::looksUnreferencedRaster(&rl));
  QVERIFY(!GeorefBackup::hasInternalGeoref(tif));
  const QString id = rl.id();

  GeorefBackup::RasterBackup b;
  QString err;
  QVERIFY2(GeorefBackup::backupRaster(tif, dir.filePath(QStringLiteral("정합백업")), &b, &err),
           qPrintable(err));
  QVERIFY(b.valid && QFileInfo(b.folder).isDir());
  QVERIFY(QFile::exists(QDir(b.folder).filePath(QStringLiteral("backup.json"))));
  QVERIFY(!b.hadGeoTransform && b.gcps.isEmpty());

  const GeorefService::Affine a = GeorefService::fitRasterToExtent(80, 40, kTarget);
  const QgsCoordinateReferenceSystem crs(QStringLiteral("EPSG:5186"));
  QVERIFY2(GeorefService::persistAlignedRaster(&rl, a, crs, &err), qPrintable(err));
  QVERIFY(!GeorefService::looksUnreferencedRaster(&rl));
  QVERIFY(QFile::exists(GeorefService::worldFilePathFor(tif)));
  QCOMPARE(fileHash(tif), tifBefore);  // the scan itself is never rewritten
  // The sidecar keeps its metadata and gains nothing that georeferences. Its bytes are
  // not compared here: the QGIS provider writes the band statistics it computed into the
  // PAM when its handle closes (for any raster it opens), and that close is what lets the
  // layer read the new world file. The restore below puts the original bytes back.
  QVERIFY(QFile::exists(aux));
  QVERIFY(fileBytes(aux).contains("NOTE") && fileBytes(aux).contains("stats"));
  QVERIFY(!GeorefBackup::pamCarriesGeoref(aux));

  QVERIFY2(GeorefBackup::restoreRasterLayer(&rl, b, QgsCoordinateReferenceSystem(), &err),
           qPrintable(err));
  QCOMPARE(rl.id(), id);
  QVERIFY(!QFile::exists(GeorefService::worldFilePathFor(tif)));
  QVERIFY(!QFile::exists(GeorefService::prjPathFor(tif)));
  QCOMPARE(fileHash(aux), auxBefore);
  QCOMPARE(fileHash(tif), tifBefore);
  QVERIFY2(GeorefService::looksUnreferencedRaster(&rl),
           qPrintable(QStringLiteral("extent after restore ") + rl.extent().toString(2)));
}

void TestGeorefBackup::geotiff_restoreBringsBackEmbeddedCoordinates() {
  // A drone orthophoto already carries coordinates; aligning it again replaced them for good.
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString tif = dir.filePath(QStringLiteral("ortho.tif"));
  GDALAllRegister();
  GDALDatasetH ds = GDALCreate(GDALGetDriverByName("GTiff"), qUtf8Printable(tif), 8, 4, 3,
                               GDT_Byte, nullptr);
  QVERIFY(ds);
  double original[6] = {194880.0, 0.5, 0.0, 574000.0, 0.0, -0.5};
  GDALSetGeoTransform(ds, original);
  GDALClose(ds);
  QVERIFY(GeorefBackup::hasInternalGeoref(tif));

  QgsRasterLayer rl(tif, QStringLiteral("ortho"), QStringLiteral("gdal"));
  QVERIFY2(rl.isValid(), qPrintable(rl.error().message()));
  QVERIFY(std::abs(rl.extent().xMinimum() - 194880.0) < 1e-6);

  GeorefBackup::RasterBackup b;
  QString err;
  QVERIFY2(GeorefBackup::backupRaster(tif, dir.filePath(QStringLiteral("정합백업")), &b, &err),
           qPrintable(err));
  QVERIFY(b.hadGeoTransform);

  const GeorefService::Affine a = GeorefService::fitRasterToExtent(8, 4, kTarget);
  const QgsCoordinateReferenceSystem crs(QStringLiteral("EPSG:5186"));
  QVERIFY2(GeorefService::persistAlignedRaster(&rl, a, crs, &err), qPrintable(err));
  QVERIFY2(std::abs(rl.extent().xMinimum() - 200000.0) < 1e-3, qPrintable(rl.extent().toString(2)));
  double now[6];
  QVERIFY(internalGt(tif, now));
  QVERIFY2(std::abs(now[0] - 200000.0) < 1e-3, "embedded GT must follow the move (world file is ignored)");

  QVERIFY2(GeorefBackup::restoreRasterLayer(&rl, b, QgsCoordinateReferenceSystem(), &err),
           qPrintable(err));
  QVERIFY(internalGt(tif, now));
  for (int i = 0; i < 6; ++i) QVERIFY2(std::abs(now[i] - original[i]) < 1e-9, "embedded GT restored");
  QVERIFY(!QFile::exists(GeorefService::worldFilePathFor(tif)));
  QVERIFY2(std::abs(rl.extent().xMinimum() - 194880.0) < 1e-6, qPrintable(rl.extent().toString(2)));
}

void TestGeorefBackup::pamRoundTrip(const QString& fileName, const char* format) {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString img = dir.filePath(fileName);
  QImage pic(40, 20, QImage::Format_RGB32);
  pic.fill(Qt::white);
  QVERIFY(pic.save(img, format));
  const QString aux = img + QStringLiteral(".aux.xml");
  writeText(aux, "<PAMDataset><GeoTransform>1.0e+05, 1.0, 0.0, 5.0e+05, 0.0, -1.0</GeoTransform>"
                 "</PAMDataset>\n");
  QVERIFY(GeorefBackup::pamCarriesGeoref(aux));
  QVERIFY(!GeorefBackup::hasInternalGeoref(img));  // the sidecar, not the file, has it
  const QByteArray auxBefore = fileHash(aux);

  QgsRasterLayer rl(img, QStringLiteral("plan"), QStringLiteral("gdal"));
  QVERIFY2(rl.isValid(), qPrintable(rl.error().message()));
  QVERIFY2(std::abs(rl.extent().xMinimum() - 100000.0) < 1e-3, qPrintable(rl.extent().toString(2)));
  GeorefBackup::RasterBackup b;
  QString err;
  QVERIFY2(GeorefBackup::backupRaster(img, dir.filePath(QStringLiteral("정합백업")), &b, &err),
           qPrintable(err));
  const GeorefService::Affine a = GeorefService::fitRasterToExtent(40, 20, kTarget);
  QVERIFY2(GeorefService::persistAlignedRaster(&rl, a, QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")), &err),
           qPrintable(err));
  QVERIFY2(!QFile::exists(aux), "a PAM with coordinates would override the new world file");
  QVERIFY2(std::abs(rl.extent().xMinimum() - 200000.0) < 1e-3, qPrintable(rl.extent().toString(2)));

  QVERIFY2(GeorefBackup::restoreRasterLayer(&rl, b, QgsCoordinateReferenceSystem(), &err),
           qPrintable(err));
  QCOMPARE(fileHash(aux), auxBefore);
  QVERIFY(!QFile::exists(GeorefService::worldFilePathFor(img)));
  QVERIFY2(std::abs(rl.extent().xMinimum() - 100000.0) < 1e-3, qPrintable(rl.extent().toString(2)));
}

void TestGeorefBackup::pamWithGeoref_isBackedUpAndRestored() {
  // PNG reads the world file before the PAM; a GeoTIFF reads the PAM first, so there the
  // sidecar must really be gone by the time the layer is read again.
  pamRoundTrip(QStringLiteral("plan.png"), nullptr);
  if (QTest::currentTestFailed()) return;
  pamRoundTrip(QStringLiteral("plan.tif"), "TIFF");
}

void TestGeorefBackup::uniqueOutputPath_neverReusesAnExistingFile() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString want = dir.filePath(QStringLiteral("cad_aligned.gpkg"));
  QCOMPARE(GeorefBackup::uniqueOutputPath(want), want);
  writeText(want, "x");
  const QString second = GeorefBackup::uniqueOutputPath(want);
  QCOMPARE(QFileInfo(second).fileName(), QStringLiteral("cad_aligned_2.gpkg"));
  writeText(second, "x");
  QCOMPARE(QFileInfo(GeorefBackup::uniqueOutputPath(want)).fileName(),
           QStringLiteral("cad_aligned_3.gpkg"));
}

void TestGeorefBackup::backupRoot_isInsideTheSurveyFolder() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString survey = dir.filePath(QStringLiteral("현장/조사.gpkg"));
  QCOMPARE(QDir::cleanPath(GeorefBackup::backupRootFor(survey)),
           QDir::cleanPath(dir.filePath(QStringLiteral("현장/정합백업"))));
  QVERIFY(GeorefBackup::backupRootFor(QString()).endsWith(QStringLiteral("정합백업")));
}

void TestGeorefBackup::readPixelWindow_readsOriginalPixels() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString tif = dir.filePath(QStringLiteral("big.tif"));
  QImage img(64, 32, QImage::Format_RGB32);
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 64; ++x) img.setPixelColor(x, y, x < 32 ? QColor(255, 0, 0) : QColor(0, 0, 255));
  QVERIFY(img.save(tif, "TIFF"));

  QString err;
  const QImage right = GeorefService::readPixelWindow(tif, QRect(32, 0, 32, 32), QSize(16, 16), &err);
  QVERIFY2(!right.isNull(), qPrintable(err));
  QCOMPARE(right.size(), QSize(16, 16));
  QCOMPARE(right.pixelColor(8, 8), QColor(0, 0, 255));

  const QImage whole = GeorefService::readPixelWindow(tif, QRect(0, 0, 64, 32), QSize(8, 4), &err);
  QVERIFY2(!whole.isNull(), qPrintable(err));
  QCOMPARE(whole.pixelColor(1, 1), QColor(255, 0, 0));
  QCOMPARE(whole.pixelColor(6, 2), QColor(0, 0, 255));

  // Never upsampled beyond the source, never outside the image.
  QCOMPARE(GeorefService::readPixelWindow(tif, QRect(0, 0, 4, 4), QSize(40, 40)).size(), QSize(4, 4));
  QVERIFY(GeorefService::readPixelWindow(tif, QRect(60, 0, 10, 10), QSize(10, 10)).isNull());
}

#include "test_georef_backup.moc"

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  const QString prefix = qEnvironmentVariable(
      "QGIS_PREFIX_PATH", QFile::exists(QStringLiteral("D:/OSGeo4W/apps/qgis-dev"))
                              ? QStringLiteral("D:/OSGeo4W/apps/qgis-dev")
                              : QStringLiteral("C:/OSGeo4W/apps/qgis-dev"));
  QgsApplication::setPrefixPath(prefix, true);
  QgsApplication::setPluginPath(prefix + QStringLiteral("/plugins"));
  QgsApplication::initQgis();
  TestGeorefBackup tc;
  const int rc = QTest::qExec(&tc, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
