// Reference-layer identity, provider policy, DSM naming and the Korea extent cache
// (evaluation F035, F059, F062, F205). Core only; no network: XYZ layers are created,
// never fetched.
#include <QtTest>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <gdal_priv.h>
#include <ogr_spatialref.h>

#include <qgsapplication.h>
#include <qgslayertree.h>
#include <qgsmaplayerserverproperties.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>
#include <qgsvectorlayer.h>

#include <vector>

#include "core/BasemapDsm.h"
#include "core/BasemapPolicy.h"
#include "core/LayerOps.h"
#include "core/ReferenceKind.h"
#include "core/VworldSettings.h"

namespace {
QgsVectorLayer* memoryLayer(const QString& title) {
  return new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"), title, QStringLiteral("memory"));
}

bool legendChecked(QgsProject& project, const QgsMapLayer* layer) {
  const QgsLayerTreeLayer* node = project.layerTreeRoot()->findLayer(layer->id());
  return node && node->itemVisibilityChecked();
}

QString writeTinyDem(const QString& path) {
  GDALDriver* drv = GetGDALDriverManager()->GetDriverByName("GTiff");
  if (!drv) return {};
  GDALDataset* ds = drv->Create(path.toUtf8().constData(), 8, 8, 1, GDT_Float32, nullptr);
  if (!ds) return {};
  double gt[6] = {200000.0, 10.0, 0.0, 450000.0, 0.0, -10.0};
  ds->SetGeoTransform(gt);
  OGRSpatialReference srs;
  srs.SetFromUserInput("EPSG:5186");
  char* wkt = nullptr;
  srs.exportToWkt(&wkt);
  if (wkt) {
    ds->SetProjection(wkt);
    CPLFree(wkt);
  }
  std::vector<float> z(64);
  for (int i = 0; i < 64; ++i) z[static_cast<size_t>(i)] = 20.0f + float(i);
  const CPLErr written = ds->GetRasterBand(1)->RasterIO(GF_Write, 0, 0, 8, 8, z.data(), 8, 8,
                                                        GDT_Float32, 0, 0);
  GDALClose(ds);
  return written == CE_None ? path : QString();
}
}  // namespace

class TestBasemapReference : public QObject {
  Q_OBJECT
private slots:
  void policy_offlinePackOnlyForAllowedProviders() {
    using namespace BasemapPolicy;
    const QString vworld = QStringLiteral(
        "type=xyz&url=https://api.vworld.kr/req/wmts/1.0.0/KEY/Satellite/%7Bz%7D/%7By%7D/%7Bx%7D.jpeg&zmax=19");
    const QString google = QStringLiteral(
        "type=xyz&url=https://mt1.google.com/vt/lyrs%3Ds%26x%3D%7Bx%7D%26y%3D%7By%7D%26z%3D%7Bz%7D&zmax=20");
    QCOMPARE(providerForUrl(vworld), Provider::VWorld);
    QCOMPARE(providerForUrl(google), Provider::Google);
    QCOMPARE(providerForUrl(QStringLiteral("https://tile.openstreetmap.org/{z}/{x}/{y}.png")),
             Provider::OpenStreetMap);
    QCOMPARE(providerForUrl(QStringLiteral("https://a.tile.opentopomap.org/{z}/{x}/{y}.png")),
             Provider::OpenTopoMap);
    QCOMPARE(providerForUrl(QStringLiteral("https://evilvworld.kr/x.png")), Provider::Other);
    QCOMPARE(providerForUrl(QStringLiteral("<GDAL_WMS><Service name=\"TMS\"><ServerUrl>"
                                           "https://api.vworld.kr/req/wmts/1.0.0/K/Base</ServerUrl>")),
             Provider::VWorld);
    QCOMPARE(providerForUrl(QStringLiteral("IgnoreGetMapUrl=1&url=https%3A%2F%2Fapi.vworld.kr%2Freq")),
             Provider::VWorld);

    const OfflineDecision v = offlineCachingForUrl(vworld);
    QVERIFY(v.allowed && !v.askFirst);
    const OfflineDecision g = offlineCachingForUrl(google);
    QVERIFY(!g.allowed);
    QVERIFY2(g.message.contains(QStringLiteral("Google")), qPrintable(g.message));
    QVERIFY(!offlineCachingForUrl(QStringLiteral("https://tile.openstreetmap.org/1/1/1.png")).allowed);
    QVERIFY(!offlineCachingForUrl(QStringLiteral("https://basemaps.cartocdn.com/light_all/1/1/1.png")).allowed);
    const OfflineDecision other = offlineCachingForUrl(QStringLiteral("https://tiles.example.org/{z}/{x}/{y}.png"));
    QVERIFY(other.allowed && other.askFirst && !other.message.isEmpty());

    // Referer authenticates VWorld keys only; OSM, Google and local servers get none.
    QCOMPARE(refererForUrl(QStringLiteral("https://xdworld.vworld.kr/2d/Base/service/1/1/1.png")),
             QStringLiteral("https://localhost"));
    QVERIFY(refererForUrl(google).isEmpty());
    QVERIFY(refererForUrl(QStringLiteral("https://tile.openstreetmap.org/1/1/1.png")).isEmpty());
    QVERIFY(refererForUrl(QStringLiteral("http://127.0.0.1:8080/1/1/1.png")).isEmpty());

    QgsRasterLayer googleLayer(google, QStringLiteral("Google 위성"), QStringLiteral("wms"));
    QVERIFY(!offlineCaching(&googleLayer).allowed);
    QVERIFY(!drawingNotice(&googleLayer).isEmpty());
    QgsRasterLayer vworldLayer(vworld, QStringLiteral("VWorld 위성"), QStringLiteral("wms"));
    QVERIFY(offlineCaching(&vworldLayer).allowed && !offlineCaching(&vworldLayer).askFirst);
    QVERIFY(drawingNotice(&vworldLayer).isEmpty());
    QVERIFY(!offlineCaching(nullptr).allowed || offlineCaching(nullptr).askFirst);

    QgsProject project;
    auto* shown = new QgsRasterLayer(google, QStringLiteral("Google 위성"), QStringLiteral("wms"));
    QVERIFY(project.addMapLayer(shown));
    QVERIFY(!drawingNoticeForProject(&project).isEmpty());
    project.layerTreeRoot()->findLayer(shown->id())->setItemVisibilityChecked(false);
    QVERIFY(drawingNoticeForProject(&project).isEmpty());
  }

