// 제출 패키지가 같은 layer_key 를 가진 레이어를 모두 내보내는지 검증한다.
// 예전 조사구역 대화상자의 「새 조사구역 레이어 만들기」가 survey_area_2, _3 을 같은 키로
// 만들었다(옛 조사에 남아 있다). 예전에는 첫 레이어 하나만 내보내 제출물에서 구역이 빠졌다.
#include "core/ChecklistEngine.h"
#include "core/ExportService.h"
#include "core/LayerOps.h"
#include "core/LayoutService.h"
#include "core/ProjectStateBuilder.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QRectF>
#include <QTemporaryDir>
#include <QVector>
#include <QtTest>

#include <qgsapplication.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgslayoutitemmap.h>
#include <qgslayoutmanager.h>
#include <qgsmaplayer.h>
#include <qgspointxy.h>
#include <qgsprintlayout.h>
#include <qgsproject.h>
#include <qgsrectangle.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorlayer.h>

#include <cmath>
#include <ogr_spatialref.h>

namespace {

QgsVectorLayer* addSurveyAreaLayer(QgsProject* project, const QString& name,
                                   const QString& layerKey = QStringLiteral("survey_area")) {
  auto* layer = new QgsVectorLayer(
      QStringLiteral("Polygon?crs=EPSG:5187&field=name:string(60)"), name,
      QStringLiteral("memory"));
  if (!layer->isValid()) return nullptr;
  LayerOps::markSurveyLayer(layer, layerKey);
  project->addMapLayer(layer);
  return layer;
}

bool addComposedUserSheet(QgsProject* project, QgsMapLayer* mapLayer) {
  if (!project || !mapLayer)
    return false;
  QString err;
  if (LayoutService::createBlankSheet(project, 297.0, 210.0, QStringLiteral("user_sheet"), &err)
          .isEmpty())
    return false;
  auto* ly = dynamic_cast<QgsPrintLayout*>(
      project->layoutManager()->layoutByName(QStringLiteral("user_sheet")));
  if (!ly)
    return false;
  auto* map = new QgsLayoutItemMap(ly);
  map->setId(QStringLiteral("ka_map"));
  map->attemptSetSceneRect(QRectF(20.0, 20.0, 120.0, 80.0));
  map->setCrs(project->crs().isValid() ? project->crs()
                                       : QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  map->setKeepLayerSet(true);
  map->setLayers(QList<QgsMapLayer*>{mapLayer});
  QgsRectangle ext = mapLayer->extent();
  if (ext.isEmpty() || !ext.isFinite())
    ext = QgsRectangle(200000.0, 450000.0, 200200.0, 450160.0);
  map->zoomToExtent(ext);
  if (map->scene() != ly)
    ly->addLayoutItem(map);
  return LayoutService::isComposedStudioSheet(project);
}

bool addSquare(QgsVectorLayer* layer, const QString& label, double x, double y) {
  QgsFeature feature(layer->fields());
  feature.setAttribute(QStringLiteral("name"), label);
  feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(x, y, x + 50, y + 50)));
  QgsFeatureList features{feature};
  if (!layer->dataProvider()->addFeatures(features)) return false;
  layer->updateExtents();
  return true;
}

QString rulesFile() {
  const QStringList candidates = {
    QDir::current().filePath(QStringLiteral("data/rules/drawing_checklist.v1.json")),
    QDir(QCoreApplication::applicationDirPath()).filePath(
        QStringLiteral("../../data/rules/drawing_checklist.v1.json")),
  };
  for (const auto& path : candidates)
    if (QFileInfo::exists(path)) return path;
  return candidates.first();
}

long long featureCountOf(const QString& shpPath) {
  QgsVectorLayer written(shpPath, QStringLiteral("written"), QStringLiteral("ogr"));
  return written.isValid() ? written.featureCount() : -1;
}

