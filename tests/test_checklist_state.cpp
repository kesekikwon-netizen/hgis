// ProjectStateBuilder probes behind the checklist: every control point carries
// its meta, features lie inside the survey polygons, symbols are not survey
// areas, and a composed sheet must show the survey rather than another place.
#include "core/ChecklistEngine.h"
#include "core/ChecklistStateChecks.h"
#include "core/LayerOps.h"
#include "core/LayoutService.h"
#include "core/ProjectStateBuilder.h"
#include "core/SurveyProjectFactory.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#include <qgsapplication.h>
#include <qgsgeometry.h>
#include <qgslayoutitemmap.h>
#include <qgslayoutmanager.h>
#include <qgsprintlayout.h>
#include <qgsproject.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorlayer.h>

namespace {
QString rulesFile() {
  const QStringList candidates = {
      QDir::current().filePath(QStringLiteral("data/rules/drawing_checklist.v1.json")),
      QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../../data/rules/drawing_checklist.v1.json")),
      QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../data/rules/drawing_checklist.v1.json")),
  };
  for (const QString& path : candidates)
    if (QFile::exists(path)) return path;
  return candidates.first();
}

// withoutCrs: a layer with no coordinate system, as an OGR source without SRS gives.
// The memory provider itself falls back to EPSG:4326 when the URI has no crs=, so the
// CRS is cleared afterwards with validation off (validation could otherwise guess one).
QgsVectorLayer* addLayer(QgsProject& project, const QString& uri, const QString& key, bool withoutCrs = false) {
  QgsVectorLayer::LayerOptions options;
  options.skipCrsValidation = withoutCrs;
  auto* layer = new QgsVectorLayer(uri, key, QStringLiteral("memory"), options);
  if (!layer->isValid()) return nullptr;
  if (withoutCrs) layer->setCrs(QgsCoordinateReferenceSystem());
  LayerOps::markSurveyLayer(layer, key);
  project.addMapLayer(layer);
  return layer;
}

void addFeature(QgsVectorLayer* layer, const QString& wkt, const QVariantMap& attributes = {}) {
  QgsFeature f(layer->fields());
  for (auto it = attributes.cbegin(); it != attributes.cend(); ++it) f.setAttribute(it.key(), it.value());
  f.setGeometry(QgsGeometry::fromWkt(wkt));
  QgsFeatureList list{f};
  layer->dataProvider()->addFeatures(list);
  layer->updateExtents();
}

QStringList offenderLabels(const QJsonObject& st, const QString& key) {
  QStringList labels;
  for (const QJsonValue& v : st.value(QStringLiteral("offenders")).toObject().value(key).toArray())
    labels << v.toObject().value(QStringLiteral("label")).toString();
  return labels;
}

const QString kSquare = QStringLiteral("POLYGON((200000 450000,200100 450000,200100 450100,200000 450100,200000 450000))");
}  // namespace

class TestChecklistState : public QObject {
  Q_OBJECT
private slots:
  void everyControlPointNeedsMeta() {
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    auto* cp = addLayer(project, QStringLiteral("Point?crs=EPSG:5187&field=point_id:string&field=datum:string"
                                                "&field=ellipsoid:string&field=projection:string"),
                        QStringLiteral("control_points"));
    QVERIFY(cp);
    const QVariantMap full{{QStringLiteral("point_id"), QStringLiteral("G1")}, {QStringLiteral("datum"), QStringLiteral("세계측지계")},
                           {QStringLiteral("ellipsoid"), QStringLiteral("GRS80")}, {QStringLiteral("projection"), QStringLiteral("TM")}};
    addFeature(cp, QStringLiteral("POINT(200010 450010)"), full);
    QVariantMap noDatum = full;
    noDatum.insert(QStringLiteral("point_id"), QStringLiteral("G2"));
    noDatum.remove(QStringLiteral("datum"));
    addFeature(cp, QStringLiteral("POINT(200020 450020)"), noDatum);
    const QJsonObject st = ProjectStateBuilder::fromProject(&project);
    QVERIFY2(!st.value(QStringLiteral("has_datum")).toBool(), "one point without datum must fail");
    QVERIFY(st.value(QStringLiteral("has_ellipsoid")).toBool());
    QCOMPARE(offenderLabels(st, QStringLiteral("has_datum")), QStringList{QStringLiteral("G2")});
    // Same point_id twice is reported, the first one is not.
    addFeature(cp, QStringLiteral("POINT(200030 450030)"), full);
    const QJsonObject dup = ProjectStateBuilder::fromProject(&project);
    QVERIFY(!dup.value(QStringLiteral("control_points_point_id_unique")).toBool());
    QCOMPARE(offenderLabels(dup, QStringLiteral("control_points_point_id_unique")), QStringList{QStringLiteral("G1")});
  }

