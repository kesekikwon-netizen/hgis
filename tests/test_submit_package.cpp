// Submission package integrity: no silent CRS guess, no dropped fields, a
// re-opened SHP check, provenance in README and cancel without leftovers.
#include "core/ExportService.h"
#include "core/HeritageStyle.h"
#include "core/LayerOps.h"
#include "core/LayoutService.h"
#include "core/SubmitReadme.h"
#include "core/SubmitShp.h"
#include "core/SurveyProjectFactory.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <qgsapplication.h>
#include <qgsgeometry.h>
#include <qgslayoutitemmap.h>
#include <qgslayoutmanager.h>
#include <qgsprintlayout.h>
#include <qgsproject.h>
#include <qgsproviderregistry.h>
#include <qgsrasterlayer.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorlayer.h>

namespace {
// withoutCrs: a layer with no coordinate system, as an OGR source without SRS gives.
// The memory provider itself falls back to EPSG:4326 when the URI has no crs=, so the
// CRS is cleared afterwards with validation off (validation could otherwise guess one).
QgsVectorLayer* surveyLayer(QgsProject& project, const QString& uri, const QString& name, bool withoutCrs = false) {
  QgsVectorLayer::LayerOptions options;
  options.skipCrsValidation = withoutCrs;
  auto* layer = new QgsVectorLayer(uri, name, QStringLiteral("memory"), options);
  if (!layer->isValid()) return nullptr;
  if (withoutCrs) layer->setCrs(QgsCoordinateReferenceSystem());
  LayerOps::markSurveyLayer(layer, QStringLiteral("survey_area"));
  project.addMapLayer(layer);
  return layer;
}

void addSquare(QgsVectorLayer* layer, double x, double y, const QVariantMap& attributes = {}) {
  QgsFeature f(layer->fields());
  for (auto it = attributes.cbegin(); it != attributes.cend(); ++it) f.setAttribute(it.key(), it.value());
  f.setGeometry(QgsGeometry::fromRect(QgsRectangle(x, y, x + 50, y + 50)));
  QgsFeatureList list{f};
  layer->dataProvider()->addFeatures(list);
  layer->updateExtents();
}

QgsLayoutItemMap* composeSheet(QgsProject& project, const QList<QgsMapLayer*>& layers) {
  QString err;
  if (LayoutService::createBlankSheet(&project, 297.0, 210.0, QStringLiteral("user_sheet"), &err).isEmpty())
    return nullptr;
  auto* ly = dynamic_cast<QgsPrintLayout*>(project.layoutManager()->layoutByName(QStringLiteral("user_sheet")));
  auto* map = new QgsLayoutItemMap(ly);
  map->setId(QStringLiteral("ka_map"));
  map->attemptSetSceneRect(QRectF(20.0, 20.0, 120.0, 80.0));
  map->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
  map->setKeepLayerSet(true);
  map->setLayers(layers);
  map->zoomToExtent(QgsRectangle(199990.0, 449990.0, 200600.0, 450600.0));
  ly->addLayoutItem(map);
  return map;
}

QString readAll(const QString& path) {
  QFile f(path);
  return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}
}  // namespace

class TestSubmitPackage : public QObject {
  Q_OBJECT
private slots:
  void layerWithoutCrsStopsThePackage() {
    QTemporaryDir temp;
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    auto* area = surveyLayer(project, QStringLiteral("Polygon"), QStringLiteral("조사구역"), /*withoutCrs=*/true);
    QVERIFY(area);
    QVERIFY2(!area->crs().isValid(), "the package test needs a layer that really has no CRS");
    addSquare(area, 200000, 450000);
    QVERIFY(composeSheet(project, {area}));
    QVERIFY(LayoutService::isComposedStudioSheet(&project));
    QString error;
    const QString out = temp.filePath(QStringLiteral("pkg"));
    QVERIFY(ExportService::exportSubmissionPackage(&project, out, QStringLiteral("UTF-8"), QStringLiteral("OK"),
                                                   true, false, &error).isEmpty());
    QVERIFY2(error.contains(QStringLiteral("좌표계")), qPrintable(error));
    QVERIFY(!QFileInfo::exists(out));
  }

