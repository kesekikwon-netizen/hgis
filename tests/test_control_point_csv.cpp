// F058/F054: control-point CSV reading. Korean headers and 표고(Z), quoted cells, skipped
// rows reported in the preview, and a re-imported point is not added twice. The axis swap
// stays a suggestion (the caller passes swapAxes).
#include <QtTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "core/ControlPointCsv.h"
#include "core/LayerOps.h"
#include "core/SurveyProjectFactory.h"

#include <qgsapplication.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsgeometry.h>
#include <qgsproject.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorlayer.h>

namespace {
bool writeCsv(const QString& path, const QString& text) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) return false;
  return file.write(QByteArray("\xEF\xBB\xBF") + text.toUtf8()) > 0;
}

QgsFeature pointNamed(QgsVectorLayer* layer, const QString& id) {
  QgsFeatureIterator it = layer->getFeatures();
  QgsFeature f;
  while (it.nextFeature(f))
    if (f.attribute(QStringLiteral("point_id")).toString() == id) return f;
  return QgsFeature();
}
}  // namespace

class TestControlPointCsv : public QObject {
  Q_OBJECT
private slots:
  void koreanHeaders_mapToColumns();
  void namedPointColumn_winsOverRowNumber();
  void quotedCellsAndDelimiters();
  void headerless_numericFourthColumnIsZ();
  void shortRowsAndMissingXY();
  void import_readsZAndSkipsReimportedPoints();
  void swapSuggestion_findsSurveyAreaTheLayerCountMissed();
};

void TestControlPointCsv::koreanHeaders_mapToColumns() {
  QCOMPARE(ControlPointCsv::normalizeHeader(QStringLiteral("측점")), QStringLiteral("point_id"));
  QCOMPARE(ControlPointCsv::normalizeHeader(QStringLiteral("X(m)")), QStringLiteral("x"));
  QCOMPARE(ControlPointCsv::normalizeHeader(QStringLiteral("표고 (m)")), QStringLiteral("z"));
  QCOMPARE(ControlPointCsv::normalizeHeader(QStringLiteral("\"Y좌표\"")), QStringLiteral("y"));
  QCOMPARE(ControlPointCsv::normalizeHeader(QStringLiteral("N")), QStringLiteral("y"));
  QCOMPARE(ControlPointCsv::normalizeHeader(QStringLiteral("경도")), QStringLiteral("x"));
  ControlPointCsv::Table table;
  QString error;
  QVERIFY2(ControlPointCsv::parseText(QStringLiteral("측점,X(m),Y(m),표고\nBM1,200000.1,450000.2,35.4\n"), &table,
                                      &error),
           qPrintable(error));
  QVERIFY(table.headerFound);
  QCOMPARE(table.idColumn, 0);
  QCOMPARE(table.xColumn, 1);
  QCOMPARE(table.yColumn, 2);
  QCOMPARE(table.zColumn, 3);
  QCOMPARE(ControlPointCsv::cell(table.rows.first(), table.zColumn), QStringLiteral("35.4"));
}

void TestControlPointCsv::namedPointColumn_winsOverRowNumber() {
  ControlPointCsv::Table table;
  QVERIFY(ControlPointCsv::parseText(QStringLiteral("No,측점,X,Y\n1,BM1,200000,450000\n"), &table));
  QCOMPARE(table.idColumn, 1);
  QCOMPARE(ControlPointCsv::cell(table.rows.first(), table.idColumn), QStringLiteral("BM1"));
}