bool errorRuleFailed(const QVector<CheckResult>& results, const char* id) {
  for (const CheckResult& result : results) {
    if (result.id == QLatin1String(id))
      return !result.passed && result.severity == QLatin1String("error");
  }
  return false;
}

}  // namespace

class TestExportSurveyAreas : public QObject {
  Q_OBJECT
private slots:
  void everySurveyAreaLayerReachesTheSubmissionShp();
  void emptyFirstLayerDoesNotDropTheSubmissionShp();
  void referenceLayerNamedLikeDomainStaysOut();
  void legacyLayerWithoutKeyStillExports();
  void invalidGeometryInSecondLayerBlocksSubmission();
  void emptyGeometryBlocksSubmission();
  void zeroAreaPolygonBlocksSubmission();
  void longFieldNamesFitTheShapefile();
  void exportShp_matchesProjDirectWithinOneMillimetre_data();
  void exportShp_matchesProjDirectWithinOneMillimetre();
  void exportSubmissionPackage_withoutUserSheetFails();
};

// 구역 레이어가 둘이면 두 도형이 모두 제출 SHP 에 들어가야 한다.
void TestExportSurveyAreas::everySurveyAreaLayerReachesTheSubmissionShp() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  QgsProject project;
  project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));

  auto* first = addSurveyAreaLayer(&project, QStringLiteral("조사구역"));
  QVERIFY(first);
  QVERIFY(addSquare(first, QStringLiteral("1구역"), 200000, 450000));
  auto* second = addSurveyAreaLayer(&project, QStringLiteral("조사구역 2"));
  QVERIFY(second);
  QVERIFY(addSquare(second, QStringLiteral("2구역"), 200500, 450500));
  QCOMPARE(LayerOps::findAllByLayerKey(&project, QStringLiteral("survey_area")).size(), 2);
  QVERIFY(addComposedUserSheet(&project, first));

  const QString output = temporary.filePath(QStringLiteral("submission"));
  QString error;
  QCOMPARE(ExportService::exportSubmissionPackage(&project, output, QStringLiteral("UTF-8"),
                                                  QStringLiteral("OK"), true, false, &error),
           output);
  QVERIFY2(error.isEmpty(), qPrintable(error));

  const QString shp = QDir(output).filePath(QStringLiteral("survey_area.shp"));
  QVERIFY2(QFileInfo::exists(shp), "survey_area.shp 가 없다");
  QCOMPARE(featureCountOf(shp), 2LL);
}

// 먼저 잡히는 레이어가 비어 있어도 다른 레이어의 도형은 제출돼야 한다.
void TestExportSurveyAreas::emptyFirstLayerDoesNotDropTheSubmissionShp() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  QgsProject project;
  project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));

  QVERIFY(addSurveyAreaLayer(&project, QStringLiteral("조사구역")));  // 도형 없음
  auto* drawn = addSurveyAreaLayer(&project, QStringLiteral("조사구역 2"));
  QVERIFY(drawn);
  QVERIFY(addSquare(drawn, QStringLiteral("실제 구역"), 200000, 450000));
  QVERIFY(addComposedUserSheet(&project, drawn));

  const QString output = temporary.filePath(QStringLiteral("submission"));
  QString error;
  QCOMPARE(ExportService::exportSubmissionPackage(&project, output, QStringLiteral("UTF-8"),
                                                  QStringLiteral("OK"), true, false, &error),
           output);
  QVERIFY2(error.isEmpty(), qPrintable(error));

  const QString shp = QDir(output).filePath(QStringLiteral("survey_area.shp"));
  QVERIFY2(QFileInfo::exists(shp), "빈 레이어 때문에 survey_area.shp 가 빠졌다");
  QCOMPARE(featureCountOf(shp), 1LL);
}

