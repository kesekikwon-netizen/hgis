// F044/F045/F054: feature numbering (suggest next, duplicates), value choices and spelling,
// audit stamps and measures. Nothing here commits or opens a dialog.
#include <QtTest>
#include <QDateTime>
#include <cmath>
#include <memory>

#include "core/FeatureNumbering.h"
#include "core/FeaturePresets.h"
#include "core/FeatureRecord.h"

#include <qgis.h>
#include <qgsapplication.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgsfields.h>
#include <qgsgeometry.h>
#include <qgspointxy.h>
#include <qgsrectangle.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorlayer.h>

namespace {
std::unique_ptr<QgsVectorLayer> memoryLayer(const QString& uri) {
  return std::make_unique<QgsVectorLayer>(uri, QStringLiteral("t"), QStringLiteral("memory"));
}

QgsFeatureId addRow(QgsVectorLayer* layer, const QVariantMap& values, double offset = 0) {
  QgsFeature f(layer->fields());
  for (auto it = values.cbegin(); it != values.cend(); ++it) f.setAttribute(it.key(), it.value());
  // The memory provider refuses a geometry of another type, so the shape follows the layer.
  const QgsRectangle box(200000 + offset, 450000, 200010 + offset, 450005);
  if (layer->geometryType() == Qgis::GeometryType::Polygon) f.setGeometry(QgsGeometry::fromRect(box));
  else if (layer->geometryType() == Qgis::GeometryType::Line)
    f.setGeometry(QgsGeometry::fromPolylineXY(
        {QgsPointXY(box.xMinimum(), box.yMinimum()), QgsPointXY(box.xMaximum(), box.yMaximum())}));
  else f.setGeometry(QgsGeometry::fromPointXY(box.center()));
  if (!layer->dataProvider()->addFeature(f)) return FID_NULL;
  return f.id();
}

std::unique_ptr<QgsVectorLayer> featureLayer() {
  auto layer = memoryLayer(QStringLiteral("Polygon?crs=EPSG:5187&field=kind:string&field=period:string"
                                          "&field=feature_no:string"));
  addRow(layer.get(), {{QStringLiteral("kind"), QStringLiteral("주거지")}, {QStringLiteral("feature_no"), QStringLiteral("1호")}});
  addRow(layer.get(), {{QStringLiteral("kind"), QStringLiteral("주거지")}, {QStringLiteral("feature_no"), QStringLiteral("02호")}}, 20);
  addRow(layer.get(), {{QStringLiteral("kind"), QStringLiteral("수혈")}, {QStringLiteral("feature_no"), QStringLiteral("1호")}}, 40);
  return layer;
}
}  // namespace

class TestFeatureRecords : public QObject {
  Q_OBJECT
private slots:
  void normalizedAndParse();
  void nextFrom_keepsTheWrittenForm();
  void suggestNext_runsPerKind();
  void artifactNumbers_runOverTheLayer();
  void duplicates_warnOnlyWithinKind();
  void choices_presetOrderThenSurveySpelling();
  void canonicalValue_reusesSurveySpelling();
  void stampNewAndTouch_fillAuditFields();
  void measure_areaPerimeterLength();
};

void TestFeatureRecords::normalizedAndParse() {
  QCOMPARE(FeatureNumbering::normalized(QStringLiteral("1호")), QStringLiteral("1"));
  QCOMPARE(FeatureNumbering::normalized(QStringLiteral("01호")), QStringLiteral("1"));
  QCOMPARE(FeatureNumbering::normalized(QStringLiteral("1 호")), QStringLiteral("1"));
  QCOMPARE(FeatureNumbering::normalized(QStringLiteral("제1호")), QStringLiteral("1"));
  QCOMPARE(FeatureNumbering::normalized(QStringLiteral("s-01")), QStringLiteral("S-1"));
  QCOMPARE(FeatureNumbering::normalized(QString()), QString());
  const FeatureNumbering::Parts parts = FeatureNumbering::parse(QStringLiteral("제03호"));
  QCOMPARE(parts.prefix, QStringLiteral("제"));
  QCOMPARE(parts.value, qint64(3));
  QCOMPARE(parts.width, 2);
  QCOMPARE(parts.suffix, QStringLiteral("호"));
  QCOMPARE(FeatureNumbering::parse(QStringLiteral("A-A'")).value, qint64(-1));
}

void TestFeatureRecords::nextFrom_keepsTheWrittenForm() {
  const QString fallback = FeatureNumbering::defaultTemplate(QStringLiteral("feature_no"));
  QCOMPARE(FeatureNumbering::nextFrom({QStringLiteral("1호"), QStringLiteral("2호")}, fallback), QStringLiteral("3호"));
  QCOMPARE(FeatureNumbering::nextFrom({QStringLiteral("02호")}, fallback), QStringLiteral("03호"));
  QCOMPARE(FeatureNumbering::nextFrom({}, fallback), QStringLiteral("1호"));
  QCOMPARE(FeatureNumbering::nextFrom({QStringLiteral("B-3")}, QString()), QStringLiteral("B-4"));
  QCOMPARE(FeatureNumbering::nextFrom({QStringLiteral("A-A'")}, QString()), QString());  // sections: no guess
  QCOMPARE(FeatureNumbering::defaultTemplate(QStringLiteral("point_id")), QStringLiteral("P%1"));
}

