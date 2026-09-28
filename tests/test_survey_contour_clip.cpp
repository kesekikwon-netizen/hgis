#include "survey_contour_fixture.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>

class TestSurveyContourClip : public QObject {
  Q_OBJECT
private slots:
  void contoursOutsideSurveyAreaAreRemoved();
  void namedNorthEastColumnsSwap();
  void dxfSkipsZeroElevationLayers();
};

void TestSurveyContourClip::contoursOutsideSurveyAreaAreRemoved() {
  QTemporaryDir dir;
  SurveyContourJob job;
  job.outputDir = dir.path();
  job.intervalCm = 10;
  job.cellSizeM = 1;
  job.colorBands = false;
  job.read.crsAuthId = QStringLiteral("EPSG:5186");
  job.clipWkt = QStringLiteral("POLYGON((0 0, 40 0, 40 40, 0 40, 0 0))");
  const SurveyContourResult built = SurveyContourBuilder::build(surveyGrid(11, 5, 10, 10), job);
  QVERIFY2(built.ok, qPrintable(built.error));
  double maxX = -1;
  eachSurveyGeometry(dir.filePath(QStringLiteral("contours.gpkg")), "contour_lines", [&](OGRFeature* feature) {
    QVector<QPointF> vertices;
    collectSurveyVertices(feature->GetGeometryRef(), &vertices);
    for (const QPointF& point : vertices) maxX = std::max(maxX, point.x());
  });
  QVERIFY2(maxX > 0 && maxX <= 40.05, qPrintable(QString::number(maxX)));
}

void TestSurveyContourClip::namedNorthEastColumnsSwap() {
  QTemporaryDir dir;
  const QString path = dir.filePath(QStringLiteral("jeju.csv"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
  file.write("포인트명,x(N),y(E),z(h)\n"
             "1,98124.43,148083.41,57.13\n"
             "2,98128.88,148082.88,57.17\n"
             "3,98120.01,148087.28,57.37\n");
  file.close();
  SurveyReadOptions options;
  options.path = path;
  options.crsAuthId = QStringLiteral("EPSG:5186");
  const SurveyReadReport raw = SurveyPointReader::read(options);
  QVERIFY2(raw.fatal.isEmpty(), qPrintable(raw.fatal + raw.fields.join('|')));
  QVERIFY2(raw.swapSuggested, qPrintable(raw.fields.join(QLatin1Char('|')) + QStringLiteral(" x=") +
                                        QString::number(raw.xColumn) + QStringLiteral(" y=") + QString::number(raw.yColumn)));
  options.swapAxes = true;
  const SurveyReadReport swapped = SurveyPointReader::read(options);
  QCOMPARE(swapped.points.size(), 3);
  for (const SurveyPoint& point : swapped.points) {
    QVERIFY(point.x > 148000.0);
    QVERIFY(point.y > 98000.0 && point.y < 99000.0);
    QVERIFY(point.z > 57.0 && point.z < 58.0);
  }
}

void TestSurveyContourClip::dxfSkipsZeroElevationLayers() {
  QTemporaryDir dir;
  const QString path = dir.filePath(QStringLiteral("points.dxf"));
  GDALAllRegister();
  GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("DXF");
  QVERIFY(driver);
  GDALDataset* dataset = driver->Create(path.toUtf8().constData(), 0, 0, 0, GDT_Unknown, nullptr);
  QVERIFY(dataset);
  OGRLayer* layer = dataset->CreateLayer("entities", nullptr, wkbPoint25D, nullptr);
  QVERIFY(layer);
  if (layer->GetLayerDefn()->GetFieldIndex("Layer") < 0) {
    OGRFieldDefn field("Layer", OFTString);
    QVERIFY(layer->CreateField(&field) == OGRERR_NONE);
  }
  auto add = [&](const char* cad, double x, double y, double z) {
    OGRFeature* feature = OGRFeature::CreateFeature(layer->GetLayerDefn());
    OGRPoint point(x, y, z);
    feature->SetGeometry(&point);
    feature->SetField("Layer", cad);
    QVERIFY(layer->CreateFeature(feature) == OGRERR_NONE);
    OGRFeature::DestroyFeature(feature);
  };
  add("GPS_Point", 148083, 98124, 57.2);
  add("GPS_Point", 148090, 98120, 57.5);
  add("GPS_Point", 148078, 98128, 57.1);
  add("GPS_Code", 148083, 98124, 0);
  add("GPS_PointName", 148083, 98124, 0);
  GDALClose(dataset);
  SurveyReadOptions options;
  options.path = path;
  const SurveyReadReport report = SurveyPointReader::read(options);
  QVERIFY2(report.fatal.isEmpty(), qPrintable(report.fatal));
  QCOMPARE(report.points.size(), 3);
  for (const SurveyPoint& point : report.points) QVERIFY(point.z > 50.0);
  QVERIFY(report.issues.join(QLatin1Char('\n')).contains(QStringLiteral("GPS_Code")));
}

int runSurveyContourClipTests(int argc, char** argv) {
  TestSurveyContourClip test;
  return QTest::qExec(&test, argc, argv);
}

#include "test_survey_contour_clip.moc"