// 표시 이름만 도메인 이름으로 바꾼 참조 자료는 제출물에 들어가면 안 된다.
void TestExportSurveyAreas::referenceLayerNamedLikeDomainStaysOut() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  QgsProject project;
  project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));

  auto* survey = addSurveyAreaLayer(&project, QStringLiteral("조사구역"));
  QVERIFY(survey);
  QVERIFY(addSquare(survey, QStringLiteral("1구역"), 200000, 450000));

  // 도메인 키 없이 이름만 feature_poly 인 참조 자료
  auto* reference = new QgsVectorLayer(
      QStringLiteral("Polygon?crs=EPSG:5187&field=name:string(60)"),
      QStringLiteral("feature_poly"), QStringLiteral("memory"));
  QVERIFY(reference->isValid());
  reference->setCustomProperty(QString::fromUtf8(LayerOps::kPropLayerRole),
                               QString::fromUtf8(LayerOps::kRoleReference));
  project.addMapLayer(reference);
  QVERIFY(addSquare(reference, QStringLiteral("주변 유적"), 300000, 460000));
  QVERIFY(LayerOps::layerKeyOf(reference).isEmpty());
  QVERIFY(addComposedUserSheet(&project, survey));

  const QString output = temporary.filePath(QStringLiteral("submission"));
  QString error;
  QCOMPARE(ExportService::exportSubmissionPackage(&project, output, QStringLiteral("UTF-8"),
                                                  QStringLiteral("OK"), true, false, &error),
           output);
  QVERIFY2(error.isEmpty(), qPrintable(error));

  QVERIFY2(!QFileInfo::exists(QDir(output).filePath(QStringLiteral("feature_poly.shp"))),
           "참조 자료가 제출 SHP 로 나갔다");
  QCOMPARE(featureCountOf(QDir(output).filePath(QStringLiteral("survey_area.shp"))), 1LL);
}

// layer_key 가 없는 예전 조사 레이어는 이름으로 찾아 제출해야 한다.
// 이름 폴백을 지우면 예전 조사와 GPKG 에서 바로 연 레이어가 제출물에서 빠진다.
void TestExportSurveyAreas::legacyLayerWithoutKeyStillExports() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  QgsProject project;
  project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));

  auto* legacy = new QgsVectorLayer(
      QStringLiteral("Polygon?crs=EPSG:5187&field=name:string(60)"),
      QStringLiteral("survey_area"), QStringLiteral("memory"));
  QVERIFY(legacy->isValid());
  project.addMapLayer(legacy);
  QVERIFY(LayerOps::layerKeyOf(legacy).isEmpty());
  QVERIFY(addSquare(legacy, QStringLiteral("예전 구역"), 200000, 450000));
  QVERIFY(addComposedUserSheet(&project, legacy));

  const QString output = temporary.filePath(QStringLiteral("submission"));
  QString error;
  QCOMPARE(ExportService::exportSubmissionPackage(&project, output, QStringLiteral("UTF-8"),
                                                  QStringLiteral("OK"), true, false, &error),
           output);
  QVERIFY2(error.isEmpty(), qPrintable(error));
  QCOMPARE(featureCountOf(QDir(output).filePath(QStringLiteral("survey_area.shp"))), 1LL);
}

