#include "survey_contour_fixture.h"

#include "core/SurveyContourStyle.h"

#include <cmath>

#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

#include <qgsapplication.h>
#include <qgslayertree.h>
#include <qgsproject.h>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

QByteArray readAll(const QString& path) {
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// Two blocks of points 30 m apart on one slope; the TIN hull bridges the gap between them.
QVector<SurveyPoint> twoBlocks() {
  QVector<SurveyPoint> points;
  for (double x0 : {0.0, 40.0})
    for (int ix = 0; ix <= 4; ++ix)
      for (int iy = 0; iy <= 8; ++iy) {
        SurveyPoint point;
        point.x = x0 + ix * 2.5;
        point.y = iy * 2.5;
        point.z = 10.0 + 0.05 * point.x;
        point.row = static_cast<int>(points.size()) + 2;
        points.push_back(point);
      }
  return points;
}

int linesBetween(const QString& gpkg, int lowCm, int highCm) {
  int count = 0;
  eachSurveyGeometry(gpkg, "contour_lines", [&](OGRFeature* feature) {
    const int cm = feature->GetFieldAsInteger("elev_cm");
    if (cm > lowCm && cm < highCm) ++count;
  });
  return count;
}

}  // namespace

class TestContourSafety : public QObject {
  Q_OBJECT
private slots:
  void cleanup() { QgsProject::instance()->clear(); }
  void fitCellSizeKeepsTheGridUnderTheLimit();
  void wideDensePointsWidenTheCellInsteadOfFailing();
  void longTrianglesAcrossAGapAreEmptied();
  void breaklineNodesShapeTheSurfaceOnly();
  void clipReportsWhetherItReallyApplied();
  void installKeepsThePreviousResultWhenASwapFails();
  void aPreviousResultCanBeShownAgain();
};

void TestContourSafety::fitCellSizeKeepsTheGridUnderTheLimit() {
  QCOMPARE(SurveyContourBuilder::fitCellSize(0.5, 10, 10), 0.5);
  const double fitted = SurveyContourBuilder::fitCellSize(0.05, 250, 250);
  QVERIFY(fitted > 0.05);
  const qint64 cells = static_cast<qint64>(std::ceil(250 / fitted + 1)) * static_cast<qint64>(std::ceil(250 / fitted + 1));
  QVERIFY2(cells <= SurveyContourBuilder::kMaxCells, qPrintable(QString::number(cells)));
  QCOMPARE(std::round(fitted * 100.0) / 100.0, fitted);
}

void TestContourSafety::wideDensePointsWidenTheCellInsteadOfFailing() {
  QTemporaryDir dir;
  QVector<SurveyPoint> points = surveyGrid(3, 3, 60, 10);
  for (SurveyPoint& point : points) point.z = 10.0 + 0.02 * point.x;
  SurveyContourJob job;
  job.outputDir = dir.path();
  job.intervalCm = 50;
  job.cellSizeM = 0.05;
  job.colorBands = false;
  job.read.crsAuthId = QStringLiteral("EPSG:5186");
  const SurveyContourResult built = SurveyContourBuilder::build(points, job);
  QVERIFY2(built.ok, qPrintable(built.error));
  QVERIFY(built.cellEnlarged);
  QVERIFY(built.cellSizeM > 0.05);
  QVERIFY2(built.warning.contains(QStringLiteral("계산 격자")), qPrintable(built.warning));
}

void TestContourSafety::longTrianglesAcrossAGapAreEmptied() {
  QTemporaryDir whole, masked;
  SurveyContourJob job;
  job.intervalCm = 10;
  job.cellSizeM = 0.5;
  job.colorBands = false;
  job.read.crsAuthId = QStringLiteral("EPSG:5186");
  job.outputDir = whole.path();
  const SurveyContourResult bridged = SurveyContourBuilder::build(twoBlocks(), job);
  QVERIFY2(bridged.ok, qPrintable(bridged.error));
  // Heights 10.6-11.9 m exist only in the empty gap between the blocks.
  QVERIFY(linesBetween(whole.filePath(QStringLiteral("contours.gpkg")), 1055, 1195) > 0);
  job.outputDir = masked.path();
  job.maxEdgeM = 5.0;
  const SurveyContourResult kept = SurveyContourBuilder::build(twoBlocks(), job);
  QVERIFY2(kept.ok, qPrintable(kept.error));
  QVERIFY(kept.maskedTriangles > 0);
  QCOMPARE(linesBetween(masked.filePath(QStringLiteral("contours.gpkg")), 1055, 1195), 0);
  QVERIFY(linesBetween(masked.filePath(QStringLiteral("contours.gpkg")), 1000, 1050) > 0);
  QVERIFY2(kept.warning.contains(QStringLiteral("긴 삼각형")), qPrintable(kept.warning));
}

