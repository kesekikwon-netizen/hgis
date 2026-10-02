#include "survey_contour_fixture.h"

#include "core/SurveyPointArrange.h"

#include <algorithm>
#include <cmath>

#include <QTemporaryDir>
#include <QtTest>

#include <qgsapplication.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransform.h>
#include <qgscoordinatetransformcontext.h>

namespace {

QVector<SurveyPoint> byRow(QVector<SurveyPoint> points) {
  std::sort(points.begin(), points.end(), [](const SurveyPoint& a, const SurveyPoint& b) { return a.row < b.row; });
  return points;
}

QByteArray jejuCsv() {
  return "포인트명,x(N),y(E),z(h)\n"
         "1,98124.43,148083.41,57.13\n"
         "2,98128.88,148082.88,57.17\n"
         "3,98120.01,148087.28,57.37\n";
}

}  // namespace

class TestSurveyPointArrange : public QObject {
  Q_OBJECT
private slots:
  void arrangeMatchesADirectRead();
  void fileCrsIsTransformedToTheWorkCrs();
  void notesNameSuspiciousDuplicateAndFarPoints();
  void outsidePointsSuggestTheOtherAxisOrder();
  void issuesAreCapped();
  void dxfLinesBecomeBreaklines();
};

void TestSurveyPointArrange::arrangeMatchesADirectRead() {
  QTemporaryDir dir;
  const QString path = dir.filePath(QStringLiteral("jeju.csv"));
  QVERIFY(writeSurveyText(path, jejuCsv()));
  SurveyReadOptions options;
  options.path = path;
  options.crsAuthId = QStringLiteral("EPSG:5186");
  const SurveyReadReport fileOrder = SurveyPointReader::read(options);
  QVERIFY2(fileOrder.fatal.isEmpty(), qPrintable(fileOrder.fatal));
  QVERIFY(fileOrder.swapSuggested);
  QVERIFY(!fileOrder.swapReason.isEmpty());
  QVERIFY(!fileOrder.swapApplied);
  options.swapAxes = true;
  const QVector<SurveyPoint> direct = byRow(SurveyPointReader::read(options).points);
  const SurveyReadReport arranged =
      SurveyPointArrange::arrange(fileOrder, {true, QString(), QStringLiteral("EPSG:5186")});
  QVERIFY(arranged.swapApplied);
  QCOMPARE(arranged.outsideCount, 0);
  const QVector<SurveyPoint> moved = byRow(arranged.points);
  QCOMPARE(moved.size(), direct.size());
  for (int i = 0; i < moved.size(); ++i) {
    QCOMPARE(moved[i].x, direct[i].x);
    QCOMPARE(moved[i].y, direct[i].y);
    QCOMPARE(moved[i].z, direct[i].z);
  }
}

void TestSurveyPointArrange::fileCrsIsTransformedToTheWorkCrs() {
  const QgsCoordinateReferenceSystem wgs(QStringLiteral("EPSG:4326"));
  const QgsCoordinateReferenceSystem east(QStringLiteral("EPSG:5187"));
  const QgsCoordinateReferenceSystem central(QStringLiteral("EPSG:5186"));
  const QgsCoordinateTransform toEast(wgs, east, QgsCoordinateTransformContext());
  QByteArray csv = "점번호,X,Y,표고\n";
  QVector<QgsPointXY> filePoints;
  for (int i = 0; i < 3; ++i) {
    const QgsPointXY p = toEast.transform(QgsPointXY(129.0 + 0.001 * i, 35.5 + 0.0007 * (i % 2)));
    filePoints << p;
    csv += QStringLiteral("P%1,%2,%3,12.5\n").arg(i).arg(p.y(), 0, 'f', 3).arg(p.x(), 0, 'f', 3).toUtf8();
  }
  QTemporaryDir dir;
  const QString path = dir.filePath(QStringLiteral("east.csv"));
  QVERIFY(writeSurveyText(path, csv));
  SurveyReadOptions options;
  options.path = path;
  options.crsAuthId = QStringLiteral("EPSG:5186");
  options.sourceCrsAuthId = QStringLiteral("EPSG:5187");
  options.swapAxes = true;
  const SurveyReadReport report = SurveyPointReader::read(options);
  QVERIFY2(report.fatal.isEmpty(), qPrintable(report.fatal));
  QCOMPARE(report.sourceCrsAuthId, QStringLiteral("EPSG:5187"));
  QCOMPARE(report.points.size(), 3);
  const QgsCoordinateTransform toCentral(east, central, QgsCoordinateTransformContext());
  const QVector<SurveyPoint> points = byRow(report.points);
  for (int i = 0; i < 3; ++i) {
    const QgsPointXY expected = toCentral.transform(filePoints[i]);
    QVERIFY2(std::hypot(points[i].x - expected.x(), points[i].y - expected.y()) < 0.01,
             qPrintable(QStringLiteral("%1 %2 vs %3 %4").arg(points[i].x).arg(points[i].y).arg(expected.x()).arg(expected.y())));
    QCOMPARE(points[i].z, 12.5);
  }
  // The two origins are about 180 km apart, so an unconverted read would sit far away.
  QVERIFY(std::abs(points[0].x - filePoints[0].x()) > 100000.0);
}