// 자기교차 도형은 제출 전에 검수에서 막혀야 한다. 두 번째 레이어에 있어도 마찬가지다.
// 예전에는 키마다 레이어를 하나만 확인해서, 두 번째 레이어의 무효 도형이 그대로 나갔다.
void TestExportSurveyAreas::invalidGeometryInSecondLayerBlocksSubmission() {
  QgsProject project;
  project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
  auto* survey = addSurveyAreaLayer(&project, QStringLiteral("조사구역"));
  QVERIFY(survey);
  QVERIFY(addSquare(survey, QStringLiteral("1구역"), 200000, 450000));

  auto* firstFeatures = addSurveyAreaLayer(&project, QStringLiteral("유구_면"),
                                           QStringLiteral("feature_poly"));
  QVERIFY(firstFeatures);
  QVERIFY(addSquare(firstFeatures, QStringLiteral("정상 유구"), 200010, 450010));

  auto* secondFeatures = addSurveyAreaLayer(&project, QStringLiteral("유구_면 2"),
                                            QStringLiteral("feature_poly"));
  QVERIFY(secondFeatures);
  // 나비 모양(자기교차) 폴리곤
  QgsFeature bowtie(secondFeatures->fields());
  bowtie.setAttribute(QStringLiteral("name"), QStringLiteral("자기교차"));
  bowtie.setGeometry(QgsGeometry::fromWkt(QStringLiteral(
      "POLYGON((200100 450100, 200150 450150, 200150 450100, 200100 450150, 200100 450100))")));
  QVERIFY(!bowtie.geometry().isNull());
  QVERIFY(!bowtie.geometry().isGeosValid());
  QgsFeatureList batch{bowtie};
  QVERIFY(secondFeatures->dataProvider()->addFeatures(batch));
  secondFeatures->updateExtents();

  const QJsonObject state = ProjectStateBuilder::fromProject(&project);
  QVERIFY2(!state.value(QStringLiteral("geometries_valid")).toBool(true),
           "두 번째 레이어의 무효 도형을 검수가 놓쳤다");
  QCOMPARE(state.value(QStringLiteral("feature_poly_count")).toInt(), 2);

  ChecklistEngine engine;
  QVERIFY2(engine.loadRules(rulesFile()), qPrintable(rulesFile()));
  QVERIFY2(errorRuleFailed(engine.evaluate(state), "GEOMETRY_VALID"),
           "GEOMETRY_VALID 규칙이 제출을 막지 않았다");
}

// 빈 도형(NULL / EMPTY)은 자기교차와 다른 제출 차단 항목이어야 한다.
void TestExportSurveyAreas::emptyGeometryBlocksSubmission() {
  QgsProject project;
  project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
  auto* survey = addSurveyAreaLayer(&project, QStringLiteral("조사구역"));
  QVERIFY(survey);
  QVERIFY(addSquare(survey, QStringLiteral("1구역"), 200000, 450000));

  auto* features = addSurveyAreaLayer(&project, QStringLiteral("유구_면"),
                                      QStringLiteral("feature_poly"));
  QVERIFY(features);
  QgsFeature empty(features->fields());
  empty.setAttribute(QStringLiteral("name"), QStringLiteral("빈 도형"));
  empty.setGeometry(QgsGeometry());
  QVERIFY(empty.geometry().isNull());
  QVERIFY(empty.geometry().isEmpty());
  QgsFeatureList batch{empty};
  QVERIFY(features->dataProvider()->addFeatures(batch));
  features->updateExtents();

  const QJsonObject state = ProjectStateBuilder::fromProject(&project);
  QVERIFY2(!state.value(QStringLiteral("geometries_nonempty")).toBool(true),
           "빈 도형을 검수가 놓쳤다");
  QVERIFY2(state.value(QStringLiteral("geometries_valid")).toBool(false),
           "빈 도형을 자기교차로 잘못 집계했다");

  ChecklistEngine engine;
  QVERIFY2(engine.loadRules(rulesFile()), qPrintable(rulesFile()));
  const auto results = engine.evaluate(state);
  QVERIFY2(errorRuleFailed(results, "GEOMETRY_NOT_EMPTY"),
           "GEOMETRY_NOT_EMPTY 규칙이 제출을 막지 않았다");
  QVERIFY2(!errorRuleFailed(results, "GEOMETRY_VALID"),
           "빈 도형이 GEOMETRY_VALID 로 표시되면 안 된다");
}

