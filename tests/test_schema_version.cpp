// F053/F054/F112: one schema table (SurveySchema) for new surveys, a schema version inside
// the GPKG, and a migration that runs only on the save generation copy (never on open).
#include <QtTest>
#include <QSet>
#include <QTemporaryDir>

#include "core/SubmitShp.h"
#include "core/SurveyProjectFactory.h"
#include "core/SurveySchema.h"
#include "core/SurveyStorage.h"

#include <gdal.h>
#include <ogr_api.h>

#include <qgsapplication.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransformcontext.h>
#include <qgsfeature.h>
#include <qgsfield.h>
#include <qgsfields.h>
#include <qgsgeometry.h>
#include <qgsrectangle.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectorlayer.h>

namespace {
// An older survey: the four-field feature_poly and a control_points table, no stamp,
// no artifact_point table.
bool writeOldSurvey(const QString& gpkg, QString* error) {
  struct Table {
    const char* name;
    const char* uri;
  };
  const Table tables[] = {
      {"feature_poly", "Polygon?crs=EPSG:5187&field=kind:string&field=period:string&field=feature_no:string"
                       "&field=note:string"},
      {"control_points", "Point?crs=EPSG:5187&field=point_id:string&field=x:double&field=y:double"},
  };
  bool first = true;
  for (const Table& t : tables) {
    QgsVectorLayer mem(QString::fromLatin1(t.uri), QString::fromLatin1(t.name), QStringLiteral("memory"));
    if (QString::fromLatin1(t.name) == QLatin1String("feature_poly")) {
      QgsFeature f(mem.fields());
      f.setAttribute(QStringLiteral("kind"), QStringLiteral("주거지"));
      f.setAttribute(QStringLiteral("feature_no"), QStringLiteral("1호"));
      f.setGeometry(QgsGeometry::fromRect(QgsRectangle(200000, 450000, 200010, 450010)));
      mem.dataProvider()->addFeature(f);
    }
    QgsVectorFileWriter::SaveVectorOptions options;
    options.driverName = QStringLiteral("GPKG");
    options.layerName = QString::fromLatin1(t.name);
    options.actionOnExistingFile =
        first ? QgsVectorFileWriter::CreateOrOverwriteFile : QgsVectorFileWriter::CreateOrOverwriteLayer;
    first = false;
    if (QgsVectorFileWriter::writeAsVectorFormatV3(&mem, gpkg, QgsCoordinateTransformContext(), options, error) !=
        QgsVectorFileWriter::NoError)
      return false;
  }
  return true;
}

QStringList fieldNames(const QString& gpkg, const QString& table) {
  QgsVectorLayer layer(gpkg + QStringLiteral("|layername=") + table, table, QStringLiteral("ogr"));
  QStringList out;
  for (const QgsField& f : layer.fields())
    if (f.name().compare(QLatin1String("fid"), Qt::CaseInsensitive) != 0) out << f.name();
  return out;
}

// The first feature of a table, read with GDAL only: a QGIS feature iterator would leave a
// pooled connection open and the temp dir could not be removed on Windows.
struct FirstRow {
  long long count = -1;
  QString kind;
  QString featureNo;
  bool depthNull = false;
  bool depthIsReal = false;
};

FirstRow firstRow(const QString& gpkg, const char* table) {
  FirstRow out;
  const char* drivers[] = {"GPKG", nullptr};
  GDALDatasetH ds =
      GDALOpenEx(gpkg.toUtf8().constData(), GDAL_OF_VECTOR | GDAL_OF_READONLY, drivers, nullptr, nullptr);
  if (!ds) return out;
  if (OGRLayerH layer = GDALDatasetGetLayerByName(ds, table)) {
    out.count = OGR_L_GetFeatureCount(layer, TRUE);
    OGRFeatureDefnH defn = OGR_L_GetLayerDefn(layer);
    const int depth = OGR_FD_GetFieldIndex(defn, "depth_m");
    out.depthIsReal = depth >= 0 && OGR_Fld_GetType(OGR_FD_GetFieldDefn(defn, depth)) == OFTReal;
    OGR_L_ResetReading(layer);
    if (OGRFeatureH f = OGR_L_GetNextFeature(layer)) {
      out.kind = QString::fromUtf8(OGR_F_GetFieldAsString(f, OGR_F_GetFieldIndex(f, "kind")));
      out.featureNo = QString::fromUtf8(OGR_F_GetFieldAsString(f, OGR_F_GetFieldIndex(f, "feature_no")));
      out.depthNull = depth >= 0 && !OGR_F_IsFieldSetAndNotNull(f, depth);
      OGR_F_Destroy(f);
    }
  }
  GDALClose(ds);
  return out;
}
}  // namespace