void TestSurveyPointArrange::notesNameSuspiciousDuplicateAndFarPoints() {
  QTemporaryDir dir;
  const QString path = dir.filePath(QStringLiteral("odd.csv"));
  QVERIFY(writeSurveyText(path, "점번호,X,Y,표고\n"
                                "P1,550000,200000,0\n"
                                "P2,550010,200000,999.5\n"
                                "P3,550000,200010,12\n"
                                "P4,550000,200010,13\n"
                                "P5,550020,200020,14\n"));
  SurveyReadOptions options;
  options.path = path;
  options.swapAxes = true;
  const SurveyReadReport report = SurveyPointReader::read(options);
  QVERIFY2(report.fatal.isEmpty(), qPrintable(report.fatal));
  QCOMPARE(report.suspiciousCount, 2);
  QCOMPARE(report.duplicateGroups, 1);
  const QgsRectangle farAway(400000, 550000, 400100, 550100);
  QVERIFY(SurveyPointArrange::gapToExtent(report.points, farAway) > 150000.0);
  QCOMPARE(SurveyPointArrange::gapToExtent(report.points, QgsRectangle(199990, 549990, 200030, 550030)), 0.0);
  const QString notes = SurveyPointArrange::notes(report, farAway, false).join(QLatin1Char('\n'));
  QVERIFY2(notes.contains(QStringLiteral("의심점 2개")), qPrintable(notes));
  QVERIFY2(notes.contains(QStringLiteral("같은 좌표 1곳")), qPrintable(notes));
  QVERIFY2(notes.contains(QStringLiteral("떨어져")), qPrintable(notes));
  QVERIFY2(notes.contains(QStringLiteral("X를 북쪽")), qPrintable(notes));
}

void TestSurveyPointArrange::outsidePointsSuggestTheOtherAxisOrder() {
  SurveyReadReport fileOrder;
  for (int i = 0; i < 3; ++i) {
    SurveyPoint point;
    point.x = 550000 + i;  // northing written first
    point.y = 200000 + 5 * i;
    point.z = 10 + i;
    point.row = i + 2;
    fileOrder.points << point;
  }
  fileOrder.points[0].x = 5000000;  // far outside either way
  const SurveyReadReport arranged =
      SurveyPointArrange::arrange(fileOrder, {false, QString(), QStringLiteral("EPSG:5186")});
  QVERIFY(arranged.outsideKorea);
  QVERIFY(arranged.outsideCount >= 1);
  const QString notes = SurveyPointArrange::notes(arranged, QgsRectangle(), true).join(QLatin1Char('\n'));
  QVERIFY2(notes.contains(QStringLiteral("한국 범위 밖")) && notes.contains(QStringLiteral("바꾸면")), qPrintable(notes));
}

void TestSurveyPointArrange::issuesAreCapped() {
  QByteArray csv = "점번호,X,Y,표고\n";
  for (int i = 0; i < 60; ++i) csv += QByteArray("P") + QByteArray::number(i) + ",abc,def,1\n";
  QTemporaryDir dir;
  const QString path = dir.filePath(QStringLiteral("bad.csv"));
  QVERIFY(writeSurveyText(path, csv));
  SurveyReadOptions options;
  options.path = path;
  const SurveyReadReport report = SurveyPointReader::read(options);
  QCOMPARE(report.skipped, 60);
  QCOMPARE(report.issues.size(), SurveyPointReader::kMaxIssues);
  QCOMPARE(report.issuesOmitted, 60 - SurveyPointReader::kMaxIssues);
  QVERIFY(SurveyPointArrange::notes(report, QgsRectangle(), false).join(QLatin1Char('\n')).contains(QStringLiteral("줄였습니다")));
}

void TestSurveyPointArrange::dxfLinesBecomeBreaklines() {
  QTemporaryDir dir;
  const QString path = dir.filePath(QStringLiteral("lines.dxf"));
  const QString written = writeSurveyDxfWithLines(path);
  QVERIFY2(written.isEmpty(), qPrintable(written));
  SurveyReadOptions options;
  options.path = path;
  const SurveyReadReport report = SurveyPointReader::read(options);
  QVERIFY2(report.fatal.isEmpty(), qPrintable(report.fatal));
  QCOMPARE(report.points.size(), 3);
  QCOMPARE(report.breaklines.size(), 1);
  QCOMPARE(report.flatLineCount, 1);
  for (const SurveyPoint& vertex : report.breaklines.first()) QVERIFY(vertex.z >= 12.0 && vertex.row == -1);
  // A line-only CAD layer is not a "point layer without heights".
  QVERIFY2(!report.issues.join(QLatin1Char('\n')).contains(QStringLiteral("BRK")), qPrintable(report.issues.join('|')));
}

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  const QString prefix = qEnvironmentVariable(
      "QGIS_PREFIX_PATH", QFile::exists(QStringLiteral("A:/OSGeo4W/apps/qgis-dev"))
                              ? QStringLiteral("A:/OSGeo4W/apps/qgis-dev")
                              : QStringLiteral("C:/OSGeo4W/apps/qgis-dev"));
  QgsApplication::setPrefixPath(prefix, true);
  QgsApplication::initQgis();
  int code = 0;
  {
    TestSurveyPointArrange test;
    code = QTest::qExec(&test, argc, argv);
  }
  QgsApplication::exitQgis();
  return code;
}

#include "test_survey_point_arrange.moc"