void TestFeatureRecords::suggestNext_runsPerKind() {
  auto layer = featureLayer();
  QCOMPARE(FeatureNumbering::suggestNext(layer.get(), QStringLiteral("주거지")), QStringLiteral("03호"));
  QCOMPARE(FeatureNumbering::suggestNext(layer.get(), QStringLiteral(" 주거 지")), QStringLiteral("03호"));
  QCOMPARE(FeatureNumbering::suggestNext(layer.get(), QStringLiteral("수혈")), QStringLiteral("2호"));
  QCOMPARE(FeatureNumbering::suggestNext(layer.get(), QStringLiteral("구")), QStringLiteral("1호"));
  auto noNumber = memoryLayer(QStringLiteral("Polygon?crs=EPSG:5187&field=kind:string"));
  QCOMPARE(FeatureNumbering::suggestNext(noNumber.get(), QStringLiteral("주거지")), QString());
}

void TestFeatureRecords::artifactNumbers_runOverTheLayer() {
  auto layer = memoryLayer(QStringLiteral("Point?crs=EPSG:5187&field=kind:string&field=artifact_no:string"));
  QVERIFY(addRow(layer.get(), {{QStringLiteral("kind"), QStringLiteral("토기")}, {QStringLiteral("artifact_no"), QStringLiteral("1")}}) !=
          FID_NULL);
  QVERIFY(addRow(layer.get(), {{QStringLiteral("kind"), QStringLiteral("석기")}, {QStringLiteral("artifact_no"), QStringLiteral("2")}},
                 20) != FID_NULL);
  const FeatureNumbering::Fields fields = FeatureNumbering::fieldsOf(layer.get());
  QCOMPARE(fields.numberName, QStringLiteral("artifact_no"));
  QCOMPARE(fields.kind, -1);
  QCOMPARE(FeatureNumbering::suggestNext(layer.get(), QStringLiteral("석기")), QStringLiteral("3"));
  QCOMPARE(FeatureNumbering::sameNumber(layer.get(), QStringLiteral("석기"), QStringLiteral("1")).size(), 1);
  // 유구 kind presets are not offered for artifacts; the layer's own kinds are.
  QCOMPARE(FeatureRecord::choices(layer.get(), QStringLiteral("kind")),
           QStringList({QStringLiteral("석기"), QStringLiteral("토기")}));
}

void TestFeatureRecords::duplicates_warnOnlyWithinKind() {
  auto layer = featureLayer();
  const QgsFeatureId extra =
      addRow(layer.get(), {{QStringLiteral("kind"), QStringLiteral("주거지")}, {QStringLiteral("feature_no"), QStringLiteral("2 호")}}, 60);
  QVERIFY(extra != FID_NULL);
  // "02호" and "2 호" are the same number of the same kind; 수혈 1호 is not a repeat of 주거지 1호.
  QCOMPARE(FeatureNumbering::sameNumber(layer.get(), QStringLiteral("주거지"), QStringLiteral("2호"), extra).size(), 1);
  QCOMPARE(FeatureNumbering::sameNumber(layer.get(), QStringLiteral("수혈"), QStringLiteral("2호")).size(), 0);
  const QList<FeatureNumbering::Duplicate> dups = FeatureNumbering::duplicates(layer.get());
  QCOMPARE(dups.size(), 2);
  QStringList labels;
  for (const auto& d : dups) labels << d.label;
  QVERIFY2(labels.contains(QStringLiteral("주거지 02호")) && labels.contains(QStringLiteral("주거지 2 호")),
           qPrintable(labels.join(QLatin1Char(','))));
  // Nothing was renumbered: a number is only pointed out.
  QCOMPARE(layer->getFeature(extra).attribute(QStringLiteral("feature_no")).toString(), QStringLiteral("2 호"));
}

void TestFeatureRecords::choices_presetOrderThenSurveySpelling() {
  QVERIFY(FeaturePresets::instance().ensureLoaded());
  auto layer = memoryLayer(QStringLiteral("Polygon?crs=EPSG:5187&field=kind:string&field=period:string"));
  addRow(layer.get(), {{QStringLiteral("period"), QStringLiteral("청동기시대")}});
  addRow(layer.get(), {{QStringLiteral("period"), QStringLiteral("미상기")}, {QStringLiteral("kind"), QStringLiteral("토기가마")}});
  const QStringList periods = FeatureRecord::choices(layer.get(), QStringLiteral("period"));
  QCOMPARE(periods.at(0), QStringLiteral("구석기"));
  QCOMPARE(periods.at(2), QStringLiteral("청동기시대"));  // the survey's spelling replaces the preset's
  QVERIFY(!periods.contains(QStringLiteral("청동기")));
  QCOMPARE(periods.last(), QStringLiteral("미상기"));  // own values after the presets
  const QStringList kinds = FeatureRecord::choices(layer.get(), QStringLiteral("kind"));
  QCOMPARE(kinds.first(), QStringLiteral("주거지"));
  QVERIFY(kinds.contains(QStringLiteral("토기가마")));
}