void TestControlPointCsv::quotedCellsAndDelimiters() {
  QCOMPARE(ControlPointCsv::detectDelimiter(QStringLiteral("a;\"b;c\";d")), QLatin1Char(';'));
  QCOMPARE(ControlPointCsv::detectDelimiter(QStringLiteral("BM1\t200000\t450000")), QLatin1Char('\t'));
  QCOMPARE(ControlPointCsv::detectDelimiter(QStringLiteral("BM1 200000  450000")), QLatin1Char(' '));
  QCOMPARE(ControlPointCsv::splitLine(QStringLiteral("\"BM,1\",200000,\"say \"\"hi\"\"\""), QLatin1Char(',')),
           QStringList({QStringLiteral("BM,1"), QStringLiteral("200000"), QStringLiteral("say \"hi\"")}));
  QCOMPARE(ControlPointCsv::splitLine(QStringLiteral("BM1 200000  450000"), QLatin1Char(' ')),
           QStringList({QStringLiteral("BM1"), QStringLiteral("200000"), QStringLiteral("450000")}));
  ControlPointCsv::Table table;
  QVERIFY(ControlPointCsv::parseText(QStringLiteral("점명;X;Y\n\"기준 1\";200000;450000\n"), &table));
  QCOMPARE(ControlPointCsv::cell(table.rows.first(), table.idColumn), QStringLiteral("기준 1"));
}

void TestControlPointCsv::headerless_numericFourthColumnIsZ() {
  ControlPointCsv::Table withZ;
  QVERIFY(ControlPointCsv::parseText(QStringLiteral("BM1,200000,450000,35.2\nBM2,200010,450010,36.0\n"), &withZ));
  QVERIFY(!withZ.headerFound);
  QCOMPARE(withZ.zColumn, 3);
  ControlPointCsv::Table withDatum;
  QVERIFY(ControlPointCsv::parseText(QStringLiteral("BM1,200000,450000,세계측지계,GRS80\n"), &withDatum));
  QCOMPARE(withDatum.zColumn, -1);
  QCOMPARE(withDatum.headers.at(3), QStringLiteral("datum"));
}

void TestControlPointCsv::shortRowsAndMissingXY() {
  ControlPointCsv::Table table;
  QVERIFY(ControlPointCsv::parseText(QStringLiteral("point_id,x,y\nBM1,1,2\njunk\n# comment\n"), &table));
  QCOMPARE(table.rows.size(), 1);
  QCOMPARE(table.shortRows, 1);
  ControlPointCsv::Table noXY;
  QString error;
  QVERIFY(!ControlPointCsv::parseText(QStringLiteral("point_id,z\nP1,3\n"), &noXY, &error));
  QVERIFY(error.contains(QStringLiteral("X·Y")));
}