  void symbolsAreNotSurveyAreas_data() {
    QTest::addColumn<QString>("wkt");
    QTest::addColumn<bool>("marker");
    QTest::newRow("rectangle") << kSquare << false;
    QTest::newRow("irregular") << QStringLiteral("POLYGON((0 0,40 0,55 30,20 60,-5 35,0 0))") << false;
    QTest::newRow("triangle") << QStringLiteral("POLYGON((0 0,50 0,25 40,0 0))") << true;
    QTest::newRow("point") << QStringLiteral("POINT(10 10)") << true;
    QTest::newRow("circle") << QgsGeometry::fromWkt(QStringLiteral("POINT(100 100)")).buffer(30, 8).asWkt() << true;
  }

  void symbolsAreNotSurveyAreas() {
    QFETCH(QString, wkt);
    QFETCH(bool, marker);
    QCOMPARE(ChecklistStateChecks::isAbstractMarker(QgsGeometry::fromWkt(wkt)), marker);
  }

  void featuresMustLieInsideTheSurveyPolygon() {
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    auto* sa = addLayer(project, QStringLiteral("Polygon?crs=EPSG:5187"), QStringLiteral("survey_area"));
    auto* fp = addLayer(project, QStringLiteral("Polygon?crs=EPSG:5187&field=feature_no:string"), QStringLiteral("feature_poly"));
    auto* ap = addLayer(project, QStringLiteral("Point?crs=EPSG:5187&field=artifact_no:string"), QStringLiteral("artifact_point"));
    QVERIFY(sa && fp && ap);
    addFeature(sa, kSquare);
    addFeature(fp, QStringLiteral("POLYGON((200010 450010,200030 450010,200030 450030,200010 450030,200010 450010))"),
               {{QStringLiteral("feature_no"), QStringLiteral("1호")}});
    QVERIFY(ProjectStateBuilder::fromProject(&project).value(QStringLiteral("features_within_survey")).toBool());
    // Half outside: the old bounding-box test passed this.
    addFeature(fp, QStringLiteral("POLYGON((200090 450010,200130 450010,200130 450030,200090 450030,200090 450010))"),
               {{QStringLiteral("feature_no"), QStringLiteral("2호")}});
    addFeature(ap, QStringLiteral("POINT(200500 450500)"), {{QStringLiteral("artifact_no"), QStringLiteral("A-9")}});
    const QJsonObject st = ProjectStateBuilder::fromProject(&project);
    QVERIFY(!st.value(QStringLiteral("features_within_survey")).toBool());
    QCOMPARE(offenderLabels(st, QStringLiteral("features_within_survey")),
             (QStringList{QStringLiteral("2호"), QStringLiteral("A-9")}));
  }

  void featureLayersHoldRealShapesAndCrs() {
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5179")));
    auto* pointsAsFeatures = addLayer(project, QStringLiteral("Point?crs=EPSG:5187"), QStringLiteral("feature_poly"));
    auto* noCrs = addLayer(project, QStringLiteral("LineString"), QStringLiteral("feature_line"), /*withoutCrs=*/true);
    QVERIFY(pointsAsFeatures && noCrs);
    QVERIFY2(!noCrs->crs().isValid(), "the probe needs a layer that really has no CRS");
    addFeature(pointsAsFeatures, QStringLiteral("POINT(200010 450010)"));
    addFeature(noCrs, QStringLiteral("LINESTRING(0 0,10 10)"));
    const QJsonObject st = ProjectStateBuilder::fromProject(&project);
    QVERIFY(!st.value(QStringLiteral("features_real_geometry")).toBool());
    QVERIFY(!st.value(QStringLiteral("export_crs_valid")).toBool());
    QCOMPARE(offenderLabels(st, QStringLiteral("export_crs_valid")), QStringList{QStringLiteral("feature_line")});
    QCOMPARE(st.value(QStringLiteral("project_crs_authid")).toString(), QStringLiteral("EPSG:5179"));
    ChecklistEngine engine;
    QVERIFY2(engine.loadRules(rulesFile()), qPrintable(rulesFile()));
    QStringList failed;
    for (const CheckResult& r : engine.evaluate(st))
      if (!r.passed) failed << r.id;
    QVERIFY(failed.contains(QStringLiteral("FEATURE_NO_SYMBOL_ONLY")));
    QVERIFY(failed.contains(QStringLiteral("EXPORT_SHP_READY")));
    QVERIFY(failed.contains(QStringLiteral("CRS_WORK_BELT")));
  }

