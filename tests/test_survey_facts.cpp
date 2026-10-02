// P3 home: SurveyFacts records (counts, check, package), the size+mtime fingerprint, the
// 24-entry cap, the wording of the summary line and the read-side GPKG probe with its
// guards. No QGIS; GDAL only to write and count a small GeoPackage.
#include <QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>

#include <gdal.h>
#include <ogr_api.h>

#include "core/SurveyFacts.h"
#include "core/SurveyFileFingerprint.h"

namespace {

QString touch(const QString& path, const QByteArray& bytes = "fixture") {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) return {};
  file.write(bytes);
  return QFileInfo(path).absoluteFilePath();
}

void addPolygons(GDALDatasetH ds, const char* table, int count) {
  OGRLayerH layer = GDALDatasetCreateLayer(ds, table, nullptr, wkbPolygon, nullptr);
  for (int i = 0; i < count; ++i) {
    OGRFeatureH feature = OGR_F_Create(OGR_L_GetLayerDefn(layer));
    OGRGeometryH geometry = nullptr;
    char wkt[] = "POLYGON((0 0,10 0,10 10,0 10,0 0))";
    char* cursor = wkt;
    OGR_G_CreateFromWkt(&cursor, nullptr, &geometry);
    OGR_F_SetGeometryDirectly(feature, geometry);
    OGR_L_CreateFeature(layer, feature);
    OGR_F_Destroy(feature);
  }
}

// A GeoPackage with the three counted tables (survey_area 1, feature_poly 2, feature_line 0).
QString writeGpkg(const QString& path) {
  GDALAllRegister();
  GDALDriverH driver = GDALGetDriverByName("GPKG");
  if (!driver) return {};
  QDir().mkpath(QFileInfo(path).absolutePath());
  GDALDatasetH ds = GDALCreate(driver, path.toUtf8().constData(), 0, 0, 0, GDT_Unknown, nullptr);
  if (!ds) return {};
  addPolygons(ds, "survey_area", 1);
  addPolygons(ds, "feature_poly", 2);
  GDALDatasetCreateLayer(ds, "feature_line", nullptr, wkbLineString, nullptr);
  GDALClose(ds);
  return QFileInfo(path).absoluteFilePath();
}

}  // namespace

class TestSurveyFacts : public QObject {
  Q_OBJECT
  QTemporaryDir m_dir;
  int m_ini = 0;

  QSettings settings() { return QSettings(m_dir.filePath(QStringLiteral("facts%1.ini").arg(++m_ini)), QSettings::IniFormat); }

private slots:
  void rememberCounts_roundTrip() {
    QSettings st = settings();
    const QString path = touch(m_dir.filePath(QStringLiteral("a/조사.gpkg")));
    QVERIFY(!SurveyFacts::lookup(st, path).known);
    SurveyFacts::rememberCounts(st, path, 2, 5);
    const SurveyFacts::Facts facts = SurveyFacts::lookup(st, path);
    QVERIFY(facts.known && facts.hasCounts());
    QCOMPARE(facts.areas, 2);
    QCOMPARE(facts.features, 5);
    QVERIFY(facts.countedAtMs > 0);
    QVERIFY(!facts.hasCheck());
    // Case and separator variants of the same Windows path share one record.
    QVERIFY(SurveyFacts::lookup(st, QDir::toNativeSeparators(path).toUpper()).hasCounts());
  }

  void rememberCheck_roundTrip() {
    QSettings st = settings();
    const QString path = touch(m_dir.filePath(QStringLiteral("b/조사.gpkg")));
    SurveyFacts::rememberCounts(st, path, 1, 1);
    SurveyFacts::rememberCheck(st, path, 0, 2);
    const SurveyFacts::Facts facts = SurveyFacts::lookup(st, path);
    QVERIFY(facts.hasCheck() && facts.hasCounts());  // counts survive the check record
    QCOMPARE(facts.errors, 0);
    QCOMPARE(facts.warnings, 2);
  }

  void markPackaged_setsTime() {
    QSettings st = settings();
    const QString path = touch(m_dir.filePath(QStringLiteral("c/조사.gpkg")));
    SurveyFacts::rememberCheck(st, path, 0, 0);
    QCOMPARE(SurveyFacts::lookup(st, path).packagedAtMs, qint64(0));
    SurveyFacts::markPackaged(st, path);
    const SurveyFacts::Facts facts = SurveyFacts::lookup(st, path);
    QVERIFY(facts.packagedAtMs > 0);
    QVERIFY(facts.hasCheck());
  }

  void fingerprintChange_dropsCounts() {
    QSettings st = settings();
    const QString path = touch(m_dir.filePath(QStringLiteral("d/조사.gpkg")));
    SurveyFacts::rememberCounts(st, path, 1, 1);
    QVERIFY(SurveyFacts::lookup(st, path).hasCounts());
    touch(path, "fixture-changed-elsewhere");  // another PC saved: size differs
    QVERIFY(!SurveyFacts::lookup(st, path).known);
    QVERIFY(!SurveyFacts::lookup(st, path).hasCounts());
    SurveyFacts::rememberCounts(st, path, 4, 4);  // our own save refreshes the fingerprint
    QCOMPARE(SurveyFacts::lookup(st, path).areas, 4);
  }