void TestContourSafety::breaklineNodesShapeTheSurfaceOnly() {
  QVector<SurveyPoint> corners;
  for (const auto& xy : {std::pair{0.0, 0.0}, std::pair{20.0, 0.0}, std::pair{20.0, 20.0}, std::pair{0.0, 20.0}}) {
    SurveyPoint point;
    point.x = xy.first;
    point.y = xy.second;
    point.z = 10.2;  // off every 50 cm level, so the flat case has no line at all
    point.row = static_cast<int>(corners.size()) + 2;
    corners.push_back(point);
  }
  SurveyPolyline ridge;
  for (double x : {0.0, 20.0}) {
    SurveyPoint vertex;
    vertex.x = x;
    vertex.y = 10.0;
    vertex.z = 12.2;
    vertex.row = -1;
    ridge.push_back(vertex);
  }
  QTemporaryDir flat, shaped;
  SurveyContourJob job;
  job.intervalCm = 50;
  job.cellSizeM = 0.5;
  job.colorBands = false;
  job.read.crsAuthId = QStringLiteral("EPSG:5186");
  job.outputDir = flat.path();
  const SurveyContourResult plain = SurveyContourBuilder::build(corners, job);
  QVERIFY2(plain.ok, qPrintable(plain.error));
  QCOMPARE(plain.lineCount, 0);
  job.outputDir = shaped.path();
  job.breaklines = {ridge};
  const SurveyContourResult ridged = SurveyContourBuilder::build(corners, job);
  QVERIFY2(ridged.ok, qPrintable(ridged.error));
  QVERIFY(ridged.breaklineNodes > 0);
  QVERIFY(ridged.lineCount > 0);
  QCOMPARE(ridged.pointCount, 4);
  int surveyed = 0;
  eachSurveyGeometry(shaped.filePath(QStringLiteral("contours.gpkg")), "survey_points", [&](OGRFeature*) { ++surveyed; });
  QCOMPARE(surveyed, 4);
}

void TestContourSafety::clipReportsWhetherItReallyApplied() {
  const QVector<SurveyPoint> points = surveyGrid(11, 5, 10, 10);
  auto run = [&](const QString& wkt, QTemporaryDir& dir) {
    SurveyContourJob job;
    job.outputDir = dir.path();
    job.intervalCm = 10;
    job.cellSizeM = 1;
    job.colorBands = false;
    job.read.crsAuthId = QStringLiteral("EPSG:5186");
    job.clipWkt = wkt;
    return SurveyContourBuilder::build(points, job);
  };
  QTemporaryDir a, b, c;
  const SurveyContourResult inside = run(QStringLiteral("POLYGON((0 0, 40 0, 40 40, 0 40, 0 0))"), a);
  QVERIFY(inside.ok && inside.clipped);
  QVERIFY2(inside.warning.contains(QStringLiteral("조사구역 안에만")), qPrintable(inside.warning));
  const SurveyContourResult broken = run(QStringLiteral("POLYGON((0 0, 40 0"), b);
  QVERIFY(broken.ok && !broken.clipped);
  QCOMPARE(broken.lineCount, broken.linesBeforeClip);
  QVERIFY2(!broken.warning.contains(QStringLiteral("조사구역 안에만")), qPrintable(broken.warning));
  const SurveyContourResult apart =
      run(QStringLiteral("POLYGON((1000 1000, 1010 1000, 1010 1010, 1000 1010, 1000 1000))"), c);
  QVERIFY(apart.ok && apart.clipped && apart.lineCount == 0 && apart.linesBeforeClip > 0);
  QVERIFY2(apart.warning.contains(QStringLiteral("겹치지 않아")), qPrintable(apart.warning));
}