  // A GeoPackage count can fail in GDAL ("unable to open database file", CI 2026-10-03) and goes
  // stale when another layer object saves; the sheet check must still see the saved survey area.
  void sheetSeesSurveyAreaTheLayerCountMissed() {
    QTemporaryDir temp;
    QString error;
    const QString gpkg = SurveyProjectFactory::createNewSurvey(temp.path(), QStringLiteral("count"), &error);
    QVERIFY2(!gpkg.isEmpty(), qPrintable(error));
    const QString uri = gpkg + QStringLiteral("|layername=survey_area");
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    auto* shown = new QgsVectorLayer(uri, QStringLiteral("survey_area"), QStringLiteral("ogr"));
    QVERIFY(shown->isValid());
    project.addMapLayer(shown);
    QCOMPARE(shown->featureCount(), 0LL);  // counted while empty
    {
      QgsVectorLayer writer(uri, QStringLiteral("writer"), QStringLiteral("ogr"));
      addSquare(&writer, 200000, 450000);
    }
    QVERIFY(composeSheet(project, {shown}));
    QVERIFY(LayoutService::isComposedStudioSheet(&project));
  }

  void mergeKeepsFieldsOfEveryLayerAndReadmeRecordsProvenance() {
    QTemporaryDir temp;
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    auto* first = surveyLayer(project, QStringLiteral("Polygon?crs=EPSG:5187&field=name:string(40)"), QStringLiteral("조사구역"));
    auto* second = surveyLayer(project, QStringLiteral("Polygon?crs=EPSG:5187&field=name:string(40)&field=memo:string(40)"),
                               QStringLiteral("조사구역 2"));
    QVERIFY(first && second);
    addSquare(first, 200000, 450000, {{QStringLiteral("name"), QStringLiteral("1구역")}});
    addSquare(second, 200500, 450500, {{QStringLiteral("name"), QStringLiteral("2구역")}, {QStringLiteral("memo"), QStringLiteral("둘째만")}});
    QVERIFY(composeSheet(project, {first, second}));
    const QString surveyFile = temp.filePath(QStringLiteral("광령리.gpkg"));
    {
      QFile f(surveyFile);
      QVERIFY(f.open(QIODevice::WriteOnly));
      f.write("survey bytes");
    }
    SubmitPackageInfo info;
    info.surveyName = QStringLiteral("광령리");
    info.surveyPath = surveyFile;
    info.unsavedEditsIncluded = true;
    int calls = 0;
    info.progress = [&calls](int done, int total, const QString&) { ++calls; return done <= total; };
    QString error;
    const QString out = temp.filePath(QStringLiteral("pkg"));
    QCOMPARE(ExportService::exportSubmissionPackage(&project, out, QStringLiteral("UTF-8"), QStringLiteral("OK\n"),
                                                    true, false, &error, info), out);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(calls > 5);
    QgsVectorLayer written(QDir(out).filePath(QStringLiteral("survey_area.shp")), QStringLiteral("w"), QStringLiteral("ogr"));
    QVERIFY(written.isValid());
    QCOMPARE(written.featureCount(), 2LL);
    QVERIFY2(written.fields().indexOf(QStringLiteral("memo")) >= 0, "a field only the second layer has was dropped");
    const QString readme = readAll(QDir(out).filePath(QStringLiteral("README_submit.txt")));
    QVERIFY(readme.contains(QStringLiteral("app: Strata ")));
    QVERIFY(readme.contains(QStringLiteral("survey_name: 광령리")));
    QVERIFY(readme.contains(QStringLiteral("survey_file: 광령리.gpkg")));
    const QString sha = QString::fromLatin1(QCryptographicHash::hash("survey bytes", QCryptographicHash::Sha256).toHex());
    QVERIFY(readme.contains(QStringLiteral("survey_sha256: %1").arg(sha)));
    QVERIFY(readme.contains(QStringLiteral("saved_state: unsaved_edits_included")));
    QVERIFY(readme.contains(QStringLiteral("survey_area.shp features=2 crs=EPSG:5179 verified")));
    QVERIFY(readme.contains(QStringLiteral("reference_data")));
  }