  void probe_countsFromLocalGpkg() {
    const QString gpkg = writeGpkg(m_dir.filePath(QStringLiteral("e/현장.gpkg")));
    QVERIFY2(!gpkg.isEmpty(), "GPKG driver unavailable");
    int areas = -1;
    int features = -1;
    QVERIFY(SurveyFacts::countLayers(gpkg, &areas, &features));
    QCOMPARE(areas, 1);
    QCOMPARE(features, 2);
    QVERIFY(!SurveyFacts::countLayers(touch(m_dir.filePath(QStringLiteral("e/글자.gpkg"))), &areas, &features));
    if (SurveyFacts::probeBlockFor(gpkg) == SurveyFacts::ProbeBlock::NotFixedDrive)
      QSKIP("temp folder is not on a fixed drive; the probe stays off there by design");
    QCOMPARE(SurveyFacts::probeBlockFor(gpkg), SurveyFacts::ProbeBlock::None);
    QSettings st = settings();
    SurveyFacts::rememberCheck(st, gpkg, 1, 0);  // a valid check record keeps its fields
    QCOMPARE(SurveyFacts::probeCountsIfUnknown(st, {gpkg}), 1);
    const SurveyFacts::Facts facts = SurveyFacts::lookup(st, gpkg);
    QVERIFY(facts.hasCounts() && facts.hasCheck());
    QCOMPARE(facts.areas, 1);
    QCOMPARE(facts.features, 2);
    QCOMPARE(facts.errors, 1);
    QCOMPARE(SurveyFacts::probeCountsIfUnknown(st, {gpkg}), 0);  // already known: no second open
  }

  void probe_skipsRememberedRemoteRemovable() {
    using Block = SurveyFacts::ProbeBlock;
    QCOMPARE(SurveyFacts::probeBlockFor(QStringLiteral("//server/share/조사.gpkg")), Block::Remote);
    QCOMPARE(SurveyFacts::probeBlockFor(QString()), Block::EmptyPath);
    QCOMPARE(SurveyFacts::probeBlockFor(m_dir.filePath(QStringLiteral("f/없음.gpkg"))), Block::MissingOrEmpty);
    QCOMPARE(SurveyFacts::probeBlockFor(touch(m_dir.filePath(QStringLiteral("f/빈.gpkg")), QByteArray())),
             Block::MissingOrEmpty);
    const QString open = touch(m_dir.filePath(QStringLiteral("f/열림.gpkg")));
    SurveyFileFingerprint::remember(open);  // this process has it open: never read behind its back
    QCOMPARE(SurveyFacts::probeBlockFor(open), Block::OpenInApp);
    SurveyFileFingerprint::forget(open);
    QSettings st = settings();
    QCOMPARE(SurveyFacts::probeCountsIfUnknown(st, {QStringLiteral("//server/share/조사.gpkg"), open}), 0);
    QVERIFY(!SurveyFacts::lookup(st, open).known);
  }

  void probe_disabledByEnv() {
    qputenv("KA_HGIS_HOME_PROBE", "0");
    QVERIFY(!SurveyFacts::probeEnabled());
    const QString gpkg = writeGpkg(m_dir.filePath(QStringLiteral("g/현장.gpkg")));
    QCOMPARE(SurveyFacts::probeBlockFor(gpkg), SurveyFacts::ProbeBlock::Disabled);
    QSettings st = settings();
    QCOMPARE(SurveyFacts::probeCountsIfUnknown(st, {gpkg}), 0);
    qunsetenv("KA_HGIS_HOME_PROBE");
    QVERIFY(SurveyFacts::probeEnabled());
  }

  void summaryLine_wordings() {
    SurveyFacts::Facts facts;
    QCOMPARE(SurveyFacts::summaryLine(facts),
             QStringLiteral("아직 검수하지 않았습니다 — 지도 탭 「검수·제출」에서 확인합니다."));
    facts.known = true;
    facts.checkedAtMs = 1;
    facts.errors = 0;
    facts.warnings = 2;
    QCOMPARE(SurveyFacts::summaryLine(facts), QStringLiteral("제출 준비: 오류 0 · 경고 2"));
    facts.known = false;  // the file changed since: the old check no longer speaks
    QVERIFY(SurveyFacts::summaryLine(facts).startsWith(QStringLiteral("아직 검수")));
  }

  void forget_removesEntry() {
    QSettings st = settings();
    const QString path = touch(m_dir.filePath(QStringLiteral("h/조사.gpkg")));
    SurveyFacts::rememberCounts(st, path, 1, 1);
    QCOMPARE(SurveyFacts::storedKeys(st).size(), 1);
    SurveyFacts::forget(st, path);
    QVERIFY(SurveyFacts::storedKeys(st).isEmpty());
    QVERIFY(!SurveyFacts::lookup(st, path).known);
  }

  void capAt24Entries() {
    QSettings st = settings();
    QString first;
    for (int i = 0; i < 30; ++i) {
      const QString path = touch(m_dir.filePath(QStringLiteral("i/조사%1.gpkg").arg(i)));
      if (i == 0) first = path;
      SurveyFacts::rememberCounts(st, path, i, i);
    }
    QCOMPARE(SurveyFacts::storedKeys(st).size(), SurveyFacts::kMaxEntries);
    QVERIFY(!SurveyFacts::lookup(st, first).known);  // the oldest fell off
    QCOMPARE(SurveyFacts::lookup(st, m_dir.filePath(QStringLiteral("i/조사29.gpkg"))).areas, 29);
    int stray = 0;
    for (const QString& key : st.allKeys())
      if (key.startsWith(QStringLiteral("RecentSurveys/facts/"))) ++stray;
    QCOMPARE(stray, SurveyFacts::kMaxEntries);  // dropped records leave no key behind
  }
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  TestSurveyFacts test;
  return QTest::qExec(&test, argc, argv);
}

#include "test_survey_facts.moc"
