#include <QtTest>
#include <qgsapplication.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgsproject.h>
#include <qgsrectangle.h>
#include <qgsvectorlayer.h>

#include "core/LayerOps.h"
#include "core/SurveyScopeClip.h"

class SurveyScopeClipTest : public QObject {
  Q_OBJECT
private slots:
  void noSurveyLeavesFeatures() {
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    QgsVectorLayer layer(QStringLiteral("Point?crs=EPSG:5186&field=nm:string"),
                         QStringLiteral("far"), QStringLiteral("memory"));
    QVERIFY(layer.startEditing());
    QgsFeature f(layer.fields());
    f.setAttribute(0, QStringLiteral("멀리"));
    f.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(300000., 500000.)));
    QVERIFY(layer.addFeature(f));
    QVERIFY(layer.commitChanges());
    QString error;
    QVERIFY(SurveyScopeClip::keepIntersectingIfSurvey(&project, &layer, &error));
    QVERIFY(error.isEmpty());
    QCOMPARE(layer.featureCount(), 1);
  }

  void surveyBufferDropsFarFeature() {
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    auto* survey = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                      QStringLiteral("조사구역"), QStringLiteral("memory"));
    LayerOps::markSurveyLayer(survey, QStringLiteral("survey_area"));
    QVERIFY(survey->startEditing());
    QgsFeature sa(survey->fields());
    sa.setGeometry(QgsGeometry::fromRect(QgsRectangle(200000., 450000., 200050., 450050.)));
    QVERIFY(survey->addFeature(sa));
    QVERIFY(survey->commitChanges());
    project.addMapLayer(survey);

    QgsVectorLayer layer(QStringLiteral("Point?crs=EPSG:5186&field=nm:string"),
                         QStringLiteral("sites"), QStringLiteral("memory"));
    QVERIFY(layer.startEditing());
    QgsFeature near(layer.fields());
    near.setAttribute(0, QStringLiteral("근처"));
    near.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(201000., 450000.)));
    QVERIFY(layer.addFeature(near));
    QgsFeature far(layer.fields());
    far.setAttribute(0, QStringLiteral("멀리"));
    far.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(220000., 470000.)));
    QVERIFY(layer.addFeature(far));
    QVERIFY(layer.commitChanges());
    QCOMPARE(layer.featureCount(), 2);

    QString error;
    QVERIFY2(SurveyScopeClip::keepIntersectingIfSurvey(&project, &layer, &error), qPrintable(error));
    QCOMPARE(layer.featureCount(), 1);
    QgsFeature kept;
    QVERIFY(layer.getFeatures().nextFeature(kept));
    QCOMPARE(kept.attribute(0).toString(), QStringLiteral("근처"));
  }

  void intersectingCopyKeepsNearLine() {
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    auto* survey = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                      QStringLiteral("조사구역"), QStringLiteral("memory"));
    LayerOps::markSurveyLayer(survey, QStringLiteral("survey_area"));
    QVERIFY(survey->startEditing());
    QgsFeature sa(survey->fields());
    sa.setGeometry(QgsGeometry::fromRect(QgsRectangle(200000., 450000., 200020., 450020.)));
    QVERIFY(survey->addFeature(sa));
    QVERIFY(survey->commitChanges());
    project.addMapLayer(survey);

    QgsVectorLayer source(QStringLiteral("LineString?crs=EPSG:5186&field=nm:string"),
                          QStringLiteral("선"), QStringLiteral("memory"));
    QVERIFY(source.startEditing());
    QgsFeature near(source.fields());
    near.setAttribute(0, QStringLiteral("근처"));
    near.setGeometry(QgsGeometry::fromWkt(QStringLiteral("LINESTRING(200010 450010,200040 450040)")));
    QVERIFY(source.addFeature(near));
    QgsFeature far(source.fields());
    far.setAttribute(0, QStringLiteral("멀리"));
    far.setGeometry(QgsGeometry::fromWkt(QStringLiteral("LINESTRING(230000 480000,230100 480100)")));
    QVERIFY(source.addFeature(far));
    QVERIFY(source.commitChanges());

    QString error;
    QgsVectorLayer* copy = SurveyScopeClip::intersectingCopyIfSurvey(&project, &source,
                                                                     QStringLiteral("수치지형도"), &error);
    QVERIFY2(copy, qPrintable(error));
    QVERIFY(copy != &source);
    QCOMPARE(copy->featureCount(), 1);
    QCOMPARE(source.featureCount(), 2);
    delete copy;
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  SurveyScopeClipTest test;
  const int result = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis();
  return result;
}
#include "test_survey_scope_clip.moc"