// 면적 0 폴리곤은 별도 제출 차단 항목이어야 한다.
void TestExportSurveyAreas::zeroAreaPolygonBlocksSubmission() {
  QgsProject project;
  project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
  auto* survey = addSurveyAreaLayer(&project, QStringLiteral("조사구역"));
  QVERIFY(survey);
  QVERIFY(addSquare(survey, QStringLiteral("1구역"), 200000, 450000));

  auto* features = addSurveyAreaLayer(&project, QStringLiteral("유구_면"),
                                      QStringLiteral("feature_poly"));
  QVERIFY(features);
  const QgsGeometry collapsed = QgsGeometry::fromWkt(QStringLiteral(
      "POLYGON((200100 450100, 200150 450100, 200200 450100, 200100 450100))"));
  QVERIFY(!collapsed.isNull());
  QVERIFY(!collapsed.isEmpty());
  QVERIFY2(!(collapsed.area() > 0.0),
           qPrintable(QStringLiteral("collapsed area=%1").arg(collapsed.area())));
  QgsFeature zero(features->fields());
  zero.setAttribute(QStringLiteral("name"), QStringLiteral("0면적"));
  zero.setGeometry(collapsed);
  QgsFeatureList batch{zero};
  QVERIFY(features->dataProvider()->addFeatures(batch));
  features->updateExtents();

  const QJsonObject state = ProjectStateBuilder::fromProject(&project);
  QVERIFY2(!state.value(QStringLiteral("geometries_nonzero_area")).toBool(true),
           "0면적 폴리곤을 검수가 놓쳤다");

  ChecklistEngine engine;
  QVERIFY2(engine.loadRules(rulesFile()), qPrintable(rulesFile()));
  QVERIFY2(errorRuleFailed(engine.evaluate(state), "GEOMETRY_NONZERO_AREA"),
           "GEOMETRY_NONZERO_AREA 규칙이 제출을 막지 않았다");
}

void TestExportSurveyAreas::longFieldNamesFitTheShapefile() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  QgsProject project;
  project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
  auto* area = new QgsVectorLayer(
      QStringLiteral("Polygon?crs=EPSG:5187&field=survey_name:string(80)&field=site_name:string(40)"),
      QStringLiteral("조사구역"), QStringLiteral("memory"));
  QVERIFY(area->isValid());
  LayerOps::markSurveyLayer(area, QStringLiteral("survey_area"));
  project.addMapLayer(area);
  QgsFeature areaFeature(area->fields());
  areaFeature.setAttribute(QStringLiteral("survey_name"), QStringLiteral("광령리"));
  areaFeature.setGeometry(QgsGeometry::fromRect(QgsRectangle(200000, 450000, 200050, 450050)));
  QgsFeatureList areaFeatures{areaFeature};
  QVERIFY(area->dataProvider()->addFeatures(areaFeatures));

  auto* points = new QgsVectorLayer(
      QStringLiteral("Point?crs=EPSG:5187&field=artifact_no:string(40)&field=kind:string(20)"),
      QStringLiteral("유물"), QStringLiteral("memory"));
  QVERIFY(points->isValid());
  LayerOps::markSurveyLayer(points, QStringLiteral("artifact_point"));
  project.addMapLayer(points);
  QgsFeature point(points->fields());
  point.setAttribute(QStringLiteral("artifact_no"), QStringLiteral("A-12"));
  point.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(200010, 450010)));
  QgsFeatureList pointFeatures{point};
  QVERIFY(points->dataProvider()->addFeatures(pointFeatures));
  QVERIFY(addComposedUserSheet(&project, area));

  const QString output = temporary.filePath(QStringLiteral("submission"));
  QString error;
  QCOMPARE(ExportService::exportSubmissionPackage(&project, output, QStringLiteral("UTF-8"),
                                                  QStringLiteral("OK"), true, false, &error),
           output);
  QVERIFY2(error.isEmpty(), qPrintable(error));

  QgsVectorLayer writtenArea(QDir(output).filePath(QStringLiteral("survey_area.shp")),
                             QStringLiteral("area"), QStringLiteral("ogr"));
  QgsVectorLayer writtenPoint(QDir(output).filePath(QStringLiteral("artifact_point.shp")),
                              QStringLiteral("point"), QStringLiteral("ogr"));
  QVERIFY(writtenArea.isValid());
  QVERIFY(writtenPoint.isValid());
  for (const QgsField& field : writtenArea.fields())
    QVERIFY2(field.name().toUtf8().size() <= 10, qPrintable(field.name()));
  for (const QgsField& field : writtenPoint.fields())
    QVERIFY2(field.name().toUtf8().size() <= 10, qPrintable(field.name()));
  QVERIFY(writtenArea.fields().indexOf(QStringLiteral("surv_name")) >= 0);
  QVERIFY(writtenArea.fields().indexOf(QStringLiteral("survey_name")) < 0);
  QVERIFY(writtenPoint.fields().indexOf(QStringLiteral("artif_no")) >= 0);
  QgsFeature readArea;
  QVERIFY(writtenArea.getFeatures().nextFeature(readArea));
  QCOMPARE(readArea.attribute(QStringLiteral("surv_name")).toString(), QStringLiteral("광령리"));
  QgsFeature readPoint;
  QVERIFY(writtenPoint.getFeatures().nextFeature(readPoint));
  QCOMPARE(readPoint.attribute(QStringLiteral("artif_no")).toString(), QStringLiteral("A-12"));
  QFile readme(QDir(output).filePath(QStringLiteral("README_submit.txt")));
  QVERIFY(readme.open(QIODevice::ReadOnly));
  const QString text = QString::fromUtf8(readme.readAll());
  QVERIFY(text.contains(QStringLiteral("survey_area survey_name=surv_name")));
  QVERIFY(text.contains(QStringLiteral("artifact_point artifact_no=artif_no")));
}