void TestFeatureRecords::canonicalValue_reusesSurveySpelling() {
  auto layer = memoryLayer(QStringLiteral("Polygon?crs=EPSG:5187&field=kind:string&field=period:string"));
  addRow(layer.get(), {{QStringLiteral("kind"), QStringLiteral("주거지")}, {QStringLiteral("period"), QStringLiteral("청동기시대")}});
  QCOMPARE(FeatureRecord::canonicalValue(layer.get(), QStringLiteral("kind"), QStringLiteral("주거 지")), QStringLiteral("주거지"));
  QCOMPARE(FeatureRecord::canonicalValue(layer.get(), QStringLiteral("period"), QStringLiteral("청동기 시대")),
           QStringLiteral("청동기시대"));
  QCOMPARE(FeatureRecord::canonicalValue(nullptr, QStringLiteral("kind"), QStringLiteral("수 혈")), QStringLiteral("수혈"));
  QCOMPARE(FeatureRecord::canonicalValue(nullptr, QStringLiteral("kind"), QStringLiteral(" 토기   가마 ")),
           QStringLiteral("토기 가마"));  // free text stays, spaces tidied
}

void TestFeatureRecords::stampNewAndTouch_fillAuditFields() {
  auto layer = memoryLayer(QStringLiteral("Polygon?crs=EPSG:5187&field=kind:string&field=uid:string"
                                          "&field=created_at:string&field=updated_at:string"));
  QgsFeature f(layer->fields());
  QVERIFY(FeatureRecord::stampNew(f));
  const QString uid = f.attribute(QStringLiteral("uid")).toString();
  QCOMPARE(uid.size(), 36);
  const QDateTime created = QDateTime::fromString(f.attribute(QStringLiteral("created_at")).toString(), Qt::ISODate);
  QVERIFY(created.isValid());
  QVERIFY(qAbs(created.secsTo(QDateTime::currentDateTime())) < 60);
  QVERIFY(!FeatureRecord::stampNew(f));  // filled values are kept
  QCOMPARE(f.attribute(QStringLiteral("uid")).toString(), uid);

  f.setAttribute(QStringLiteral("updated_at"), QStringLiteral("2000-01-01T00:00:00+09:00"));
  f.setGeometry(QgsGeometry::fromRect(QgsRectangle(0, 0, 1, 1)));
  QVERIFY(layer->dataProvider()->addFeature(f));
  QVERIFY(!FeatureRecord::touch(layer.get(), f.id()));  // not editing: nothing written
  QVERIFY(layer->startEditing());
  QVERIFY(FeatureRecord::touch(layer.get(), f.id()));
  QVERIFY(layer->isModified());
  QVERIFY(layer->getFeature(f.id()).attribute(QStringLiteral("updated_at")).toString().startsWith(
      QString::number(QDate::currentDate().year())));
  QgsFeature stored;
  QVERIFY(layer->dataProvider()->getFeatures(QgsFeatureRequest(f.id())).nextFeature(stored));
  QCOMPARE(stored.attribute(QStringLiteral("updated_at")).toString(), QStringLiteral("2000-01-01T00:00:00+09:00"));
  layer->rollBack();
}

void TestFeatureRecords::measure_areaPerimeterLength() {
  auto polygons = memoryLayer(QStringLiteral("Polygon?crs=EPSG:5187"));
  QgsFeature box(polygons->fields());
  box.setGeometry(QgsGeometry::fromRect(QgsRectangle(0, 0, 10, 5)));
  const FeatureRecord::Measures m = FeatureRecord::measure(polygons.get(), box);
  QVERIFY(qFuzzyCompare(m.area, 50.0));
  QVERIFY(qFuzzyCompare(m.perimeter, 30.0));
  QVERIFY(std::isnan(m.length));
  auto lines = memoryLayer(QStringLiteral("LineString?crs=EPSG:5187"));
  QgsFeature line(lines->fields());
  line.setGeometry(QgsGeometry::fromPolylineXY({QgsPointXY(0, 0), QgsPointXY(3, 4)}));
  QVERIFY(qFuzzyCompare(FeatureRecord::measure(lines.get(), line).length, 5.0));
  auto degrees = memoryLayer(QStringLiteral("Polygon?crs=EPSG:4326"));
  QVERIFY(std::isnan(FeatureRecord::measure(degrees.get(), box).area));  // no metres in degrees
}

#include "test_feature_records.moc"

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH", QStringLiteral("A:/OSGeo4W/apps/qgis-dev")),
                                true);
  QgsApplication::initQgis();
  TestFeatureRecords tc;
  const int rc = QTest::qExec(&tc, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
