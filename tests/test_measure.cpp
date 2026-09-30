#include <cmath>

#include <QtTest>
#include <QFile>
#include <QVector>

#include "core/MeasureOps.h"

#include <qgis.h>
#include <qgsapplication.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransformcontext.h>
#include <qgsdistancearea.h>
#include <qgsexpression.h>
#include <qgsexpressioncontext.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgspointxy.h>
#include <qgsproject.h>

class TestMeasure : public QObject {
  Q_OBJECT
private slots:
  void length10mOn5187();
  void rectangleArea200m2();
  void formatters();
  void planarAreaMatchesLabelSelectAndTape();
  void gridBearingIsClockwiseFromGridNorth();
  void pinPlanarEllipsoidKeepsSavedState();
};

void TestMeasure::length10mOn5187() {
  const QgsCoordinateReferenceSystem crs(QStringLiteral("EPSG:5187"));
  QVERIFY(crs.isValid());
  const QgsCoordinateTransformContext ctx;
  const QVector<QgsPointXY> pts{QgsPointXY(200000.0, 450000.0), QgsPointXY(200010.0, 450000.0)};
  const double len = MeasureOps::lineLengthMeters(pts, crs, ctx);
  QVERIFY2(std::abs(len - 10.0) < 0.02, qPrintable(QString::number(len)));
}

void TestMeasure::rectangleArea200m2() {
  const QgsCoordinateReferenceSystem crs(QStringLiteral("EPSG:5186"));
  QVERIFY(crs.isValid());
  const QgsCoordinateTransformContext ctx;
  const QVector<QgsPointXY> pts{QgsPointXY(200000.0, 450000.0), QgsPointXY(200010.0, 450000.0),
                                QgsPointXY(200010.0, 450020.0), QgsPointXY(200000.0, 450020.0)};
  const double area = MeasureOps::polygonAreaSquareMeters(pts, crs, ctx);
  QVERIFY2(std::abs(area - 200.0) < 0.5, qPrintable(QString::number(area)));
  const double peri = MeasureOps::polygonPerimeterMeters(pts, crs, ctx);
  QVERIFY2(std::abs(peri - 60.0) < 0.2, qPrintable(QString::number(peri)));
}

void TestMeasure::formatters() {
  QCOMPARE(MeasureOps::formatLengthM(1.234), QStringLiteral("1.234 m"));
  QCOMPARE(MeasureOps::formatLengthM(12.3), QStringLiteral("12.30 m"));
  QVERIFY(MeasureOps::formatAreaM2(42.1).contains(QStringLiteral("㎡")));
  QVERIFY(MeasureOps::formatAreaM2(20000.0).contains(QStringLiteral("ha")));
}

void TestMeasure::planarAreaMatchesLabelSelectAndTape() {
  // P2-3: 라벨 area($geometry)·줄자·선택·면적 요약은 모두 작업 CRS 평면 면적.
  const QgsCoordinateReferenceSystem crs(QStringLiteral("EPSG:5186"));
  QVERIFY(crs.isValid());
  QgsProject bare;
  bare.setCrs(crs);
  // 새 조사와 같이 앱이 타원체를 직접 지정하지 않는다.
  QVERIFY2(bare.ellipsoid().isEmpty() || bare.ellipsoid() == Qgis::geoNone() ||
               bare.ellipsoid().compare(QStringLiteral("NONE"), Qt::CaseInsensitive) == 0,
           qPrintable(bare.ellipsoid()));

  const QVector<QgsPointXY> pts{QgsPointXY(200000.0, 450000.0), QgsPointXY(200010.0, 450000.0),
                                QgsPointXY(200010.0, 450020.0), QgsPointXY(200000.0, 450020.0)};
  QgsPolylineXY ring = pts;
  ring.append(pts.first());
  const QgsGeometry geom = QgsGeometry::fromPolygonXY({ring});
  QVERIFY(!geom.isEmpty());

  const QgsCoordinateTransformContext ctx = bare.transformContext();
  const double tape = MeasureOps::polygonAreaSquareMeters(pts, crs, ctx);
  const double selectOrSummary = MeasureOps::geometryAreaSquareMeters(geom, crs, ctx);

  QgsExpression labelExpr(QStringLiteral("area($geometry)"));
  QgsExpressionContext exprCtx;
  auto* scope = new QgsExpressionContextScope();
  scope->setFeature(QgsFeature());
  scope->setGeometry(geom);
  exprCtx.appendScope(scope);
  const QVariant labelVal = labelExpr.evaluate(&exprCtx);
  QVERIFY2(labelVal.isValid(), qPrintable(labelExpr.evalErrorString()));
  const double label = labelVal.toDouble();

  QVERIFY2(std::abs(tape - 200.0) < 0.5, qPrintable(QString::number(tape)));
  QVERIFY2(std::abs(selectOrSummary - tape) < 1e-6,
           qPrintable(QStringLiteral("select=%1 tape=%2").arg(selectOrSummary).arg(tape)));
  QVERIFY2(std::abs(label - tape) < 0.5,
           qPrintable(QStringLiteral("label=%1 tape=%2").arg(label).arg(tape)));

  // 프로젝트에 타원체를 켜도 MeasureOps는 평면을 유지한다(예전 선택/요약은 타원체를 따름).
  QgsProject::instance()->setCrs(crs);
  QgsProject::instance()->setEllipsoid(QStringLiteral("EPSG:7030"));
  QVERIFY(QgsProject::instance()->ellipsoid() != Qgis::geoNone());
  QVERIFY(!QgsProject::instance()->ellipsoid().isEmpty());
  const double stillPlanar = MeasureOps::geometryAreaSquareMeters(
      geom, crs, QgsProject::instance()->transformContext());
  QVERIFY2(std::abs(stillPlanar - tape) < 1e-6,
           qPrintable(QStringLiteral("stillPlanar=%1 tape=%2 ellipsoid=%3")
                          .arg(stillPlanar)
                          .arg(tape)
                          .arg(QgsProject::instance()->ellipsoid())));
  QgsProject::instance()->setEllipsoid(Qgis::geoNone());
}