  void kind_toggleFollowsRenamedLayer() {
    QgsProject project;
    auto* dem = memoryLayer(QStringLiteral("DEM"));
    LayerOps::markReferenceLayer(dem);
    ReferenceKind::tag(dem, QString::fromLatin1(ReferenceKind::kDem));
    QVERIFY(project.addMapLayer(dem));
    dem->setName(QStringLiteral("우리 조사지 표고"));
    QVERIFY(LayerOps::isLayerVisible(&project, QStringLiteral("DEM")));
    QVERIFY(LayerOps::toggleLayerVisibility(&project, nullptr, QStringLiteral("DEM"), false));
    QVERIFY(!legendChecked(project, dem));
    QVERIFY(!LayerOps::isLayerVisible(&project, QStringLiteral("DEM")));
    QVERIFY(LayerOps::toggleLayerVisibility(&project, nullptr, BasemapDsm::copernicusTitle(), true));
    QVERIFY(legendChecked(project, dem));
    QVERIFY(LayerOps::setLayerOpacity(&project, nullptr, QStringLiteral("DEM"), 0.4));
    QCOMPARE(dem->opacity(), 0.4);
    QVERIFY(BasemapDsm::isDemLayer(dem));
  }

  void kind_userLayerWithSimilarTitleIsLeftAlone() {
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    auto* reading = memoryLayer(QStringLiteral("위성사진_판독"));
    LayerOps::markReferenceLayer(reading);  // imported picture tracing, no layer_key
    auto* survey = memoryLayer(QStringLiteral("위성 판독 유구"));
    LayerOps::markSurveyLayer(survey, QStringLiteral("user:위성 판독 유구"));
    QVERIFY(project.addMapLayer(reading));
    QVERIFY(project.addMapLayer(survey));
    QString err;
    QVERIFY2(LayerOps::addVworldSatelliteMap(&project, nullptr, QStringLiteral("UNITTEST_KEY"), &err),
             qPrintable(err));
    QVERIFY2(project.mapLayer(reading->id()) && project.mapLayer(survey->id()),
             "adding the satellite must not replace a user layer titled 위성…");
    const auto satellites = ReferenceKind::tagged(&project, QString::fromLatin1(ReferenceKind::kSatellite));
    QCOMPARE(satellites.size(), 1);
    QgsMapLayer* satellite = satellites.first();

    QVERIFY(LayerOps::toggleLayerVisibility(&project, nullptr, QStringLiteral("VWorld 위성"), false));
    QVERIFY(!legendChecked(project, satellite));
    QVERIFY(legendChecked(project, reading) && legendChecked(project, survey));

    LayerOps::pruneDuplicateSatelliteLayers(&project);
    QVERIFY(project.mapLayer(reading->id()) && project.mapLayer(survey->id()));

    // Renamed satellite: a second click finds it instead of stacking another copy.
    satellite->setName(QStringLiteral("우리 위성"));
    QVERIFY(LayerOps::addVworldSatelliteMap(&project, nullptr, QStringLiteral("UNITTEST_KEY"), &err));
    QCOMPARE(ReferenceKind::tagged(&project, QString::fromLatin1(ReferenceKind::kSatellite)).size(), 1);

    QVERIFY(LayerOps::isolateSurfaceSurveyView(&project, nullptr, nullptr));
    QVERIFY(legendChecked(project, satellite));
    QVERIFY2(!legendChecked(project, reading), "surface view shows our satellite, not every 위성 title");
  }