// GDAL OSR가 부르는 PROJ(proj.db)와 제출 SHP 좌표를 비교한다.
// cs2cs.exe는 이 SDK에 없고, proj.h도 공개 헤더로 없다.
// 공식: https://gdal.org/en/stable/tutorials/osr_api_tut.html
//        https://proj.org/en/stable/development/quickstart.html
bool projDirectTo5179(int sourceEpsg, double x, double y, double* outX, double* outY,
                      QString* error) {
  OGRSpatialReference source;
  OGRSpatialReference target;
  if (source.importFromEPSG(sourceEpsg) != OGRERR_NONE ||
      target.importFromEPSG(5179) != OGRERR_NONE) {
    if (error) *error = QStringLiteral("EPSG:%1 또는 EPSG:5179를 열지 못했습니다.").arg(sourceEpsg);
    return false;
  }
  source.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
  target.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
  OGRCoordinateTransformation* transform = OGRCreateCoordinateTransformation(&source, &target);
  if (!transform) {
    if (error) *error = QStringLiteral("EPSG:%1 → EPSG:5179 변환을 만들지 못했습니다.").arg(sourceEpsg);
    return false;
  }
  *outX = x;
  *outY = y;
  const bool ok = transform->Transform(1, outX, outY) == TRUE;
  OGRCoordinateTransformation::DestroyCT(transform);
  if (!ok) {
    if (error) *error = QStringLiteral("EPSG:%1 (%2, %3) 변환이 실패했습니다.").arg(sourceEpsg).arg(x).arg(y);
    return false;
  }
  return std::isfinite(*outX) && std::isfinite(*outY);
}

void TestExportSurveyAreas::exportShp_matchesProjDirectWithinOneMillimetre_data() {
  QTest::addColumn<QString>("crs");
  QTest::addColumn<int>("epsg");
  QTest::addColumn<double>("x");
  QTest::addColumn<double>("y");
  QTest::newRow("central-5186") << QStringLiteral("EPSG:5186") << 5186 << 198000.0 << 451000.0;
  QTest::newRow("east-5187") << QStringLiteral("EPSG:5187") << 5187 << 200010.0 << 450010.0;
}