void TestMeasure::gridBearingIsClockwiseFromGridNorth() {
  const QgsPointXY o(200000.0, 450000.0);
  QCOMPARE(MeasureOps::gridBearingDegrees(o, QgsPointXY(200000.0, 450010.0)), 0.0);
  QCOMPARE(MeasureOps::gridBearingDegrees(o, QgsPointXY(200010.0, 450000.0)), 90.0);
  QCOMPARE(MeasureOps::gridBearingDegrees(o, QgsPointXY(200000.0, 449990.0)), 180.0);
  QCOMPARE(MeasureOps::gridBearingDegrees(o, QgsPointXY(199990.0, 450000.0)), 270.0);
  QVERIFY(std::abs(MeasureOps::gridBearingDegrees(o, QgsPointXY(200010.0, 450010.0)) - 45.0) < 1e-9);
  QVERIFY(std::isnan(MeasureOps::gridBearingDegrees(o, o)));
  QCOMPARE(MeasureOps::formatBearing(123.0 + 27.0 / 60.0 + 15.0 / 3600.0), QStringLiteral("123°27′15″"));
  QCOMPARE(MeasureOps::formatBearing(359.99999), QStringLiteral("0°00′00″"));
  QCOMPARE(MeasureOps::formatBearing(std::nan("")), QStringLiteral("—"));
}

void TestMeasure::pinPlanarEllipsoidKeepsSavedState() {
  // Labels follow the project ellipsoid; the tape is planar. Pinning NONE keeps them equal
  // without marking a freshly opened survey as unsaved.
  const QgsCoordinateReferenceSystem crs(QStringLiteral("EPSG:5186"));
  QgsProject project;
  project.setCrs(crs);
  project.setEllipsoid(QStringLiteral("EPSG:7019"));
  project.setDirty(false);
  QVERIFY(project.ellipsoid() != Qgis::geoNone());

  QVERIFY(MeasureOps::pinPlanarEllipsoid(&project));
  QCOMPARE(project.ellipsoid(), Qgis::geoNone());
  QVERIFY(!project.isDirty());
  QVERIFY(!MeasureOps::pinPlanarEllipsoid(&project));

  project.setDirty(true);
  project.setEllipsoid(QStringLiteral("EPSG:7019"));
  QVERIFY(MeasureOps::pinPlanarEllipsoid(&project));
  QVERIFY(project.isDirty());  // real unsaved work stays flagged

  const QVector<QgsPointXY> pts{QgsPointXY(200000.0, 450000.0), QgsPointXY(200010.0, 450000.0),
                                QgsPointXY(200010.0, 450020.0), QgsPointXY(200000.0, 450020.0)};
  QgsPolylineXY ring = pts;
  ring.append(pts.first());
  QgsDistanceArea labelEngine;
  labelEngine.setSourceCrs(crs, project.transformContext());
  labelEngine.setEllipsoid(project.ellipsoid());
  const double label = labelEngine.measureArea(QgsGeometry::fromPolygonXY({ring}));
  const double tape = MeasureOps::polygonAreaSquareMeters(pts, crs, project.transformContext());
  QVERIFY2(std::abs(label - tape) < 1e-6, qPrintable(QStringLiteral("label=%1 tape=%2").arg(label).arg(tape)));
}

#include "test_measure.moc"

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  const QString prefix = qEnvironmentVariable(
      "QGIS_PREFIX_PATH", QFile::exists(QStringLiteral("A:/OSGeo4W/apps/qgis-dev"))
                              ? QStringLiteral("A:/OSGeo4W/apps/qgis-dev")
                              : QStringLiteral("C:/OSGeo4W/apps/qgis-dev"));
  QgsApplication::setPrefixPath(prefix, true);
  QgsApplication::setPluginPath(prefix + QStringLiteral("/plugins"));
  QgsApplication::initQgis();
  TestMeasure tc;
  const int rc = QTest::qExec(&tc, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
