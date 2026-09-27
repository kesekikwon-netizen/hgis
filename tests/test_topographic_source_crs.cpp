#include <QtTest>
#include <qgsapplication.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransform.h>
#include <qgsproject.h>
#include "core/TopographicSheets.h"
#include "core/TopographicSourceCrs.h"
#include <limits>
#include <QTemporaryDir>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <memory>
#include <future>

namespace {
TopographicCatalog::Record fixture(const QString& sheetNo, const QString& crs, bool point = false) {
  TopographicCatalog::Record record;
  const auto sheet = TopographicSheets::resolve(sheetNo);
  if (!sheet) return record;
  auto bounds = sheet->geographicExtent;
  bounds.scale(.8);
  QgsCoordinateTransform transform(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4737")),
                                    QgsCoordinateReferenceSystem(crs), QgsCoordinateTransformContext());
  record.extent = point ? QgsRectangle(transform.transform(bounds.center()), transform.transform(bounds.center()))
                        : transform.transformBoundingBox(bounds);
  record.hasExtent = true;
  record.category = TopographicCatalog::Category::Contour;
  return record;
}
}
class TopographicSourceCrsTest : public QObject {
  Q_OBJECT
private slots:
  void shiftedCoastalFrameIsIndependentCrsEvidence_data() {
    QTest::addColumn<QString>("condition");QTest::addColumn<bool>("accepted");
    for(const auto& name:{"shifted-frame","extra-edge-vertices","damaged-edge","offset-frame","outside-data","duplicate-frame","unclosed-frame","no-frame"})
      QTest::newRow(name)<<QString::fromLatin1(name)<<(QByteArray(name)=="shifted-frame" || QByteArray(name)=="extra-edge-vertices" || QByteArray(name)=="offset-frame");
  }
  void shiftedCoastalFrameIsIndependentCrsEvidence() {
    QFETCH(QString,condition);QFETCH(bool,accepted);
    QTemporaryDir dir;QTemporaryDir cache;QVERIFY(dir.isValid());
    const auto geographic=QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4737"));
    const auto source=QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186"));
    QgsCoordinateTransform toSource(geographic,source,QgsCoordinateTransformContext());
    QgsCoordinateTransform toOfficial(geographic,QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5179")),QgsCoordinateTransformContext());
    const auto official=toOfficial.transformBoundingBox(QgsRectangle(126.375,33.375,126.5,33.5));
    const QVector<QgsPointXY> corners={{126.375,33.4},{126.5,33.4},{126.5,33.525},{126.375,33.525},{126.375,33.4}};
    QVector<QgsPointXY> points;
    for(int side=0;side<4;++side)for(int i=0;i<5;++i) {
      const double t=double(i)/5.;
      points.append(toSource.transform(QgsPointXY(corners[side].x()*(1-t)+corners[side+1].x()*t,
                                                  corners[side].y()*(1-t)+corners[side+1].y()*t)));
      if(condition==QLatin1String("extra-edge-vertices") && side==0)
        points.append(toSource.transform(QgsPointXY(corners[side].x()+.025*i+.0083,33.4)));
    }
    points.append(points.first());
    if(condition==QLatin1String("damaged-edge"))points[7].setX(points[7].x()+5.);
    if(condition==QLatin1String("offset-frame"))for(auto& p:points)p.setY(p.y()+365.);
    if(condition==QLatin1String("unclosed-frame"))points.removeLast();
    const QString path=dir.filePath(QStringLiteral("336064.dxf"));
    GDALAllRegister();auto* driver=GetGDALDriverManager()->GetDriverByName("DXF");QVERIFY(driver);
    {
      std::unique_ptr<GDALDataset,decltype(&GDALClose)> ds(driver->Create(path.toUtf8().constData(),0,0,0,GDT_Unknown,nullptr),GDALClose);QVERIFY(ds);
      auto* layer=ds->CreateLayer("entities",nullptr,wkbUnknown,nullptr);QVERIFY(layer);
      const auto add=[&](const char* code,const QVector<QgsPointXY>& vertices){
        std::unique_ptr<OGRFeature,decltype(&OGRFeature::DestroyFeature)> feature(OGRFeature::CreateFeature(layer->GetLayerDefn()),OGRFeature::DestroyFeature);
        feature->SetField("Layer",code);OGRLineString line;for(const auto& p:vertices)line.addPoint(p.x(),p.y());
        return feature->SetGeometry(&line)==OGRERR_NONE && layer->CreateFeature(feature.get())==OGRERR_NONE;
      };
      if(condition!=QLatin1String("no-frame"))QVERIFY(add("H0017334",points));
      if(condition==QLatin1String("duplicate-frame"))QVERIFY(add("H0017334",points));
      auto upper=toSource.transform(QgsPointXY(126.48,condition==QLatin1String("outside-data")?33.55:33.52));
      QVERIFY(add("F0017111",{toSource.transform(QgsPointXY(126.4,33.49)),upper}));
    }
    const auto scan=TopographicCatalog::scan(dir.path(),nullptr,cache.path());QVERIFY2(scan.error.isEmpty(),qPrintable(scan.error));
    const auto context=QgsProject::instance()->transformContext();
    QTest::failOnWarning(QRegularExpression(QStringLiteral("QgsProject::.*different thread.*")));
    // Production resolves downloaded files on a worker; constructing a display
    // layer there must not read style/relations from the GUI-owned project.
    const auto result=std::async(std::launch::async,[records=scan.records,context,official] {
      return TopographicSourceCrs::resolve(QStringLiteral("336064"),QStringLiteral("GRS80"),records,context,official);
    }).get();
    if(!accepted){QVERIFY(!result.error.isEmpty());QVERIFY(result.authId.isEmpty());return;}
    QVERIFY2(result.error.isEmpty(),qPrintable(result.error));QCOMPARE(result.authId,QStringLiteral("EPSG:5186"));
    QVERIFY(result.evidence.contains(QStringLiteral("원본 도곽")));
    QVERIFY(result.evidence.contains(QStringLiteral("색인")));
    QVERIFY(result.unverifiedRecordIndexes.isEmpty());
  }
  void jejuFrameSeveralKilometresFromTheIndexSelects5186() {
    QTemporaryDir dir;
    QTemporaryDir cache;
    QVERIFY(dir.isValid());
    const auto geographic = QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4737"));
    const auto source = QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186"));
    QgsCoordinateTransform toSource(geographic, source, QgsCoordinateTransformContext());
    QgsCoordinateTransform toOfficial(geographic, QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5179")),
                                      QgsCoordinateTransformContext());
    const auto official = toOfficial.transformBoundingBox(QgsRectangle(126.375, 33.375, 126.5, 33.5));
    // About 4.6 km east of the index. A quarter of the sheet is only ~3 km.
    const QVector<QgsPointXY> corners = {{126.425, 33.375}, {126.55, 33.375}, {126.55, 33.5},
                                         {126.425, 33.5}, {126.425, 33.375}};
    QVector<QgsPointXY> points;
    for (int side = 0; side < 4; ++side) {
      for (int i = 0; i < 5; ++i) {
        const double t = double(i) / 5.;
        points.append(toSource.transform(QgsPointXY(corners[side].x() * (1 - t) + corners[side + 1].x() * t,
                                                    corners[side].y() * (1 - t) + corners[side + 1].y() * t)));
      }
    }
    points.append(points.first());
    const QString path = dir.filePath(QStringLiteral("336064.dxf"));
    GDALAllRegister();
    auto* driver = GetGDALDriverManager()->GetDriverByName("DXF");
    QVERIFY(driver);
    {
      std::unique_ptr<GDALDataset, decltype(&GDALClose)> ds(
          driver->Create(path.toUtf8().constData(), 0, 0, 0, GDT_Unknown, nullptr), GDALClose);
      QVERIFY(ds);
      auto* layer = ds->CreateLayer("entities", nullptr, wkbUnknown, nullptr);
      QVERIFY(layer);
      const auto add = [&](const char* code, const QVector<QgsPointXY>& vertices) {
        std::unique_ptr<OGRFeature, decltype(&OGRFeature::DestroyFeature)> feature(
            OGRFeature::CreateFeature(layer->GetLayerDefn()), OGRFeature::DestroyFeature);
        feature->SetField("Layer", code);
        OGRLineString line;
        for (const auto& p : vertices) line.addPoint(p.x(), p.y());
        return feature->SetGeometry(&line) == OGRERR_NONE && layer->CreateFeature(feature.get()) == OGRERR_NONE;
      };
      QVERIFY(add("H0017334", points));
      QVERIFY(add("F0017111", {toSource.transform(QgsPointXY(126.51, 33.42)),
                               toSource.transform(QgsPointXY(126.53, 33.46))}));
    }
    const auto scan = TopographicCatalog::scan(dir.path(), nullptr, cache.path());
    QVERIFY2(scan.error.isEmpty(), qPrintable(scan.error));
    const auto result = TopographicSourceCrs::resolve(QStringLiteral("336064"), QStringLiteral("GRS80"),
                                                      scan.records, {}, official);
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QCOMPARE(result.authId, QStringLiteral("EPSG:5186"));
    QVERIFY(result.warning.contains(QStringLiteral("336064")));
    QVERIFY(result.unverifiedRecordIndexes.isEmpty());

    QTemporaryDir farDir;
    QTemporaryDir farCache;
    QVERIFY(farDir.isValid());
    QVERIFY(farCache.isValid());
    QVector<QgsPointXY> far;
    for (const auto& point : points) far.append(QgsPointXY(point.x() + 60000., point.y()));
    const QString farPath = farDir.filePath(QStringLiteral("336064.dxf"));
    {
      std::unique_ptr<GDALDataset, decltype(&GDALClose)> ds(
          driver->Create(farPath.toUtf8().constData(), 0, 0, 0, GDT_Unknown, nullptr), GDALClose);
      QVERIFY(ds);
      auto* layer = ds->CreateLayer("entities", nullptr, wkbUnknown, nullptr);
      QVERIFY(layer);
      const auto add = [&](const char* code, const QVector<QgsPointXY>& vertices) {
        std::unique_ptr<OGRFeature, decltype(&OGRFeature::DestroyFeature)> feature(
            OGRFeature::CreateFeature(layer->GetLayerDefn()), OGRFeature::DestroyFeature);
        feature->SetField("Layer", code);
        OGRLineString line;
        for (const auto& p : vertices) line.addPoint(p.x(), p.y());
        return feature->SetGeometry(&line) == OGRERR_NONE && layer->CreateFeature(feature.get()) == OGRERR_NONE;
      };
      QVERIFY(add("H0017334", far));
      QVERIFY(add("F0017111", {far.at(1), far.at(6)}));
    }
    const auto farScan = TopographicCatalog::scan(farDir.path(), nullptr, farCache.path());
    QVERIFY2(farScan.error.isEmpty(), qPrintable(farScan.error));
    const auto rejected = TopographicSourceCrs::resolve(QStringLiteral("336064"), QStringLiteral("GRS80"),
                                                        farScan.records, {}, official);
    QVERIFY(!rejected.error.isEmpty());
    QVERIFY(rejected.authId.isEmpty());
  }
  void choosesNorthingAndZone_data() {
    QTest::addColumn<QString>("sheet"); QTest::addColumn<QString>("auth");
    QTest::newRow("central-600k") << QStringLiteral("367154") << QStringLiteral("EPSG:5186");
    QTest::newRow("central-500k") << QStringLiteral("367154") << QStringLiteral("EPSG:5181");
    QTest::newRow("east-600k") << QStringLiteral("378044") << QStringLiteral("EPSG:5187");
    QTest::newRow("east-500k") << QStringLiteral("378044") << QStringLiteral("EPSG:5183");
    QTest::newRow("jeju-550k") << QStringLiteral("336064") << QStringLiteral("EPSG:5182");
    QTest::newRow("jeju-600k") << QStringLiteral("336064") << QStringLiteral("EPSG:5186");
    QTest::newRow("west-of-128") << QStringLiteral("377164") << QStringLiteral("EPSG:5186");
    QTest::newRow("east-of-128") << QStringLiteral("378133") << QStringLiteral("EPSG:5187");
  }
  void choosesNorthingAndZone() {
    QFETCH(QString, sheet); QFETCH(QString, auth);
    for (const bool point : {false, true}) {
      const auto record = fixture(sheet, auth, point);
      const auto result = TopographicSourceCrs::resolve(sheet, QStringLiteral(" grs80 "), {record}, {});
      QVERIFY2(result.error.isEmpty(), qPrintable(result.error)); QCOMPARE(result.authId, auth);
      QVERIFY(!result.crsWkt.isEmpty()); QVERIFY(!result.evidence.isEmpty());
      QVERIFY(result.unverifiedRecordIndexes.isEmpty());
    }
  }
  void rejectsMissingLegacyAndWrongSheet() {
    const QString sheet = QStringLiteral("367154");
    auto record = fixture(sheet, QStringLiteral("EPSG:5186"));
    for (const auto& projection : {QString(), QStringLiteral("BESSEL"), QStringLiteral("Tokyo"), QStringLiteral("GRS80 EPSG:5186")}) {
      const auto result = TopographicSourceCrs::resolve(sheet, projection, {record}, {});
      QVERIFY(!result.error.isEmpty()); QVERIFY(result.authId.isEmpty());
    }
    record.extent = QgsRectangle(record.extent.xMinimum() + 30000, record.extent.yMinimum(),
                                 record.extent.xMaximum() + 30000, record.extent.yMaximum());
    QVERIFY(!TopographicSourceCrs::resolve(sheet, QStringLiteral("GRS80"), {record}, {}).error.isEmpty());
    QVERIFY(!TopographicSourceCrs::resolve(QStringLiteral("bad"), QStringLiteral("GRS80"), {record}, {}).error.isEmpty());
  }
  void preservesMetadataAndFlagsUnverified() {
    const QString sheet = QStringLiteral("367154");
    auto good = fixture(sheet, QStringLiteral("EPSG:5186"));
    good.crsWkt = QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5181")).toWkt();
    QVERIFY(!TopographicSourceCrs::resolve(sheet, QStringLiteral("GRS80"), {good}, {}).error.isEmpty());
    good.crsWkt.clear();
    auto bad = good; bad.category = TopographicCatalog::Category::Unknown;
    bad.extent = QgsRectangle(0, 0, 1, 1);
    const auto result = TopographicSourceCrs::resolve(sheet, QStringLiteral("GRS80"), {good, bad}, {});
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QCOMPARE(result.unverifiedRecordIndexes, QList<int>{1});
    bad.category = TopographicCatalog::Category::Building;
    QVERIFY(!TopographicSourceCrs::resolve(sheet, QStringLiteral("GRS80"), {good, bad}, {}).error.isEmpty());
  }
  void rejectsMissingAndNonFiniteGeometry() {
    TopographicCatalog::Record record;
    QVERIFY(!TopographicSourceCrs::resolve(QStringLiteral("367154"), QStringLiteral("GRS80"), {record}, {}).error.isEmpty());
    record.hasExtent = true; record.category = TopographicCatalog::Category::Contour;
    record.extent = QgsRectangle(0, 0, std::numeric_limits<double>::infinity(), 5);
    QVERIFY(!TopographicSourceCrs::resolve(QStringLiteral("367154"), QStringLiteral("GRS80"), {record}, {}).error.isEmpty());
  }
  void edgeMarginAndEveryPrimaryLayerAreChecked() {
    const QString sheet = QStringLiteral("367154");
    const auto resolved = TopographicSheets::resolve(sheet); QVERIFY(resolved.has_value());
    QgsCoordinateTransform transform(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4737")),
                                      QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")), QgsCoordinateTransformContext());
    const auto projected = transform.transformBoundingBox(resolved->geographicExtent);
    auto edge = fixture(sheet, QStringLiteral("EPSG:5186"));
    const auto y = projected.center().y();
    edge.extent = QgsRectangle(projected.xMaximum() + 99., y, projected.xMaximum() + 99., y);
    QVERIFY(TopographicSourceCrs::resolve(sheet, QStringLiteral("GRS80"), {edge}, {}).error.isEmpty());
    edge.extent = QgsRectangle(projected.xMaximum() + 101., y, projected.xMaximum() + 101., y);
    const auto center = fixture(sheet, QStringLiteral("EPSG:5186"));
    QVERIFY(!TopographicSourceCrs::resolve(sheet, QStringLiteral("GRS80"), {center, edge}, {}).error.isEmpty());
    const auto wrongZone = fixture(sheet, QStringLiteral("EPSG:5187"));
    QVERIFY(!TopographicSourceCrs::resolve(sheet, QStringLiteral("GRS80"), {wrongZone}, {}).error.isEmpty());
  }
  void officialGosanBoundsOverrideNominalGrid() {
    // Official indexQuery/suchiQuery 336101 (Gosan, 2024) in EPSG:5179.
    // This footprint is substantially southwest of the sheet-number grid.
    const QgsRectangle official(871973.078, 1478540.7787, 883778.9408, 1492547.0542);
    for (const auto& auth : {QStringLiteral("EPSG:5182"), QStringLiteral("EPSG:5186")}) {
      QgsCoordinateTransform transform(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5179")),
                                       QgsCoordinateReferenceSystem(auth), QgsCoordinateTransformContext());
      TopographicCatalog::Record record;
      record.category = TopographicCatalog::Category::Contour; record.hasExtent = true;
      record.extent = transform.transformBoundingBox(official); record.extent.scale(.85);
      QVERIFY(!TopographicSourceCrs::resolve(QStringLiteral("336101"), QStringLiteral("GRS80"), {record}, {}).error.isEmpty());
      const auto result = TopographicSourceCrs::resolve(QStringLiteral("336101"), QStringLiteral("GRS80"), {record}, {}, official);
      QVERIFY2(result.error.isEmpty(), qPrintable(result.error)); QCOMPARE(result.authId, auth);
      QVERIFY(result.evidence.contains(QStringLiteral("실제 공식 도곽")));
    }
  }
  void officialCenterSelectsZoneAcrossNominalSeam() {
    // Synthetic official inventory footprint just west of 128 degrees, while
    // sheet 378133 has a nominal center east of the seam.
    const QgsRectangle actualGeographic(127.975, 37.03, 127.999, 37.08);
    QgsCoordinateTransform toOfficial(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4737")),
                                      QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5179")), QgsCoordinateTransformContext());
    QgsCoordinateTransform toSource(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5179")),
                                    QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")), QgsCoordinateTransformContext());
    const auto official = toOfficial.transformBoundingBox(actualGeographic);
    auto record = fixture(QStringLiteral("378133"), QStringLiteral("EPSG:5186"));
    record.extent = toSource.transformBoundingBox(official); record.extent.scale(.8);
    const auto result = TopographicSourceCrs::resolve(QStringLiteral("378133"), QStringLiteral("GRS80"), {record}, {}, official);
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error)); QCOMPARE(result.authId, QStringLiteral("EPSG:5186"));
  }
  void explicitInvalidOrUnrelatedOfficialBoundsNeverFallBack() {
    const auto record = fixture(QStringLiteral("367154"), QStringLiteral("EPSG:5186"));
    QVERIFY(TopographicSourceCrs::resolve(QStringLiteral("367154"), QStringLiteral("GRS80"), {record}, {}).error.isEmpty());
    for (const auto& bounds : {QgsRectangle(), QgsRectangle(0, 0, 1, 1),
                              QgsRectangle(871973.078, 1478540.7787, 883778.9408, 1492547.0542),
                              QgsRectangle(0, 0, 100000, 100000)}) {
      const auto result = TopographicSourceCrs::resolve(QStringLiteral("367154"), QStringLiteral("GRS80"), {record}, {}, bounds);
      QVERIFY(!result.error.isEmpty()); QVERIFY(result.authId.isEmpty());
    }
  }
};
int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  int result;
  { TopographicSourceCrsTest tests; result = QTest::qExec(&tests, argc, argv); }
  QgsApplication::exitQgis(); return result;
}
#include "test_topographic_source_crs.moc"
