// 저장이 원본 조사 파일을 제자리에서 바꾸지 못해 옆에 「이름-저장.gpkg」를 만들던 일(2026-10-02 조사 R67).
// 앱 안에서 원본을 연 채 남은 손잡이 때문이었다(진단 줄: 「ka-hgis.exe · 이 앱」).
// 원인 1: 레이어 목록에서 지운 조사 레이어를 Ctrl+Z 를 위해 붙들어 두면 그 레이어가 원본을 계속 연다.
// 원인 2(가설): 레이어 하나짜리 파일을 읽은 뒤 편집 없이 저장하면 QGIS 연결 풀의 읽기 손잡이가 늦게 닫힌다.
// 원인 3: 다 그린 지도 그리기 작업이 지워지기 전까지 읽기 손잡이를 쥔다(readerAwaitingDeletion_…).
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "core/LayerOps.h"
#include "core/SurveyHandles.h"
#include "core/SurveyProjectFactory.h"
#include "core/SurveyStorage.h"

#include <qgsapplication.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgsmaplayerstyle.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

namespace {

bool addArea(QgsVectorLayer* layer, double x) {
  if (!layer->isEditable() && !layer->startEditing()) return false;
  QgsFeature feature(layer->fields());
  feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(x, 450000, x + 10, 450010)));
  return layer->addFeature(feature);
}

void readAll(QgsVectorLayer* layer) {  // 지도가 그릴 때처럼 도형을 한 번 읽는다
  QgsFeature f;
  QgsFeatureIterator it = layer->getFeatures();
  while (it.nextFeature(f)) {
  }
}

QString newSurvey(const QTemporaryDir& dir, const QString& name) {
  QString error;
  return SurveyProjectFactory::createNewSurvey(dir.path(), name, &error, QStringLiteral("EPSG:5186"));
}

bool savedInPlace(const SurveyStorage::PersistAttempt& attempt, const QString& gpkg) {
  return attempt.saved &&
         QFileInfo(attempt.surveyPath).absoluteFilePath().compare(QFileInfo(gpkg).absoluteFilePath(),
                                                                  Qt::CaseInsensitive) == 0;
}

}  // namespace

class TestSaveHandles : public QObject {
  Q_OBJECT
 private slots:
  void removedLayerHeldForUndo_doesNotBlockTheSave() {
    QTemporaryDir dir;
    const QString gpkg = newSurvey(dir, QStringLiteral("지움"));
    QVERIFY(!gpkg.isEmpty());
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    QString error;
    auto* area = LayerOps::ensureDomainLayer(&project, gpkg, QStringLiteral("survey_area"), QStringLiteral("조사구역"), &error);
    auto* poly = LayerOps::ensureDomainLayer(&project, gpkg, QStringLiteral("feature_poly"), QStringLiteral("유구면"), &error);
    QVERIFY2(area && poly, qPrintable(error));
    QVERIFY(addArea(area, 200000));
    QVERIFY(savedInPlace(SurveyStorage::persistWorkspace(&project, gpkg, dir.filePath(QStringLiteral("복구사본"))), gpkg));
    readAll(area);
    QgsMapLayerStyle style;
    style.readFromLayer(area);
    // 레이어 목록에서 지우면 Ctrl+Z 로 되살리려고 붙들어 두되, 조사 파일 손잡이는 놓는다(MainWindowUndo).
    std::unique_ptr<QgsMapLayer> held(SurveyHandles::release(project.takeMapLayer(area), gpkg));
    QVERIFY(addArea(poly, 200100));
    const auto attempt = SurveyStorage::persistWorkspace(&project, gpkg, dir.filePath(QStringLiteral("복구사본")));
    QVERIFY2(savedInPlace(attempt, gpkg), qPrintable(attempt.surveyPath + QLatin1Char(' ') + attempt.error));
    QVERIFY(!QFileInfo::exists(dir.filePath(QStringLiteral("지움-저장.gpkg"))));
    // Ctrl+Z: 지금 조사 파일의 그 표를 같은 모양으로 다시 연다.
    auto* back = qobject_cast<QgsVectorLayer*>(SurveyHandles::reattach(held.release(), attempt.surveyPath));
    QVERIFY(back && project.addMapLayer(back));
    QVERIFY(back->isValid());
    QCOMPARE(back->featureCount(), 1LL);
    QgsMapLayerStyle after;
    after.readFromLayer(back);
    QCOMPARE(after.xmlData(), style.xmlData());
  }

  void singleLayerReadThenSaveWithoutEdits_replacesInPlace() {
    QTemporaryDir dir;
    const QString gpkg = newSurvey(dir, QStringLiteral("하나"));
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    QString error;
    auto* area = LayerOps::ensureDomainLayer(&project, gpkg, QStringLiteral("survey_area"), QStringLiteral("조사구역"), &error);
    QVERIFY2(area, qPrintable(error));
    QVERIFY(addArea(area, 200000));
    QVERIFY(savedInPlace(SurveyStorage::persistWorkspace(&project, gpkg, dir.filePath(QStringLiteral("복구사본"))), gpkg));
    readAll(area);
    project.setDirty(true);
    const auto attempt = SurveyStorage::persistWorkspace(&project, gpkg, dir.filePath(QStringLiteral("복구사본")));
    QVERIFY2(savedInPlace(attempt, gpkg), qPrintable(attempt.surveyPath + QLatin1Char(' ') + attempt.error));
  }

  // 원인 3(2026-10-03 확인): 다 그린 지도 그리기 작업은 「나중에 지우기」로 남아 지워질 때까지 조사 파일의 읽기
  // 손잡이를 쥔다. 닫기 물음에서 「저장」을 누르면 저장이 그 정리 차례보다 먼저 파일을 바꾸려다 거부당했다.
  void readerAwaitingDeletion_doesNotBlockTheSave() {
    QTemporaryDir dir;
    const QString gpkg = newSurvey(dir, QStringLiteral("그린뒤"));
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    QString error;
    auto* area = LayerOps::ensureDomainLayer(&project, gpkg, QStringLiteral("survey_area"), QStringLiteral("조사구역"), &error);
    QVERIFY2(area, qPrintable(error));
    QVERIFY(addArea(area, 200000));
    QVERIFY(savedInPlace(SurveyStorage::persistWorkspace(&project, gpkg, dir.filePath(QStringLiteral("복구사본"))), gpkg));
    auto* finishedRender = new QgsVectorLayer(area->source(), QStringLiteral("그리기 사본"), QStringLiteral("ogr"));
    QVERIFY(finishedRender->isValid());
    readAll(finishedRender);
    finishedRender->deleteLater();
    QVERIFY(addArea(area, 200100));
    const auto attempt = SurveyStorage::persistWorkspace(&project, gpkg, dir.filePath(QStringLiteral("복구사본")));
    QVERIFY2(savedInPlace(attempt, gpkg), qPrintable(attempt.surveyPath + QLatin1Char(' ') + attempt.error));
    QVERIFY(!QFileInfo::exists(dir.filePath(QStringLiteral("그린뒤-저장.gpkg"))));
  }
};

int main(int argc, char** argv) {
  QStandardPaths::setTestModeEnabled(true);
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  int result = 0;
  {
    TestSaveHandles tests;
    result = QTest::qExec(&tests, argc, argv);
  }
  QgsApplication::exitQgis();
  return result;
}

#include "test_save_handles.moc"
