// 지도가 그리는 동안 새 도엽이 오면 수치지형도 합침 파일을 새로 만든다(그리는 파일을 건드리지 않으려고).
// 예전에는 옛 합침 파일을 한 번만 지워 보고 넘어가, 그리기가 그 파일을 읽고 있던 Windows 에서 지워지지 않았다.
// 도엽이 올 때마다 앞 도엽을 모두 담은 「수치지형도-합침-*.gpkg」가 조사 폴더에 쌓였다(2026-10-03 검토).
#include <QtTest>
#include <QDir>
#include <QFontDatabase>
#include <QSettings>
#include <QTemporaryDir>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <qgsapplication.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsmapcanvas.h>
#include <qgsproject.h>
#include <qgstaskmanager.h>
#include <qgsvectorlayer.h>
#include "app/KaTopographicImportDialog.h"
#include "core/TopographicCatalog.h"
#include <memory>

namespace {
bool fixture(const QString& path, double x) {
  auto* driver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");
  if (!driver) return false;
  std::unique_ptr<GDALDataset, decltype(&GDALClose)> ds(driver->Create(path.toUtf8().constData(), 0, 0, 0, GDT_Unknown, nullptr), GDALClose);
  if (!ds) return false;
  OGRSpatialReference crs; if (crs.importFromEPSG(5186) != OGRERR_NONE) return false;
  auto* layer = ds->CreateLayer("road", &crs, wkbLineString, nullptr);
  if (!layer) return false;
  OGRLineString line; line.addPoint(x, 450000.); line.addPoint(x + 100., 450100.);
  std::unique_ptr<OGRFeature, decltype(&OGRFeature::DestroyFeature)> feature(OGRFeature::CreateFeature(layer->GetLayerDefn()), OGRFeature::DestroyFeature);
  return feature->SetGeometry(&line) == OGRERR_NONE && layer->CreateFeature(feature.get()) == OGRERR_NONE;
}
QgsMapLayer* topographic() {
  for (auto* layer : QgsProject::instance()->mapLayers())
    if (!layer->customProperty(QStringLiteral("ka_hgis/topographic_source")).toString().isEmpty()) return layer;
  return nullptr;
}
}  // namespace

class TopographicMergeTest : public QObject {
  Q_OBJECT
 private slots:
  void sheetArrivingWhileMapDrawsLeavesOneMergedFile() {
    QTemporaryDir first, second, cache, survey;
    QVERIFY(fixture(first.filePath(QStringLiteral("A0010000_1.shp")), 200000.));
    QVERIFY(fixture(first.filePath(QStringLiteral("A0010000_2.shp")), 200050.));
    QVERIFY(fixture(second.filePath(QStringLiteral("A0010000_3.shp")), 200100.));
    auto records = TopographicCatalog::scan(first.path(), nullptr, cache.path()).records;
    auto later = TopographicCatalog::scan(second.path(), nullptr, cache.path()).records;
    QCOMPARE(records.size(), 2); QCOMPARE(later.size(), 1);
    for (auto& record : records) record.sourceSheet = QStringLiteral("37701");
    later[0].sourceSheet = QStringLiteral("37702");
    const QgsCoordinateReferenceSystem crs(records.first().crsWkt);
    QgsProject::instance()->setCrs(crs);
    QgsMapCanvas canvas; canvas.setDestinationCrs(crs);
    canvas.setExtent(QgsRectangle(199900., 449900., 200300., 450300.));
    KaTopographicImportDialog dialog(&canvas);
    const QString mergeFolder = QDir(survey.path()).filePath(QStringLiteral("지형도/합침"));
    dialog.setMergeDirectory(mergeFolder);
    QVERIFY(dialog.importVerified(records));
    QTRY_VERIFY_WITH_TIMEOUT(!dialog.isAutomaticLoading() && topographic(), 10000);
    const QString firstId = topographic()->id();
    // The second sheet arrives while the map still draws (reads) the first merged file. A reader held
    // until the merge is done stands for that render; it lets go a moment later, as a render does.
    auto reader = std::make_unique<QgsVectorLayer>(topographic()->source(), QStringLiteral("render"), QStringLiteral("ogr"));
    QgsFeature feature;
    QgsFeatureIterator reading = reader->getFeatures();
    QVERIFY(reading.nextFeature(feature));
    canvas.refresh();
    QVERIFY(dialog.importVerified(later));
    QTRY_VERIFY_WITH_TIMEOUT(!dialog.isAutomaticLoading() && topographic() &&
                             qobject_cast<QgsVectorLayer*>(topographic())->featureCount() == 3, 10000);
    QVERIFY2(topographic()->id() != firstId, "the map finished drawing before the merge: the rewrite path did not run");
    reading.close();
    reader.reset();
    const QStringList filter{QStringLiteral("수치지형도-합침-*.gpkg")};
    QTRY_COMPARE_WITH_TIMEOUT(QDir(mergeFolder).entryList(filter, QDir::Files).size(), 1, 20000);
    QCOMPARE(QFileInfo(topographic()->source().section(QLatin1Char('|'), 0, 0)).fileName(),
             QDir(mergeFolder).entryList(filter, QDir::Files).first());
    canvas.stopRendering(); canvas.setLayers({}); QgsProject::instance()->clear();
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, true);
  QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/malgun.ttf"));
  app.setOrganizationName(QStringLiteral("ka-hgis-tests"));
  app.setApplicationName(QStringLiteral("topographic-merge"));
  QTemporaryDir settings;
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis(); GDALAllRegister();
  int result = 0;
  {
    TopographicMergeTest test;
    result = QTest::qExec(&test, argc, argv);
  }
  QgsApplication::taskManager()->cancelAll();
  QgsApplication::exitQgis();
  return result;
}

#include "test_topographic_merge.moc"