void TestContourSafety::installKeepsThePreviousResultWhenASwapFails() {
  QTemporaryDir dir;
  const QString to = dir.path();
  const QString from = dir.filePath(QStringLiteral("_new"));
  QVERIFY(QDir().mkpath(from));
  const QString gpkg = QDir(to).filePath(QStringLiteral("contours.gpkg"));
  const QString tif = QDir(to).filePath(QStringLiteral("surface.tif"));
  QVERIFY(writeSurveyText(gpkg, "OLD-GPKG") && writeSurveyText(tif, "OLD-TIF"));
  QVERIFY(writeSurveyText(QDir(from).filePath(QStringLiteral("contours.gpkg")), "NEW-GPKG"));
  QVERIFY(writeSurveyText(QDir(from).filePath(QStringLiteral("surface.tif")), "NEW-TIF"));
  QString error;
#ifdef Q_OS_WIN
  // Another program holds surface.tif without delete sharing, so it cannot be moved aside.
  HANDLE lock = CreateFileW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(tif).utf16()), GENERIC_READ,
                            FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  QVERIFY(lock != INVALID_HANDLE_VALUE);
  const bool installed = SurveyContourBuilder::installFiles(from, to, &error);
  CloseHandle(lock);
  QVERIFY(!installed);
  QVERIFY(!error.isEmpty());
  QCOMPARE(readAll(gpkg), QByteArray("OLD-GPKG"));
  QCOMPARE(readAll(tif), QByteArray("OLD-TIF"));
  QCOMPARE(readAll(QDir(from).filePath(QStringLiteral("contours.gpkg"))), QByteArray("NEW-GPKG"));
  QVERIFY(!QFile::exists(gpkg + QStringLiteral(".old")));
#endif
  QVERIFY2(SurveyContourBuilder::installFiles(from, to, &error), qPrintable(error));
  QCOMPARE(readAll(gpkg), QByteArray("NEW-GPKG"));
  QCOMPARE(readAll(tif), QByteArray("NEW-TIF"));
  QVERIFY(!QFile::exists(gpkg + QStringLiteral(".old")) && !QFile::exists(tif + QStringLiteral(".old")));
  QVERIFY(!QFile::exists(QDir(from).filePath(QStringLiteral("contours.gpkg"))));
}

void TestContourSafety::aPreviousResultCanBeShownAgain() {
  QTemporaryDir dir;
  QVector<SurveyPoint> points = surveyGrid(11, 4, 10, 10);
  for (SurveyPoint& point : points) point.z = 10.0 + 0.03 * point.x;
  const SurveyContourResult built = buildSurvey(points, dir.path(), 50, 50, 2);
  QVERIFY2(built.ok, qPrintable(built.error));
  const QString gpkg = dir.filePath(QStringLiteral("contours.gpkg"));
  SurveyContourResult again;
  QVERIFY(SurveyContourStyle::readResult(gpkg, &again));
  QCOMPARE(again.minCm, built.minCm);
  QCOMPARE(again.maxCm, built.maxCm);
  QCOMPARE(again.bandIntervalCm, built.bandIntervalCm);
  QCOMPARE(again.lineCount, built.lineCount);
  QCOMPARE(again.bandCount, built.bandCount);
  QCOMPARE(again.pointCount, built.pointCount);
  const QString group = QStringLiteral("측량 등고선 · 복구");
  QVERIFY(SurveyContourStyle::reapply(QgsProject::instance(), gpkg, group));
  QgsLayerTreeGroup* reference = QgsProject::instance()->layerTreeRoot()->findGroup(QStringLiteral("참조 지도"));
  QVERIFY(reference && reference->findGroup(group));
  QCOMPARE(reference->findGroup(group)->findLayers().size(), 3);
  QVERIFY(!SurveyContourStyle::reapply(QgsProject::instance(), dir.filePath(QStringLiteral("none.gpkg")), group));
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
    TestContourSafety test;
    code = QTest::qExec(&test, argc, argv);
  }
  QgsApplication::exitQgis();
  return code;
}

#include "test_contour_safety.moc"