  void kind_legacyProjectFoundByOldTitle() {
    QgsProject project;
    auto* geology = memoryLayer(QStringLiteral("지질도(KIGAM 1:5만)"));
    LayerOps::markReferenceLayer(geology);
    auto* reading = memoryLayer(QStringLiteral("위성사진_판독"));
    LayerOps::markReferenceLayer(reading);
    QVERIFY(project.addMapLayer(geology));
    QVERIFY(project.addMapLayer(reading));
    QCOMPARE(ReferenceKind::find(&project, QString::fromLatin1(ReferenceKind::kGeology)),
             QList<QgsMapLayer*>{geology});
    QVERIFY(LayerOps::toggleLayerVisibility(&project, nullptr, QStringLiteral("지질도"), false));
    QVERIFY(!legendChecked(project, geology));
    // Folding "…위성…" to 위성 is for live tiles only; a user picture never counts.
    QVERIFY(ReferenceKind::find(&project, QString::fromLatin1(ReferenceKind::kSatellite)).isEmpty());
    QVERIFY(!LayerOps::isLayerVisible(&project, QStringLiteral("위성")));
    QVERIFY(LayerOps::isLayerVisible(&project, QStringLiteral("위성사진_판독")));
  }

  void dsm_downloadedLayerSaysWhatItIs() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeTinyDem(dir.filePath(QStringLiteral("dem.tif")));
    QVERIFY(!path.isEmpty());
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    QString err;
    QVERIFY2(LayerOps::addDemElevationRaster(&project, nullptr, path, &err), qPrintable(err));
    QgsRasterLayer* dem = BasemapDsm::findDem(&project);
    QVERIFY(dem);
    QCOMPARE(dem->name(), QStringLiteral("DEM"));  // a user's NGII file keeps its plain title
    BasemapDsm::labelCopernicus(dem);
    QCOMPARE(dem->name(), QStringLiteral("지표모델(DSM) 30m · Copernicus"));
    QVERIFY(BasemapDsm::isDemLayer(dem));
    const QString tip = dem->serverProperties()->abstract();
    QVERIFY2(tip.contains(QStringLiteral("DSM")) && tip.contains(QStringLiteral("30 m")) &&
                 tip.contains(QStringLiteral("EPSG:5186")),
             qPrintable(tip));
    QCOMPARE(BasemapDsm::findDem(&project), dem);
    QVERIFY(LayerOps::isLayerVisible(&project, QStringLiteral("DEM")));
    // Downloading again replaces the renamed layer instead of adding a second DEM.
    QVERIFY2(LayerOps::addDemElevationRaster(&project, nullptr, path, &err), qPrintable(err));
    QCOMPARE(ReferenceKind::tagged(&project, QString::fromLatin1(ReferenceKind::kDem)).size(), 1);
    QVERIFY(ReferenceKind::tagged(&project, QString::fromLatin1(ReferenceKind::kDemRelief)).size() <= 1);
  }

  void koreaExtent_sameAnswerFromCache() {
    const QgsRectangle a = LayerOps::koreaExtentForCrs(QStringLiteral("EPSG:5186"));
    const QgsRectangle b = LayerOps::koreaExtentForCrs(QStringLiteral("EPSG:5186"));
    QVERIFY(!a.isEmpty() && a.isFinite());
    QCOMPARE(a, b);
    const QgsRectangle other = LayerOps::koreaExtentForCrs(QStringLiteral("EPSG:5187"));
    QVERIFY(other != a);
    const QgsRectangle fillA = LayerOps::satelliteFillExtentForCrs(QStringLiteral("EPSG:5186"));
    const QgsRectangle fillB = LayerOps::satelliteFillExtentForCrs(QStringLiteral("EPSG:5186"));
    QCOMPARE(fillA, fillB);
    QVERIFY(a.contains(fillA.center()));
    const QgsRectangle merc = LayerOps::satelliteFillExtentForCrs(QStringLiteral("EPSG:3857"));
    QVERIFY(merc.width() > 100000.0);
    // An unknown CRS still answers (WGS84 box) and is not remembered as a real one.
    const QgsRectangle unknown = LayerOps::koreaExtentForCrs(QStringLiteral("EPSG:999999"));
    QCOMPARE(unknown, QgsRectangle(124.5, 33.0, 132.0, 39.5));
  }
};

int main(int argc, char** argv) {
  // Keep the user's settings and keys out of reach: VWorld reads happen in addVworld*.
  QStandardPaths::setTestModeEnabled(true);
  QTemporaryDir isolated;
  if (!isolated.isValid()) return 1;
  if (qEnvironmentVariableIsEmpty("KA_HGIS_LOG_DIR"))
    qputenv("KA_HGIS_LOG_DIR", isolated.filePath(QStringLiteral("logs")).toUtf8());
  QgsApplication app(argc, argv, false, isolated.filePath(QStringLiteral("qgis-profile")));
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, isolated.path());
  VworldSettings::Scope scope;
  scope.folder = isolated.filePath(QStringLiteral("keys"));
  VworldSettings::setScope(scope);
  GDALAllRegister();
  TestBasemapReference test;
  const int rc = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
#include "test_basemap_reference.moc"