class TestSchemaVersion : public QObject {
  Q_OBJECT
private slots:
  void schemaTable_isShapefileSafeAndOptional();
  void newSurvey_isStampedWithRecordFields();
  void oldSurvey_gainsFieldsOnlyThroughMigration();
  void migration_neverLowersANewerStamp();
};

void TestSchemaVersion::schemaTable_isShapefileSafeAndOptional() {
  QStringList keys;
  for (const SurveySchema::LayerDef& def : SurveySchema::layers()) {
    keys << QString::fromLatin1(def.name);
    QSet<QString> seen;
    QSet<QString> dbfNames;
    for (const SurveySchema::FieldDef& field : def.fields) {
      const QString name = QString::fromLatin1(field.name);
      QVERIFY2(!seen.contains(name), qPrintable(QString::fromLatin1(def.name) + QLatin1Char('.') + name));
      seen.insert(name);
      // The submit package is a shapefile: through the export alias table every name fits
      // DBF's 10 bytes, and only the two legacy names (survey_name, artifact_no) need an alias.
      const QString dbf = SubmitShp::fieldNameFor(name, dbfNames);
      QVERIFY2(dbf.toUtf8().size() <= 10, qPrintable(name + QStringLiteral(" -> ") + dbf));
      const bool legacyAlias = name == QLatin1String("survey_name") || name == QLatin1String("artifact_no");
      QVERIFY2(dbf == name || legacyAlias, qPrintable(name + QStringLiteral(": 별칭 없이 DBF 10바이트를 넘습니다")));
    }
  }
  QCOMPARE(keys, QStringList({QStringLiteral("survey_area"), QStringLiteral("feature_poly"),
                              QStringLiteral("feature_line"), QStringLiteral("section_line"),
                              QStringLiteral("control_points"), QStringLiteral("artifact_point"),
                              QStringLiteral("trial_trench")}));
  const QgsFields poly = SurveySchema::fieldsFor(QStringLiteral("feature_poly"));
  QCOMPARE(poly.at(0).name(), QStringLiteral("kind"));  // older columns keep their order
  QCOMPARE(poly.at(2).name(), QStringLiteral("feature_no"));
  for (const char* name : {"depth_m", "top_el", "bottom_el", "layer_no", "relation", "status", "photo", "surveyor",
                           "surv_date", "uid", "created_at", "updated_at"})
    QVERIFY2(poly.lookupField(QString::fromLatin1(name)) >= 0, name);
  QCOMPARE(poly.field(QStringLiteral("depth_m")).type(), QMetaType::Double);
  QVERIFY(SurveySchema::fieldsFor(QStringLiteral("feature_line")).lookupField(QStringLiteral("feature_no")) >= 0);
  QVERIFY(SurveySchema::fieldsFor(QStringLiteral("artifact_point")).lookupField(QStringLiteral("in_feature")) >= 0);
  QVERIFY(SurveySchema::fieldsFor(QStringLiteral("nope")).isEmpty());
  QVERIFY(SurveySchema::isAutoField(QStringLiteral("uid")));
  QVERIFY(!SurveySchema::isAutoField(QStringLiteral("kind")));
  QCOMPARE(SurveySchema::labelKo(QStringLiteral("depth_m")), QStringLiteral("깊이(m)"));
}

