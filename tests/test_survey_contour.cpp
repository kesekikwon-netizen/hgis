#include "survey_contour_fixture.h"

#include "core/LayerOps.h"
#include "core/SurveyContourMath.h"
#include "core/SurveyContourStyle.h"

#include <algorithm>
#include <cmath>

#include <QDir>
#include <QElapsedTimer>
#include <QSet>
#include <QTemporaryDir>
#include <QtTest>

#include <qgsapplication.h>
#include <qgsgraduatedsymbolrenderer.h>
#include <qgslayertree.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

class TestSurveyContour : public QObject {
  Q_OBJECT
private slots:
  void cleanup() { QgsProject::instance()->clear(); }
  void xlsxSuggestsAxisSwap();
  void dxfLayerFiltersPointZ();
  void planeContourSitsOnTheSlope();
  void northSlopeKeepsHighContoursNorth();
  void tenthMetreLabelsHaveNoBinaryResidue();
  void contoursStayInsideThePointHull();
  void colorBandsMatchTheInterval();
  void referenceGroupIsNotASubmitLayer();
  void sourceFilesAreNotModified();
  void badRowsAreReportedAndTwoPointsAreRefused();
  void twentyThousandPointsFinishWithinBudget();
};

void TestSurveyContour::xlsxSuggestsAxisSwap() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString path = dir.filePath(QStringLiteral("survey.xlsx"));
  const QString written = writeSurveyTable(path, {{QStringLiteral("점번호"), QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("표고")},
                    {QStringLiteral("P1"), QStringLiteral("550000"), QStringLiteral("200000"), QStringLiteral("12.0")},
                    {QStringLiteral("P2"), QStringLiteral("550010"), QStringLiteral("200000"), QStringLiteral("12.2")},
                    {QStringLiteral("P3"), QStringLiteral("550000"), QStringLiteral("200020"), QStringLiteral("12.4")},
                    {QStringLiteral("P4"), QStringLiteral("550030"), QStringLiteral("200040"), QStringLiteral("12.8")}});
  QVERIFY2(written.isEmpty(), qPrintable(written));
  SurveyReadOptions options;
  options.path = path;
  options.crsAuthId = QStringLiteral("EPSG:5186");
  const SurveyReadReport raw = SurveyPointReader::read(options);
  QVERIFY2(raw.fatal.isEmpty(), qPrintable(raw.fatal + QStringLiteral(" [") + raw.fields.join(QLatin1Char('|')) + QLatin1Char(']')));
  QCOMPARE(raw.points.size(), 4);
  QVERIFY(raw.swapSuggested);
  options.swapAxes = true;
  const SurveyReadReport swapped = SurveyPointReader::read(options);
  QVERIFY(!swapped.outsideKorea);
  const QgsRectangle korea = LayerOps::koreaExtentForCrs(QStringLiteral("EPSG:5186"));
  for (const SurveyPoint& point : swapped.points) QVERIFY(korea.contains(QgsPointXY(point.x, point.y)));
}

void TestSurveyContour::dxfLayerFiltersPointZ() {
  QTemporaryDir dir;
  const QString path = dir.filePath(QStringLiteral("points.dxf"));
  const QString written = writeSurveyDxf(path);
  QVERIFY2(written.isEmpty(), qPrintable(written));
  const SurveySourceInfo info = SurveyPointReader::inspect(path);
  SurveyReadOptions options;
  options.path = path;
  options.layer = QStringLiteral("A");
  const SurveyReadReport report = SurveyPointReader::read(options);
  QVERIFY2(report.points.size() == 10,
           qPrintable(report.fatal + info.layers.join(QLatin1Char(',')) + QStringLiteral(" n=") +
                      QString::number(report.points.size())));
  QSet<int> elevations;
  for (const SurveyPoint& point : report.points) elevations.insert(surveyMetersToCm(point.z));
  QVERIFY(elevations.contains(1000));
  QVERIFY(elevations.contains(1090));
  options.layer = QStringLiteral("B");
  QCOMPARE(SurveyPointReader::read(options).points.size(), 10);
}

