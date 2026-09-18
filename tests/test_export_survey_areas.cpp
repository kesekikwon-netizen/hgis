// 제출 패키지가 같은 layer_key 를 가진 레이어를 모두 내보내는지 검증한다.
// 조사구역 대화상자의 「새 조사구역 레이어 만들기」는 survey_area_2, _3 을
// 같은 키로 만든다. 예전에는 첫 레이어 하나만 내보내 제출물에서 구역이 빠졌다.
#include "core/ExportService.h"
#include "core/LayerOps.h"

#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

#include <qgsapplication.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgsproject.h>
#include <qgsrectangle.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorlayer.h>

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

bool addSquare(QgsVectorLayer* layer, const QString& label, double x, double y) {
  QgsFeature feature(layer->fields());
  feature.setAttribute(QStringLiteral("name"), label);
  feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(x, y, x + 50, y + 50)));
  QgsFeatureList features{feature};
  if (!layer->dataProvider()->addFeatures(features)) return false;
  layer->updateExtents();
  return true;
}

long long featureCountOf(const QString& shpPath) {
  QgsVectorLayer written(shpPath, QStringLiteral("written"), QStringLiteral("ogr"));
  return written.isValid() ? written.featureCount() : -1;
}

}  // namespace

class TestExportSurveyAreas : public QObject {
  Q_OBJECT
private slots:
  void everySurveyAreaLayerReachesTheSubmissionShp();
  void emptyFirstLayerDoesNotDropTheSubmissionShp();
  void referenceLayerNamedLikeDomainStaysOut();
  void legacyLayerWithoutKeyStillExports();
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

  const QString output = temporary.filePath(QStringLiteral("submission"));
  QString error;
  QCOMPARE(ExportService::exportSubmissionPackage(&project, output, QStringLiteral("UTF-8"),
                                                  QStringLiteral("OK"), true, false, &error),
           output);
  QVERIFY2(error.isEmpty(), qPrintable(error));
  QCOMPARE(featureCountOf(QDir(output).filePath(QStringLiteral("survey_area.shp"))), 1LL);
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