void TestControlPointCsv::import_readsZAndSkipsReimportedPoints() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QString error;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir.path(), QStringLiteral("기준점"), &error,
                                                             QStringLiteral("EPSG:5186"));
  QVERIFY2(!gpkg.isEmpty(), qPrintable(error));
  QgsVectorLayer cp(gpkg + QStringLiteral("|layername=control_points"), QStringLiteral("control_points"),
                    QStringLiteral("ogr"));
  QVERIFY(cp.isValid());
  const QString csv = QDir(dir.path()).filePath(QStringLiteral("성과표.csv"));
  QVERIFY(writeCsv(csv, QStringLiteral("측점,X,Y,표고\nBM1,200000,450000,35.4\nBM2,200010,450010,36.1\n"
                                       "잘못,abc,def,1\n")));

  const LayerOps::ControlCsvPreview preview = LayerOps::previewControlPointsCsv(&cp, csv, nullptr, &error);
  QVERIFY2(preview.ok, qPrintable(error));
  QCOMPARE(preview.count, 2);
  QVERIFY2(preview.summary.contains(QStringLiteral("1행은 건너뜁니다")), qPrintable(preview.summary));
  QVERIFY2(preview.summary.contains(QStringLiteral("표고(Z)")), qPrintable(preview.summary));
  QVERIFY(!preview.swapSuggested);  // suggestion only; nothing is swapped here

  QCOMPARE(LayerOps::importControlPointsCsv(&cp, csv, &error, false), 2);
  const QgsFeature bm1 = pointNamed(&cp, QStringLiteral("BM1"));
  QVERIFY(bm1.isValid());
  QCOMPARE(bm1.attribute(QStringLiteral("z")).toDouble(), 35.4);
  QVERIFY(!bm1.attribute(QStringLiteral("uid")).toString().isEmpty());
  QVERIFY(!bm1.attribute(QStringLiteral("created_at")).toString().isEmpty());

  // The same file again: nothing new, and the user is told why.
  const LayerOps::ControlCsvPreview again = LayerOps::previewControlPointsCsv(&cp, csv, nullptr, &error);
  QVERIFY2(again.summary.contains(QStringLiteral("이름·위치가 같은 2행")), qPrintable(again.summary));
  error.clear();
  QCOMPARE(LayerOps::importControlPointsCsv(&cp, csv, &error, false), -1);
  QVERIFY2(error.contains(QStringLiteral("이미 가져온 점")), qPrintable(error));
  QCOMPARE(cp.featureCount(), 2LL);

  // Same name at another place is imported and pointed out, never dropped silently.
  const QString moved = QDir(dir.path()).filePath(QStringLiteral("moved.csv"));
  QVERIFY(writeCsv(moved, QStringLiteral("측점,X,Y\nBM1,200500,450500\n")));
  const LayerOps::ControlCsvPreview renamed = LayerOps::previewControlPointsCsv(&cp, moved, nullptr, &error);
  QVERIFY2(renamed.summary.contains(QStringLiteral("이름은 같은데 위치가 다른 점이 1개")), qPrintable(renamed.summary));
  QCOMPARE(LayerOps::importControlPointsCsv(&cp, moved, &error, false), 1);
  QCOMPARE(cp.featureCount(), 3LL);
}

// A GeoPackage count can fail in GDAL ("unable to open database file", CI 2026-10-03) and goes stale
// when another layer object saves; the X·Y check must still find the saved survey area.
void TestControlPointCsv::swapSuggestion_findsSurveyAreaTheLayerCountMissed() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QString error;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir.path(), QStringLiteral("csvxy"), &error,
                                                             QStringLiteral("EPSG:5186"));
  QVERIFY2(!gpkg.isEmpty(), qPrintable(error));
  const QString uri = gpkg + QStringLiteral("|layername=survey_area");
  QgsProject project;
  auto* area = new QgsVectorLayer(uri, QStringLiteral("조사구역"), QStringLiteral("ogr"));
  QVERIFY(area->isValid());
  project.addMapLayer(area);
  QCOMPARE(area->featureCount(), 0LL);  // counted while empty
  {
    QgsVectorLayer writer(uri, QStringLiteral("writer"), QStringLiteral("ogr"));
    QgsFeature zone(writer.fields());
    zone.setGeometry(QgsGeometry::fromRect(QgsRectangle(148078, 98110, 148110, 98134)));
    QgsFeatureList list{zone};
    QVERIFY(writer.dataProvider()->addFeatures(list));
  }
  QgsVectorLayer cp(gpkg + QStringLiteral("|layername=control_points"), QStringLiteral("control_points"),
                    QStringLiteral("ogr"));
  const QString csv = QDir(dir.path()).filePath(QStringLiteral("korean-xy.csv"));
  QVERIFY(writeCsv(csv, QStringLiteral("point_id,X,Y\nK1,98120,148100\n")));
  const LayerOps::ControlCsvPreview preview = LayerOps::previewControlPointsCsv(&cp, csv, &project, &error);
  QVERIFY2(preview.ok, qPrintable(error));
  QVERIFY2(preview.swapSuggested, qPrintable(preview.summary));
}

#include "test_control_point_csv.moc"

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH", QStringLiteral("A:/OSGeo4W/apps/qgis-dev")),
                                true);
  QgsApplication::initQgis();
  TestControlPointCsv tc;
  const int rc = QTest::qExec(&tc, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