void TestExportSurveyAreas::exportShp_matchesProjDirectWithinOneMillimetre() {
  QFETCH(QString, crs);
  QFETCH(int, epsg);
  QFETCH(double, x);
  QFETCH(double, y);
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  QgsProject project;
  project.setCrs(QgsCoordinateReferenceSystem(crs));
  auto* points = new QgsVectorLayer(
      QStringLiteral("Point?crs=%1&field=name:string(20)").arg(crs),
      QStringLiteral("기준점"), QStringLiteral("memory"));
  QVERIFY(points->isValid());
  LayerOps::markSurveyLayer(points, QStringLiteral("control_points"));
  project.addMapLayer(points);
  QgsFeature feature(points->fields());
  feature.setAttribute(QStringLiteral("name"), QStringLiteral("proj-check"));
  feature.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(x, y)));
  QgsFeatureList features{feature};
  QVERIFY(points->dataProvider()->addFeatures(features));
  QVERIFY(addComposedUserSheet(&project, points));

  const QString output = temporary.filePath(QStringLiteral("submission"));
  QString error;
  QCOMPARE(ExportService::exportSubmissionPackage(&project, output, QStringLiteral("UTF-8"),
                                                  QStringLiteral("OK"), true, false, &error),
           output);
  QVERIFY2(error.isEmpty(), qPrintable(error));

  QgsVectorLayer written(QDir(output).filePath(QStringLiteral("control_points.shp")),
                         QStringLiteral("written"), QStringLiteral("ogr"));
  QVERIFY(written.isValid());
  QCOMPARE(written.crs().authid(), QStringLiteral("EPSG:5179"));
  QgsFeature exported;
  QVERIFY(written.getFeatures().nextFeature(exported));
  const QgsPointXY shp = exported.geometry().asPoint();

  double projX = 0;
  double projY = 0;
  QVERIFY2(projDirectTo5179(epsg, x, y, &projX, &projY, &error), qPrintable(error));
  const double dx = shp.x() - projX;
  const double dy = shp.y() - projY;
  const double metres = std::hypot(dx, dy);
  QVERIFY2(metres <= 0.001,
           qPrintable(QStringLiteral("%1 SHP=(%2,%3) PROJ=(%4,%5) d=%6m")
                          .arg(crs)
                          .arg(shp.x(), 0, 'f', 6)
                          .arg(shp.y(), 0, 'f', 6)
                          .arg(projX, 0, 'f', 6)
                          .arg(projY, 0, 'f', 6)
                          .arg(metres, 0, 'f', 6)));
}

void TestExportSurveyAreas::exportSubmissionPackage_withoutUserSheetFails() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  QgsProject project;
  project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
  auto* first = addSurveyAreaLayer(&project, QStringLiteral("조사구역"));
  QVERIFY(first);
  QVERIFY(addSquare(first, QStringLiteral("1구역"), 200000, 450000));

  const QString output = temporary.filePath(QStringLiteral("submission"));
  QString error;
  QVERIFY(ExportService::exportSubmissionPackage(&project, output, QStringLiteral("UTF-8"),
                                                 QStringLiteral("OK"), true, false, &error)
              .isEmpty());
  QVERIFY2(error.contains(QStringLiteral("도면만들기")), qPrintable(error));
  QVERIFY(!QFileInfo::exists(QDir(output).filePath(QStringLiteral("README_submit.txt"))));
}

#include "test_export_survey_areas.moc"

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  const QString prefix = qEnvironmentVariable("QGIS_PREFIX_PATH");
  if (!prefix.isEmpty()) {
    QgsApplication::setPrefixPath(prefix, true);
    QgsApplication::setPluginPath(prefix + QStringLiteral("/plugins"));
  }
  QgsApplication::initQgis();
  TestExportSurveyAreas test;
  const int rc = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