void TestSchemaVersion::newSurvey_isStampedWithRecordFields() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QString error;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir.path(), QStringLiteral("판번호"), &error);
  QVERIFY2(!gpkg.isEmpty(), qPrintable(error));
  QCOMPARE(SurveySchema::readVersion(gpkg), SurveySchema::kCurrentVersion);
  QVERIFY(SurveyStorage::validateForOpen(gpkg, &error));
  QStringList expected;
  for (const QgsField& f : SurveySchema::fieldsFor(QStringLiteral("feature_poly"))) expected << f.name();
  QCOMPARE(fieldNames(gpkg, QStringLiteral("feature_poly")), expected);
  // A current survey needs nothing: no column is added and the stamp stays.
  const SurveySchema::MigrationResult same = SurveySchema::migrateGenerationCopy(gpkg);
  QVERIFY2(same.ok, qPrintable(same.error));
  QCOMPARE(same.fromVersion, SurveySchema::kCurrentVersion);
  QVERIFY(same.addedFields.isEmpty());
  QCOMPARE(SurveySchema::readVersion(gpkg), SurveySchema::kCurrentVersion);
  QCOMPARE(fieldNames(gpkg, QStringLiteral("feature_poly")), expected);
}

void TestSchemaVersion::oldSurvey_gainsFieldsOnlyThroughMigration() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString gpkg = dir.filePath(QStringLiteral("old.gpkg"));
  QString error;
  QVERIFY2(writeOldSurvey(gpkg, &error), qPrintable(error));
  QCOMPARE(SurveySchema::readVersion(gpkg), 0);
  const QStringList oldFields = fieldNames(gpkg, QStringLiteral("feature_poly"));
  // Reading (as opening does) changes nothing.
  QCOMPARE(SurveySchema::readVersion(gpkg), 0);
  QCOMPARE(fieldNames(gpkg, QStringLiteral("feature_poly")), oldFields);

  const SurveySchema::MigrationResult result = SurveySchema::migrateGenerationCopy(gpkg);
  QVERIFY2(result.ok, qPrintable(result.error));
  QCOMPARE(result.fromVersion, 0);
  QVERIFY(result.addedFields.contains(QStringLiteral("feature_poly.depth_m")));
  QVERIFY(result.addedFields.contains(QStringLiteral("feature_poly.uid")));
  QVERIFY(result.addedFields.contains(QStringLiteral("control_points.z")));
  for (const QString& added : result.addedFields)
    QVERIFY2(!added.startsWith(QLatin1String("artifact_point.")), "absent tables stay absent");
  QCOMPARE(SurveySchema::readVersion(gpkg), SurveySchema::kCurrentVersion);
  QVERIFY(SurveyStorage::validateForOpen(gpkg, &error));

  QVERIFY(fieldNames(gpkg, QStringLiteral("feature_poly")).contains(QStringLiteral("depth_m")));  // QGIS sees it
  const FirstRow row = firstRow(gpkg, "feature_poly");
  QCOMPARE(row.count, 1LL);
  QCOMPARE(row.kind, QStringLiteral("주거지"));  // values kept
  QCOMPARE(row.featureNo, QStringLiteral("1호"));
  QVERIFY(row.depthNull);  // new columns stay empty
  QVERIFY(row.depthIsReal);

  const SurveySchema::MigrationResult again = SurveySchema::migrateGenerationCopy(gpkg);
  QVERIFY(again.ok);
  QCOMPARE(again.fromVersion, SurveySchema::kCurrentVersion);
  QVERIFY(again.addedFields.isEmpty());
}

void TestSchemaVersion::migration_neverLowersANewerStamp() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString gpkg = dir.filePath(QStringLiteral("newer.gpkg"));
  QString error;
  QVERIFY2(writeOldSurvey(gpkg, &error), qPrintable(error));
  QVERIFY2(SurveySchema::writeVersion(gpkg, SurveySchema::kCurrentVersion + 6, &error), qPrintable(error));
  const SurveySchema::MigrationResult result = SurveySchema::migrateGenerationCopy(gpkg);
  QVERIFY2(result.ok, qPrintable(result.error));
  QCOMPARE(SurveySchema::readVersion(gpkg), SurveySchema::kCurrentVersion + 6);
  QVERIFY(!SurveySchema::migrateGenerationCopy(dir.filePath(QStringLiteral("missing.gpkg"))).ok);
}

#include "test_schema_version.moc"

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH", QStringLiteral("A:/OSGeo4W/apps/qgis-dev")),
                                true);
  QgsApplication::initQgis();
  TestSchemaVersion tc;
  const int rc = QTest::qExec(&tc, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
