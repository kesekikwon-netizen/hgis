#include <QtTest>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QScopeGuard>
#include <QJsonDocument>
#include <QJsonObject>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <qgsapplication.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransform.h>
#include <qgspointxy.h>
#include <qgsfeedback.h>
#include "core/TopographicCatalog.h"
#include <memory>
#include <algorithm>
#include <limits>

namespace {
bool writeVector(const QString& path, const char* driverName, bool knownCrs,
                 double x = 190000., const char* layerName = "contours", bool append = false) {
  auto* driver = GetGDALDriverManager()->GetDriverByName(driverName);
  if (!driver) return false;
  std::unique_ptr<GDALDataset, decltype(&GDALClose)> dataset(
      append ? static_cast<GDALDataset*>(GDALOpenEx(path.toUtf8().constData(), GDAL_OF_VECTOR | GDAL_OF_UPDATE,
                                                   nullptr, nullptr, nullptr))
             : driver->Create(path.toUtf8().constData(), 0, 0, 0, GDT_Unknown, nullptr), GDALClose);
  if (!dataset) return false;
  OGRSpatialReference crs;
  if (crs.importFromEPSG(5187) != OGRERR_NONE) return false;
  auto* layer = dataset->CreateLayer(layerName, knownCrs ? &crs : nullptr, wkbLineString, nullptr);
  if (!layer) return false;
  OGRLineString line; line.addPoint(x, 560000.); line.addPoint(x + 100., 560100.);
  std::unique_ptr<OGRFeature, decltype(&OGRFeature::DestroyFeature)> feature(
      OGRFeature::CreateFeature(layer->GetLayerDefn()), OGRFeature::DestroyFeature);
  return feature->SetGeometry(&line) == OGRERR_NONE && layer->CreateFeature(feature.get()) == OGRERR_NONE;
}
QByteArray hash(const QString& path) {
  QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {};
  return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256);
}
}
class TopographicCatalogTest : public QObject {
  Q_OBJECT
private slots:
  void initTestCase() { GDALAllRegister(); }
  void dxfCadLayersAreIndexedSeparately() {
    QTemporaryDir files; QTemporaryDir cache;
    const QString path = files.filePath(QStringLiteral("37701.dxf"));
    auto* driver = GetGDALDriverManager()->GetDriverByName("DXF"); QVERIFY(driver);
    {
      std::unique_ptr<GDALDataset, decltype(&GDALClose)> ds(driver->Create(path.toUtf8().constData(), 0, 0, 0, GDT_Unknown, nullptr), GDALClose);
      QVERIFY(ds);
      auto* layer = ds->CreateLayer("entities", nullptr, wkbUnknown, nullptr); QVERIFY(layer);
      for (const auto& code : {"F0017111", "F0017114", "mystery"}) {
        std::unique_ptr<OGRFeature, decltype(&OGRFeature::DestroyFeature)> feature(OGRFeature::CreateFeature(layer->GetLayerDefn()), OGRFeature::DestroyFeature);
        feature->SetField("Layer", code);
        OGRLineString line; line.addPoint(200000., 450000., 50.); line.addPoint(200100., 450100., 50.);
        QCOMPARE(feature->SetGeometry(&line), OGRERR_NONE);
        QCOMPARE(layer->CreateFeature(feature.get()), OGRERR_NONE);
      }
    }
    const auto before = hash(path);
    const auto result = TopographicCatalog::scan(files.path(), nullptr, cache.path());
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QCOMPARE(result.records.size(), 3);
    int contours = 0, unknown = 0;
    for (const auto& record : result.records) {
      QVERIFY(record.hasExtent); QVERIFY(record.crsWkt.isEmpty());
      if (record.category == TopographicCatalog::Category::Contour) ++contours;
      else if (record.category == TopographicCatalog::Category::Unknown) ++unknown;
    }
    QCOMPARE(contours, 2); QCOMPARE(unknown, 1);
    QCOMPARE(hash(path), before);
    QCOMPARE(TopographicCatalog::scan(files.path(), nullptr, cache.path()).records.size(), 3);
  }
  void recursiveReadOnlyCacheAndInvalidation() {
    QTemporaryDir files; QTemporaryDir cache; QVERIFY(files.isValid() && cache.isValid());
    QVERIFY(QDir(files.path()).mkdir(QStringLiteral("nested")));
    const QString gpkg = files.filePath(QStringLiteral("nested/multi.gpkg"));
    const QString shp = files.filePath(QStringLiteral("unclassified.shp"));
    QVERIFY(writeVector(gpkg, "GPKG", true)); QVERIFY(writeVector(shp, "ESRI Shapefile", false));
    const auto gpkgHash = hash(gpkg), shpHash = hash(shp);
    const auto initial = TopographicCatalog::scan(files.path(), nullptr, cache.path());
    QVERIFY2(initial.error.isEmpty(), qPrintable(initial.error)); QVERIFY(!initial.canceled);
    QCOMPARE(initial.records.size(), 2); QCOMPARE(initial.filesRead, 2); QCOMPARE(initial.cacheHits, 0);
    bool unknown = false, known = false;
    for (const auto& record : initial.records) {
      QVERIFY(record.hasExtent); QVERIFY(!record.signature.isEmpty());
      QVERIFY(record.extent.contains(190050., 560050.));
      if (record.source == shp) { unknown = true; QVERIFY(record.crsWkt.isEmpty()); }
      if (record.source == gpkg) {
        known = true;
        QCOMPARE(QgsCoordinateReferenceSystem(record.crsWkt).authid(), QStringLiteral("EPSG:5187"));
      }
    }
    QVERIFY(unknown && known);
    const auto reused = TopographicCatalog::scan(files.path(), nullptr, cache.path());
    QCOMPARE(reused.records.size(), 2); QCOMPARE(reused.filesRead, 0); QCOMPARE(reused.cacheHits, 2);
    // CRS lives in a SHP sidecar, so changing only that file must invalidate it.
    QFile prj(files.filePath(QStringLiteral("unclassified.prj")));
    QVERIFY(prj.open(QIODevice::WriteOnly));
    QVERIFY(prj.write(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")).toWkt().toUtf8()) > 0);
    prj.close();
    const auto changed = TopographicCatalog::scan(files.path(), nullptr, cache.path());
    QCOMPARE(changed.records.size(), 2); QCOMPARE(changed.filesRead, 1); QCOMPARE(changed.cacheHits, 1);
    for (const auto& record : changed.records) QVERIFY(!record.crsWkt.isEmpty());
    QCOMPARE(hash(gpkg), gpkgHash); QCOMPARE(hash(shp), shpHash);
  }
  void projectsBoundsAndKeepsUnknownSeparate() {
    QTemporaryDir files; QTemporaryDir cache;
    QVERIFY(writeVector(files.filePath(QStringLiteral("near.gpkg")), "GPKG", true));
    QVERIFY(writeVector(files.filePath(QStringLiteral("far.gpkg")), "GPKG", true, 390000.));
    QVERIFY(writeVector(files.filePath(QStringLiteral("unknown.shp")), "ESRI Shapefile", false));
    const auto catalog = TopographicCatalog::scan(files.path(), nullptr, cache.path());
    QCOMPARE(catalog.records.size(), 3);
    auto records = catalog.records;
    for (auto& record : records)
      if (!record.crsWkt.isEmpty()) record.category = TopographicCatalog::Category::Road;
    const auto found = TopographicCatalog::query(records, QgsRectangle(189990., 559990., 190110., 560110.),
        QStringLiteral("EPSG:5187"), QgsCoordinateTransformContext());
    QVERIFY2(found.error.isEmpty(), qPrintable(found.error));
    QCOMPARE(found.matches.size(), 1); QCOMPARE(found.unknownCrs.size(), 1);
    QVERIFY(found.matches.first().source.endsWith(QStringLiteral("near.gpkg")));
    const QgsCoordinateTransform toCentral(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")),
        QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")), QgsCoordinateTransformContext());
    const auto center = toCentral.transform(QgsPointXY(190050., 560050.));
    const auto central = TopographicCatalog::query(records,
        QgsRectangle(center.x() - 70., center.y() - 70., center.x() + 70., center.y() + 70.),
        QStringLiteral("EPSG:5186"), QgsCoordinateTransformContext());
    QVERIFY(central.error.isEmpty()); QCOMPARE(central.matches.size(), 1);
    QCOMPARE(central.matches.first().source, found.matches.first().source);
    const auto noMatch = TopographicCatalog::query(records, QgsRectangle(1000., 1000., 2000., 2000.),
        QStringLiteral("EPSG:5186"), QgsCoordinateTransformContext());
    QVERIFY(noMatch.error.isEmpty()); QCOMPARE(noMatch.matches.size(), 0); QCOMPARE(noMatch.unknownCrs.size(), 1);
    const auto invalid = TopographicCatalog::query(records, QgsRectangle(0., 0., 1., 1.),
        QStringLiteral("invalid"), QgsCoordinateTransformContext());
    QVERIFY(!invalid.error.isEmpty());
  }
  void cancellationPreservesCacheAndRejectsPartialResult() {
    QTemporaryDir files; QTemporaryDir cache;
    QVERIFY(writeVector(files.filePath(QStringLiteral("data.gpkg")), "GPKG", true));
    const auto original = TopographicCatalog::scan(files.path(), nullptr, cache.path());
    QCOMPARE(original.records.size(), 1);
    const auto cacheFiles = QDir(cache.path()).entryList({QStringLiteral("*.json")}, QDir::Files);
    QCOMPARE(cacheFiles.size(), 1);
    const QString cacheFile = QDir(cache.path()).filePath(cacheFiles.first());
    const auto originalHash = hash(cacheFile);
    QgsFeedback canceled; canceled.cancel();
    const auto scan = TopographicCatalog::scan(files.path(), &canceled, cache.path());
    QVERIFY(scan.canceled); QVERIFY(scan.records.isEmpty()); QCOMPARE(hash(cacheFile), originalHash);
    const auto query = TopographicCatalog::query(original.records, QgsRectangle(189990., 559990., 190110., 560110.),
        QStringLiteral("EPSG:5187"), QgsCoordinateTransformContext(), &canceled);
    QVERIFY(query.canceled); QVERIFY(query.matches.isEmpty());
    bool stopAfterProgress = false;
    QgsFeedback progress;
    connect(&progress, &QgsFeedback::progressChanged, &progress, [&] { stopAfterProgress = true; });
    const auto callbackCanceled = TopographicCatalog::scan(files.path(), &progress, cache.path(),
        [&] { return stopAfterProgress; });
    QVERIFY(callbackCanceled.canceled); QVERIFY(callbackCanceled.records.isEmpty());
    QCOMPARE(hash(cacheFile), originalHash);
  }
  void multiLayerAndZipKeepDatasourceAndCrs() {
    QTemporaryDir files; QTemporaryDir cache; QTemporaryDir source;
    const QString gpkg = files.filePath(QStringLiteral("multi.gpkg"));
    QVERIFY(writeVector(gpkg, "GPKG", true, 190000., "F001"));
    QVERIFY(writeVector(gpkg, "GPKG", true, 390000., QStringLiteral("도로").toUtf8().constData(), true));
    const QString shp = source.filePath(QStringLiteral("boundary.shp"));
    QVERIFY(writeVector(shp, "ESRI Shapefile", true));
    const QString zip = files.filePath(QStringLiteral("tile.zip"));
    {
      void* handle = CPLCreateZip(zip.toUtf8().constData(), nullptr); QVERIFY(handle);
      const auto close = qScopeGuard([handle] { CPLCloseZip(handle); });
      const auto sidecars = QDir(source.path()).entryList(QDir::Files);
      for (const auto& name : sidecars) {
        QFile input(source.filePath(name)); QVERIFY(input.open(QIODevice::ReadOnly));
        const auto data = input.readAll();
        QCOMPARE(CPLCreateFileInZip(handle, (QStringLiteral("tile/") + name).toUtf8().constData(), nullptr), CE_None);
        QCOMPARE(CPLWriteFileInZip(handle, data.constData(), int(data.size())), CE_None);
        QCOMPARE(CPLCloseFileInZip(handle), CE_None);
      }
    }
    const auto before = hash(zip);
    const auto scan = TopographicCatalog::scan(files.path(), nullptr, cache.path());
    QVERIFY2(scan.error.isEmpty(), qPrintable(scan.error)); QCOMPARE(scan.records.size(), 3);
    QCOMPARE(scan.filesRead, 2);
    bool archive = false, road = false, code = false;
    for (const auto& record : scan.records) {
      if (record.source.startsWith(QLatin1String("/vsizip/"))) {
        archive = true; QVERIFY(record.hasExtent);
        QCOMPARE(QgsCoordinateReferenceSystem(record.crsWkt).authid(), QStringLiteral("EPSG:5187"));
      }
      if (record.layerName == QLatin1String("F001")) {
        code = true; QCOMPARE(record.category, TopographicCatalog::Category::Unknown);
      }
      if (record.layerName == QStringLiteral("도로")) {
        road = true; QCOMPARE(record.category, TopographicCatalog::Category::Road);
      }
    }
    QVERIFY(archive && road && code);
    const auto cached = TopographicCatalog::scan(files.path(), nullptr, cache.path());
    QCOMPARE(cached.records.size(), 3); QCOMPARE(cached.cacheHits, 2); QCOMPARE(cached.filesRead, 0);
    QCOMPARE(hash(zip), before);
  }
  void officialCodesRequireCompleteTokensAndUpgradeOldCache() {
    using Category = TopographicCatalog::Category;
    const QList<QPair<QString, Category>> examples{
      {QStringLiteral("F0010000"), Category::Contour},
      {QStringLiteral("F0020000_37701001"), Category::ElevationPoint},
      {QStringLiteral("수치지형도_A0010000_37701"), Category::Road},
      {QStringLiteral("A0020000"), Category::Road}, {QStringLiteral("B0010000"), Category::Building},
      {QStringLiteral("E0010001"), Category::Water}, {QStringLiteral("E0020000"), Category::Water},
      {QStringLiteral("E0032111"), Category::Water}, {QStringLiteral("G0010000"), Category::Boundary},
      {QStringLiteral("H0040000"), Category::PlaceName}, {QStringLiteral("F001"), Category::Unknown},
      {QStringLiteral("F001000037701"), Category::Unknown}, {QStringLiteral("XF0010000"), Category::Unknown},
      {QStringLiteral("F0010000X"), Category::Unknown}, {QStringLiteral("E0010000"), Category::Unknown},
      {QStringLiteral("F0010000_B0010000"), Category::Unknown}};
    QTemporaryDir files; QTemporaryDir cache;
    const QString path = files.filePath(QStringLiteral("codes.gpkg"));
    bool append = false;
    for (const auto& example : examples) {
      QVERIFY(writeVector(path, "GPKG", true, 190000., example.first.toUtf8().constData(), append));
      append = true;
    }
    const auto first = TopographicCatalog::scan(files.path(), nullptr, cache.path());
    QCOMPARE(first.records.size(), examples.size());
    for (const auto& record : first.records) {
      auto found = std::find_if(examples.cbegin(), examples.cend(), [&](const auto& example) {
        return example.first == record.layerName;
      });
      QVERIFY(found != examples.cend()); QCOMPARE(record.category, found->second);
    }
    QCOMPARE(TopographicCatalog::categoryName(Category::ElevationPoint), QStringLiteral("표고점"));
    QCOMPARE(TopographicCatalog::categoryName(Category::PlaceName), QStringLiteral("지명"));
    const auto cacheFiles = QDir(cache.path()).entryList({QStringLiteral("*.json")}, QDir::Files);
    QCOMPARE(cacheFiles.size(), 1);
    QFile old(QDir(cache.path()).filePath(cacheFiles.first())); QVERIFY(old.open(QIODevice::ReadOnly));
    auto document = QJsonDocument::fromJson(old.readAll()).object(); old.close();
    document.insert(QStringLiteral("version"), 1);
    QVERIFY(old.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray data = QJsonDocument(document).toJson(); QCOMPARE(old.write(data), data.size()); old.close();
    const auto upgraded = TopographicCatalog::scan(files.path(), nullptr, cache.path());
    QCOMPARE(upgraded.filesRead, 1); QCOMPARE(upgraded.cacheHits, 0); QCOMPARE(upgraded.records.size(), examples.size());
  }
  void removedFilesDisappearAndMalformedCacheRebuilds() {
    QTemporaryDir files; QTemporaryDir cache;
    const QString path = files.filePath(QStringLiteral("one.gpkg"));
    QVERIFY(writeVector(path, "GPKG", true));
    QCOMPARE(TopographicCatalog::scan(files.path(), nullptr, cache.path()).records.size(), 1);
    const auto names = QDir(cache.path()).entryList({QStringLiteral("*.json")}, QDir::Files);
    QCOMPARE(names.size(), 1);
    QFile corrupt(QDir(cache.path()).filePath(names.first())); QVERIFY(corrupt.open(QIODevice::WriteOnly));
    QCOMPARE(corrupt.write("{broken"), qint64(7)); corrupt.close();
    const auto rebuilt = TopographicCatalog::scan(files.path(), nullptr, cache.path());
    QCOMPARE(rebuilt.filesRead, 1); QCOMPARE(rebuilt.records.size(), 1);
    QVERIFY(QFile::remove(path));
    QVERIFY(TopographicCatalog::scan(files.path(), nullptr, cache.path()).records.isEmpty());
  }
  void reportLocationMapKeepsHousesAndDropsClutterAtAnyScale() {
    using C = TopographicCatalog::Category;
    QCOMPARE(TopographicCatalog::kDetailMaxScale, 25000.);
    for (const double scale : {0., 1218.64, 25000., 25000.1, 227884.,
                               std::numeric_limits<double>::infinity()}) {
      QVERIFY(TopographicCatalog::visibleAtScale(C::Building, scale));
      QVERIFY(TopographicCatalog::visibleAtScale(C::Contour, scale));
      QVERIFY(TopographicCatalog::visibleAtScale(C::Road, scale));
      QVERIFY(TopographicCatalog::visibleAtScale(C::Water, scale));
      QVERIFY(TopographicCatalog::visibleAtScale(C::Boundary, scale));
      QVERIFY(!TopographicCatalog::visibleAtScale(C::Vegetation, scale));
      QVERIFY(!TopographicCatalog::visibleAtScale(C::Facility, scale));
      QVERIFY(!TopographicCatalog::visibleAtScale(C::ElevationPoint, scale));
      QVERIFY(!TopographicCatalog::visibleAtScale(C::PlaceName, scale));
      QVERIFY(!TopographicCatalog::visibleAtScale(C::Unknown, scale));
    }

    TopographicCatalog::Record building, vegetation;
    building.source = QStringLiteral("B0010000.shp");
    building.layerName = QStringLiteral("B0010000");
    building.crsWkt = QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")).toWkt();
    building.extent = QgsRectangle(199900., 449900., 200200., 450200.);
    building.hasExtent = true;
    building.category = C::Building;
    vegetation = building;
    vegetation.source = QStringLiteral("D0010000.shp");
    vegetation.layerName = QStringLiteral("D0010000");
    vegetation.category = C::Vegetation;
    const QgsRectangle view(165000., 415000., 235000., 485000.);
    const auto overview = TopographicCatalog::query({building, vegetation}, view, QStringLiteral("EPSG:5186"),
        QgsCoordinateTransformContext(), nullptr, {}, 227884.);
    QCOMPARE(overview.matches.size(), 1);
    QCOMPARE(overview.matches.first().category, C::Building);
    const auto detail = TopographicCatalog::query({building, vegetation}, view, QStringLiteral("EPSG:5186"),
        QgsCoordinateTransformContext(), nullptr, {}, 1218.64);
    QCOMPARE(detail.matches.size(), 1);
    QCOMPARE(detail.matches.first().category, C::Building);
  }
  void coverageBoundsGrowsSurveyViewToFiveKilometers() {
    QCOMPARE(TopographicCatalog::kCoverageRadiusMeters, 5000.);
    const QgsRectangle survey(199900., 449900., 200200., 450200.);
    const auto cover = TopographicCatalog::coverageBounds(survey);
    QVERIFY(cover.contains(survey));
    QVERIFY(cover.contains(QgsPointXY(204000., 450050.)));
    QVERIFY(!cover.contains(QgsPointXY(208000., 450050.)));
    const QgsRectangle wide(165000., 415000., 235000., 485000.);
    QCOMPARE(TopographicCatalog::coverageBounds(wide), wide);
    QCOMPARE(TopographicCatalog::coverageBounds(QgsRectangle()), QgsRectangle());
  }
};
int main(int argc, char** argv) {
  CPLSetConfigOption("GDAL_PAM_ENABLED", "NO");
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  int result;
  { TopographicCatalogTest tests; result = QTest::qExec(&tests, argc, argv); }
  QgsApplication::exitQgis(); return result;
}
#include "test_topographic_catalog.moc"