void TestSurveyContour::planeContourSitsOnTheSlope() {
  QTemporaryDir dir;
  const SurveyContourResult built = buildSurvey(surveyGrid(11, 5, 10, 10), dir.path(), 10, 0, 1);
  QVERIFY2(built.ok, qPrintable(built.error));
  QVector<QPointF> at1050, at1020, at1080;
  eachSurveyGeometry(dir.filePath(QStringLiteral("contours.gpkg")), "contour_lines", [&](OGRFeature* feature) {
    QVector<QPointF> vertices;
    collectSurveyVertices(feature->GetGeometryRef(), &vertices);
    const int cm = feature->GetFieldAsInteger("elev_cm");
    if (cm == 1050) at1050 += vertices;
    if (cm == 1020) at1020 += vertices;
    if (cm == 1080) at1080 += vertices;
  });
  QVERIFY(!at1050.isEmpty());
  double worst = 0;
  for (const QPointF& point : at1050) worst = std::max(worst, std::abs(point.x() - 50.0));
  QVERIFY2(worst <= 1.01, qPrintable(QString::number(worst)));
  double meanLow = 0, meanHigh = 0;
  for (const QPointF& point : at1020) meanLow += point.x();
  for (const QPointF& point : at1080) meanHigh += point.x();
  QVERIFY(!at1020.isEmpty() && !at1080.isEmpty());
  QVERIFY(meanLow / at1020.size() < meanHigh / at1080.size());
}

void TestSurveyContour::northSlopeKeepsHighContoursNorth() {
  QTemporaryDir dir;
  QVector<SurveyPoint> points = surveyGrid(5, 11, 10, 10);
  for (SurveyPoint& point : points) point.z = 10.0 + 0.01 * point.y;
  const SurveyContourResult built = buildSurvey(points, dir.path(), 10, 0, 1);
  QVERIFY2(built.ok, qPrintable(built.error));
  QVector<QPointF> low, high;
  eachSurveyGeometry(dir.filePath(QStringLiteral("contours.gpkg")), "contour_lines", [&](OGRFeature* feature) {
    QVector<QPointF> vertices;
    collectSurveyVertices(feature->GetGeometryRef(), &vertices);
    const int cm = feature->GetFieldAsInteger("elev_cm");
    if (cm == 1020) low += vertices;
    if (cm == 1080) high += vertices;
  });
  QVERIFY(!low.isEmpty() && !high.isEmpty());
  double meanLow = 0, meanHigh = 0;
  for (const QPointF& point : low) meanLow += point.y();
  for (const QPointF& point : high) meanHigh += point.y();
  QVERIFY2(meanLow / low.size() < meanHigh / high.size(),
           qPrintable(QString::number(meanLow / low.size()) + QLatin1Char(' ') +
                      QString::number(meanHigh / high.size())));
}

void TestSurveyContour::tenthMetreLabelsHaveNoBinaryResidue() {
  QTemporaryDir dir;
  QVector<SurveyPoint> points = surveyGrid(7, 5, 5, 10);
  for (SurveyPoint& point : points) point.z = 10.0 + 0.1 * point.x;
  const SurveyContourResult built = buildSurvey(points, dir.path(), 10, 0, 1);
  QVERIFY2(built.ok, qPrintable(built.error));
  int seen = 0;
  eachSurveyGeometry(dir.filePath(QStringLiteral("contours.gpkg")), "contour_lines", [&](OGRFeature* feature) {
    const int cm = feature->GetFieldAsInteger("elev_cm");
    const QString label = QString::fromUtf8(feature->GetFieldAsString("elev_label"));
    if (cm % 10 != 0 || label.contains(QStringLiteral("9999")) || label.contains(QStringLiteral("0001")))
      seen = -100000 - cm;
    else ++seen;
  });
  QVERIFY2(seen > 0, qPrintable(QString::number(seen)));
}