  void cancelLeavesNoPackageAndNoStaging() {
    QTemporaryDir temp;
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    auto* area = surveyLayer(project, QStringLiteral("Polygon?crs=EPSG:5187"), QStringLiteral("조사구역"));
    addSquare(area, 200000, 450000);
    QVERIFY(composeSheet(project, {area}));
    SubmitPackageInfo info;
    info.progress = [](int done, int, const QString&) { return done < 3; };
    QString error;
    const QString out = temp.filePath(QStringLiteral("pkg"));
    QVERIFY(ExportService::exportSubmissionPackage(&project, out, QStringLiteral("UTF-8"), QStringLiteral("OK"),
                                                   true, false, &error, info).isEmpty());
    QVERIFY2(error.contains(QStringLiteral("취소")), qPrintable(error));
    QVERIFY(!QFileInfo::exists(out));
    QVERIFY(QDir(temp.path()).entryList({QStringLiteral(".ka-hgis-export-*")},
                                        QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty());
  }

  void blockedAndManifestMessagesAreKorean() {
    QString error;
    QVERIFY(ExportService::exportSubmissionPackage(nullptr, QDir::temp().filePath(QStringLiteral("ka_block_ko")),
                                                   QStringLiteral("UTF-8"), QStringLiteral("x"), true, true, &error).isEmpty());
    QVERIFY2(error.contains(QStringLiteral("검수 오류")), qPrintable(error));
    QVERIFY(!ExportService::writeSha256Manifest(QDir::temp().filePath(QStringLiteral("ka-no-such-folder-4ad522")), &error));
    QVERIFY2(error.contains(QStringLiteral("폴더")), qPrintable(error));
  }

  void koreanFieldNamesCutOnCodePoints() {
    QCOMPARE(SubmitShp::leftUtf8(QStringLiteral("조사구역이름"), 10), QStringLiteral("조사구"));
    QSet<QString> used;
    const QString a = SubmitShp::fieldNameFor(QStringLiteral("유구종류메모"), used);
    const QString b = SubmitShp::fieldNameFor(QStringLiteral("유구종류메모2"), used);
    for (const QString& name : {a, b}) {
      QVERIFY(name.toUtf8().size() <= 10);
      QVERIFY(!name.contains(QChar::ReplacementCharacter));
    }
    QVERIFY(a != b);
  }

  void verifyWrittenCatchesAWrongCount() {
    QTemporaryDir temp;
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    auto* area = surveyLayer(project, QStringLiteral("Polygon?crs=EPSG:5187"), QStringLiteral("조사구역"));
    addSquare(area, 200000, 450000);
    QVERIFY(composeSheet(project, {area}));
    QString error;
    const QString out = temp.filePath(QStringLiteral("pkg"));
    QCOMPARE(ExportService::exportSubmissionPackage(&project, out, QStringLiteral("UTF-8"), QStringLiteral("OK"),
                                                    true, false, &error), out);
    const QString shp = QDir(out).filePath(QStringLiteral("survey_area.shp"));
    QVERIFY2(SubmitShp::verifyWritten(shp, 1, &error), qPrintable(error));
    QVERIFY(!SubmitShp::verifyWritten(shp, 2, &error));
    QVERIFY2(error.contains(QStringLiteral("2")), qPrintable(error));
  }

  void referenceProvenanceNamesHostNeverKey() {
    if (!QgsProviderRegistry::instance()->providerMetadata(QStringLiteral("wms")))
      QSKIP("wms provider is not loaded in this environment");
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    auto* area = surveyLayer(project, QStringLiteral("Polygon?crs=EPSG:5187"), QStringLiteral("조사구역"));
    addSquare(area, 200000, 450000);
    auto* tiles = new QgsRasterLayer(
        QStringLiteral("type=xyz&url=https://api.vworld.kr/req/wmts/1.0.0/SECRETKEY123/Satellite/{z}/{y}/{x}.jpeg&zmax=19&zmin=0"),
        QStringLiteral("VWorld 위성"), QStringLiteral("wms"));
    LayerOps::markReferenceLayer(tiles);
    project.addMapLayer(tiles, false);
    auto* heritage = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187"),
                                        HeritageStyle::layerName(HeritageStyle::allDatasets().first()), QStringLiteral("memory"));
    LayerOps::markReferenceLayer(heritage);
    project.addMapLayer(heritage, false);
    QVERIFY(composeSheet(project, {area, heritage, tiles}));
    const QString lines = SubmitReadme::referenceLines(&project, {QStringLiteral("user_sheet")}).join(QLatin1Char('\n'));
    QVERIFY2(lines.contains(QStringLiteral("api.vworld.kr")), qPrintable(lines));
    QVERIFY2(!lines.contains(QStringLiteral("SECRETKEY123")), "an API key must never reach the package");
    QVERIFY2(lines.contains(QStringLiteral("배포 제한")), qPrintable(lines));
    QVERIFY(!lines.contains(QStringLiteral("조사구역")));
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  const QString prefix = qEnvironmentVariable("QGIS_PREFIX_PATH");
  if (!prefix.isEmpty()) {
    QgsApplication::setPrefixPath(prefix, true);
    QgsApplication::setPluginPath(prefix + QStringLiteral("/plugins"));
  }
  QgsApplication::initQgis();
  TestSubmitPackage test;
  const int rc = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}

#include "test_submit_package.moc"