  void emptyRequiredFieldsAreReported() {
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    auto* sa = addLayer(project, QStringLiteral("Polygon?crs=EPSG:5187&field=survey_name:string"), QStringLiteral("survey_area"));
    auto* fl = addLayer(project, QStringLiteral("LineString?crs=EPSG:5187&field=kind:string"), QStringLiteral("feature_line"));
    QVERIFY(sa && fl);
    addFeature(sa, kSquare, {{QStringLiteral("survey_name"), QStringLiteral("광령리")}});
    addFeature(fl, QStringLiteral("LINESTRING(200010 450010,200020 450020)"));
    const QJsonObject st = ProjectStateBuilder::fromProject(&project);
    QVERIFY(!st.value(QStringLiteral("required_fields_filled")).toBool());
    QVERIFY(st.value(QStringLiteral("notes")).toObject().value(QStringLiteral("required_fields_filled")).toString().contains(QStringLiteral("kind")));
  }

  // QGIS can fail its own GeoPackage count and a count goes stale when another layer object saves
  // (CI 2026-10-03); the submit state must still count the survey area and see its polygon.
  void staleCountStillCountsTheSurveyArea() {
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
      addFeature(&writer, QStringLiteral("Polygon((200000 450000, 200100 450000, 200100 450100, 200000 450100, 200000 450000))"));
    }
    QVERIFY(!LayoutService::createBlankSheet(&project, 297.0, 210.0, QStringLiteral("user_sheet"), &error).isEmpty());
    auto* ly = dynamic_cast<QgsPrintLayout*>(project.layoutManager()->layoutByName(QStringLiteral("user_sheet")));
    QVERIFY(ly);
    auto* map = new QgsLayoutItemMap(ly);
    map->setId(QStringLiteral("ka_map"));
    map->attemptSetSceneRect(QRectF(20.0, 20.0, 120.0, 80.0));
    map->setCrs(project.crs());
    map->setKeepLayerSet(true);
    map->setLayers({shown});
    map->zoomToExtent(QgsRectangle(199990.0, 449990.0, 200110.0, 450110.0));
    ly->addLayoutItem(map);
    const QJsonObject st = ProjectStateBuilder::fromProject(&project);
    QCOMPARE(st.value(QStringLiteral("survey_area_count")).toInt(), 1);
    QVERIFY(st.value(QStringLiteral("survey_is_polygon")).toBool());
    QVERIFY(st.value(QStringLiteral("layout_exists:site_location")).toBool());
  }

  void sheetOfAnotherPlaceDoesNotPass() {
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    auto* sa = addLayer(project, QStringLiteral("Polygon?crs=EPSG:5187"), QStringLiteral("survey_area"));
    QVERIFY(sa);
    addFeature(sa, kSquare);
    QString err;
    QVERIFY(!LayoutService::createBlankSheet(&project, 297.0, 210.0, QStringLiteral("user_sheet"), &err).isEmpty());
    auto* ly = dynamic_cast<QgsPrintLayout*>(project.layoutManager()->layoutByName(QStringLiteral("user_sheet")));
    QVERIFY(ly);
    auto* map = new QgsLayoutItemMap(ly);
    map->setId(QStringLiteral("ka_map"));
    map->attemptSetSceneRect(QRectF(20.0, 20.0, 120.0, 80.0));
    map->setCrs(project.crs());
    map->setKeepLayerSet(true);
    map->setLayers({sa});
    map->zoomToExtent(QgsRectangle(300000.0, 550000.0, 300200.0, 550160.0));  // 100 km away
    ly->addLayoutItem(map);
    QVERIFY(LayoutService::isComposedStudioSheet(&project));
    const QJsonObject away = ProjectStateBuilder::fromProject(&project);
    QVERIFY2(!away.value(QStringLiteral("layout_exists:site_location")).toBool(),
             "a composed sheet of another place must not count as the site location drawing");
    QVERIFY(!away.value(QStringLiteral("sheet_has_elements")).toBool());
    QVERIFY(away.value(QStringLiteral("notes")).toObject().value(QStringLiteral("sheet_has_elements")).toString().contains(QStringLiteral("방위")));
    map->zoomToExtent(QgsRectangle(199990.0, 449990.0, 200110.0, 450110.0));
    const QJsonObject here = ProjectStateBuilder::fromProject(&project);
    QVERIFY(here.value(QStringLiteral("layout_exists:site_location")).toBool());
    QVERIFY(here.value(QStringLiteral("sheet_covers_targets")).toBool());
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  const QString prefix = qEnvironmentVariable("QGIS_PREFIX_PATH");
  if (!prefix.isEmpty()) QgsApplication::setPrefixPath(prefix, true);
  QgsApplication::initQgis();
  TestChecklistState test;
  const int rc = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}

#include "test_checklist_state.moc"