void TestSurveyContour::contoursStayInsideThePointHull() {
  QVector<SurveyPoint> points;
  SurveyPoint center;
  center.z = 15;
  points.push_back(center);
  for (int i = 0; i < 16; ++i) {
    const double angle = i * 6.283185307179586 / 16.0;
    SurveyPoint point;
    point.x = 30.0 * std::cos(angle);
    point.y = 30.0 * std::sin(angle);
    point.z = 10;
    points.push_back(point);
  }
  QTemporaryDir dir;
  const SurveyContourResult built = buildSurvey(points, dir.path(), 50, 50, 1);
  QVERIFY2(built.ok, qPrintable(built.error));
  double farthest = 0;
  auto measure = [&](OGRFeature* feature) {
    QVector<QPointF> vertices;
    collectSurveyVertices(feature->GetGeometryRef(), &vertices);
    for (const QPointF& point : vertices) farthest = std::max(farthest, std::hypot(point.x(), point.y()));
  };
  eachSurveyGeometry(dir.filePath(QStringLiteral("contours.gpkg")), "contour_lines", measure);
  eachSurveyGeometry(dir.filePath(QStringLiteral("contours.gpkg")), "contour_bands", measure);
  QVERIFY2(farthest <= 31.5, qPrintable(QString::number(farthest)));
}

void TestSurveyContour::colorBandsMatchTheInterval() {
  QTemporaryDir dir;
  QVector<SurveyPoint> points = surveyGrid(11, 4, 10, 10);
  for (SurveyPoint& point : points) point.z = 10.0 + 0.03 * point.x;
  const SurveyContourResult built = buildSurvey(points, dir.path(), 50, 50, 2);
  QVERIFY2(built.ok, qPrintable(built.error));
  QString error;
  QVERIFY2(SurveyContourStyle::apply(QgsProject::instance(), dir.filePath(QStringLiteral("contours.gpkg")),
                                     QStringLiteral("측량 등고선 · sample"), built, &error),
           qPrintable(error));
  QgsVectorLayer* bands = nullptr;
  for (QgsMapLayer* layer : QgsProject::instance()->mapLayers()) {
    if (layer && layer->name() == QStringLiteral("색 구간")) bands = qobject_cast<QgsVectorLayer*>(layer);
  }
  QVERIFY(bands);
  auto* renderer = dynamic_cast<QgsGraduatedSymbolRenderer*>(bands->renderer());
  QVERIFY(renderer);
  const int classes = surveyClassCount(built.minCm, built.maxCm, 50);
  QCOMPARE(renderer->ranges().size(), classes);
  QCOMPARE(classes, (built.maxCm - built.minCm + 49) / 50);
  QCOMPARE(renderer->ranges().at(0).label(), surveyBandLabel(built.minCm, built.minCm + 50));
  QVERIFY(renderer->ranges().at(0).label().contains(QStringLiteral(" – ")));
  QVERIFY(renderer->ranges().at(0).label().endsWith(QStringLiteral(" m")));
}

void TestSurveyContour::referenceGroupIsNotASubmitLayer() {
  QTemporaryDir dir;
  const SurveyContourResult built = buildSurvey(surveyGrid(6, 4, 10, 12), dir.path(), 50, 50, 2);
  QVERIFY2(built.ok, qPrintable(built.error));
  const QString group = QStringLiteral("측량 등고선 · 현장");
  QString error;
  QVERIFY(SurveyContourStyle::apply(QgsProject::instance(), dir.filePath(QStringLiteral("contours.gpkg")), group,
                                    built, &error));
  QgsLayerTreeGroup* reference = QgsProject::instance()->layerTreeRoot()->findGroup(QStringLiteral("참조 지도"));
  QVERIFY(reference);
  QgsLayerTreeGroup* sub = reference->findGroup(group);
  QVERIFY(sub);
  QStringList names;
  for (QgsLayerTreeLayer* node : sub->findLayers()) names << node->name();
  QCOMPARE(names, QStringList({QStringLiteral("측량점"), QStringLiteral("등고선"), QStringLiteral("색 구간")}));
  const QStringList domain{QStringLiteral("survey_area"), QStringLiteral("feature_poly"),
                           QStringLiteral("feature_line"), QStringLiteral("section_line"),
                           QStringLiteral("control_points"), QStringLiteral("artifact_point")};
  for (QgsMapLayer* layer : QgsProject::instance()->mapLayers()) {
    QVERIFY(LayerOps::isReferenceLayer(layer));
    QVERIFY(LayerOps::layerKeyOf(layer).isEmpty());
    for (const QString& key : domain) {
      for (QgsVectorLayer* owned : LayerOps::domainLayersForKey(QgsProject::instance(), key))
        QVERIFY(owned->id() != layer->id());
    }
  }
  auto* points = sub->findLayers().first();
  QVERIFY(points && !points->itemVisibilityChecked());
}

void TestSurveyContour::sourceFilesAreNotModified() {
  QTemporaryDir dir;
  const QString xlsx = dir.filePath(QStringLiteral("survey.xlsx"));
  const QString dxf = dir.filePath(QStringLiteral("points.dxf"));
  const QString tableError = writeSurveyTable(xlsx, {{QStringLiteral("점번호"), QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("표고")},
                    {QStringLiteral("1"), QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("10")},
                    {QStringLiteral("2"), QStringLiteral("20"), QStringLiteral("0"), QStringLiteral("11")},
                    {QStringLiteral("3"), QStringLiteral("0"), QStringLiteral("20"), QStringLiteral("12")},
                    {QStringLiteral("4"), QStringLiteral("20"), QStringLiteral("20"), QStringLiteral("13")}});
  QVERIFY2(tableError.isEmpty(), qPrintable(tableError));
  const QString dxfError = writeSurveyDxf(dxf);
  QVERIFY2(dxfError.isEmpty(), qPrintable(dxfError));
  const QByteArray beforeX = surveySha256(xlsx);
  const QByteArray beforeD = surveySha256(dxf);
  SurveyReadOptions options;
  options.path = xlsx;
  const SurveyReadReport report = SurveyPointReader::read(options);
  QVERIFY(report.points.size() >= 3);
  const SurveyContourResult built = buildSurvey(report.points, dir.filePath(QStringLiteral("out")), 50, 50, 2);
  QVERIFY2(built.ok, qPrintable(built.error));
  options.path = dxf;
  QVERIFY(SurveyPointReader::read(options).points.size() == 20);
  QCOMPARE(surveySha256(xlsx), beforeX);
  QCOMPARE(surveySha256(dxf), beforeD);
}

void TestSurveyContour::badRowsAreReportedAndTwoPointsAreRefused() {
  QTemporaryDir dir;
  const QString path = dir.filePath(QStringLiteral("messy.xlsx"));
  const QString tableError = writeSurveyTable(path, {{QStringLiteral("점번호"), QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("표고")},
                    {QStringLiteral(" "), QString(), QString(), QString()},
                    {QStringLiteral("A"), QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("abc")},
                    {QStringLiteral("B"), QStringLiteral("10"), QStringLiteral("20"), QStringLiteral("5")},
                    {QStringLiteral("C"), QStringLiteral("10"), QStringLiteral("20"), QStringLiteral("7")},
                    {QStringLiteral("D"), QStringLiteral("12"), QStringLiteral("20"), QStringLiteral("6")}});
  QVERIFY2(tableError.isEmpty(), qPrintable(tableError));
  SurveyReadOptions options;
  options.path = path;
  const SurveyReadReport report = SurveyPointReader::read(options);
  QVERIFY2(report.fatal.isEmpty(), qPrintable(report.issues.join('\n') + report.fatal));
  QVERIFY2(report.issues.join(QLatin1Char('\n')).contains(QStringLiteral("빈 행")),
           qPrintable(report.issues.join(QStringLiteral(" | "))));
  QVERIFY2(report.issues.join(QLatin1Char('\n')).contains(QStringLiteral("표고가 숫자가 아닙니다")),
           qPrintable(report.issues.join(QStringLiteral(" | "))));
  QCOMPARE(report.duplicateGroups, 1);
  QCOMPARE(report.points.size(), 2);
  SurveyPoint a, b;
  a.x = 0;
  a.y = 0;
  a.z = 1;
  b.x = 5;
  b.y = 0;
  b.z = 2;
  const SurveyContourResult refused = buildSurvey({a, b}, dir.filePath(QStringLiteral("no")), 10, 0, 1);
  QVERIFY(!refused.ok);
  QVERIFY(refused.error.contains(QStringLiteral("3개")));
  SurveyPoint c = b;
  c.x = 10;
  const SurveyContourResult line = buildSurvey({a, b, c}, dir.filePath(QStringLiteral("line")), 10, 0, 1);
  QVERIFY(!line.ok);
  QVERIFY(line.error.contains(QStringLiteral("일직선")));
}

void TestSurveyContour::twentyThousandPointsFinishWithinBudget() {
  QTemporaryDir dir;
  QElapsedTimer timer;
  timer.start();
  const SurveyContourResult built = buildSurvey(surveyGrid(100, 200, 0.25, 10), dir.path(), 50, 0, 0.25);
  const qint64 elapsed = timer.elapsed();
  qWarning("survey contour 20000 points %lld ms", static_cast<long long>(elapsed));
  QVERIFY2(built.ok, qPrintable(built.error));
  // 2026-09-29 field PC, quiet machine: 76.5 s here and the same wall time for the b7e2916
  // binary, so 56 s was a faster reference PC. Budget = measured x 1.3 (TEST_INFRA.md).
  QVERIFY2(elapsed < 100000, qPrintable(QString::number(elapsed)));
}

int runSurveyContourClipTests(int argc, char** argv);

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  const QString prefix = qEnvironmentVariable(
      "QGIS_PREFIX_PATH", QFile::exists(QStringLiteral("A:/OSGeo4W/apps/qgis-dev"))
                              ? QStringLiteral("A:/OSGeo4W/apps/qgis-dev")
                              : QStringLiteral("C:/OSGeo4W/apps/qgis-dev"));
  QgsApplication::setPrefixPath(prefix, true);
  QgsApplication::setPluginPath(prefix + QStringLiteral("/plugins"));
  QgsApplication::initQgis();
  TestSurveyContour test;
  // The ctest wrapper passes "-o <log>,txt". A second qExec with the same file truncates the
  // first suite's report (and stdout of these exes is empty, TEST_INFRA.md), so the clip suite
  // writes "<log>.clip.txt"; run_qtest.cmake prints "<log>.*.txt" next to the main log.
  QVector<QByteArray> clipStorage;
  QVector<char*> clipArgs;
  for (int i = 0; i < argc; ++i) {
    clipStorage.append(QByteArray(argv[i]));
    if (qstrcmp(argv[i], "-o") == 0 && i + 1 < argc) {
      const QByteArray spec(argv[++i]);
      const int comma = spec.lastIndexOf(',');
      clipStorage.append((comma < 0 ? spec : spec.left(comma)) + ".clip.txt,txt");
    }
  }
  for (QByteArray& arg : clipStorage) clipArgs.append(arg.data());
  int clipArgc = clipArgs.size();
  const int code = QTest::qExec(&test, argc, argv) | runSurveyContourClipTests(clipArgc, clipArgs.data());
  QgsApplication::exitQgis();
  return code;
}

#include "test_survey_contour.moc"
