// Per-rule regression net for data/rules/drawing_checklist.v1.json.
// Each rule has one state that violates only that rule; the engine must fail
// exactly that rule with the severity the JSON gives it. Adding a rule without
// a row here fails everyRuleHasAViolationRow.
#include "core/ChecklistEngine.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

namespace {
QString rulesFile() {
  const QStringList candidates = {
      QDir::current().filePath(QStringLiteral("data/rules/drawing_checklist.v1.json")),
      QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../../data/rules/drawing_checklist.v1.json")),
      QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../data/rules/drawing_checklist.v1.json")),
  };
  for (const QString& path : candidates)
    if (QFile::exists(path)) return path;
  return candidates.first();
}

QJsonArray jsonRules() {
  QFile file(rulesFile());
  if (!file.open(QIODevice::ReadOnly)) return {};
  return QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("rules")).toArray();
}

// Every key at its passing value.
QJsonObject cleanState() {
  QJsonObject st;
  st.insert(QStringLiteral("survey_area_count"), 1);
  st.insert(QStringLiteral("control_points_count"), 2);
  st.insert(QStringLiteral("feature_poly_count"), 1);
  st.insert(QStringLiteral("project_crs_set"), true);
  st.insert(QStringLiteral("project_crs_authid"), QStringLiteral("EPSG:5187"));
  for (const char* key : {"has_datum", "has_ellipsoid", "has_projection", "has_origin", "has_accuracy",
                          "has_kind_period", "survey_is_polygon", "features_within_survey",
                          "features_real_geometry", "geometries_valid", "geometries_nonempty",
                          "geometries_nonzero_area", "export_crs_valid", "required_fields_filled",
                          "control_points_point_id_unique", "feature_poly_feature_no_unique",
                          "sheet_covers_targets", "sheet_has_elements", "HERITAGE_PLEDGE_SCOPE",
                          "layout_exists:site_location", "layout_exists:feature_plan",
                          "layout_exists:feature_detail", "layout_exists:section"})
    st.insert(QString::fromLatin1(key), true);
  st.insert(QStringLiteral("has_abstract_marker"), false);
  return st;
}

struct Violation {
  const char* ruleId;
  const char* key;
  QJsonValue value;
};

const QVector<Violation>& violations() {
  static const QVector<Violation> rows = {
      {"REQ_DRAWING_SURVEY_AREA", "survey_area_count", 0},
      {"REQ_DRAWING_SITE_LOCATION", "layout_exists:site_location", false},
      {"REQ_DRAWING_FEATURE_PLAN", "layout_exists:feature_plan", false},
      {"REQ_DRAWING_FEATURE_DETAIL", "layout_exists:feature_detail", false},
      {"REQ_DRAWING_SECTION", "layout_exists:section", false},
      {"GEOMETRY_VALID", "geometries_valid", false},
      {"GEOMETRY_NOT_EMPTY", "geometries_nonempty", false},
      {"GEOMETRY_NONZERO_AREA", "geometries_nonzero_area", false},
      {"SURVEY_POLYGON_ONLY", "survey_is_polygon", false},
      {"SURVEY_NO_ABSTRACT_MARKER", "has_abstract_marker", true},
      {"GCP_MIN_TWO", "control_points_count", 1},
      {"GCP_DATUM_META", "has_datum", false},
      {"GCP_ELLIPSOID_META", "has_ellipsoid", false},
      {"GCP_PROJECTION_META", "has_projection", false},
      {"GCP_ORIGIN_META", "has_origin", false},
      {"GCP_ACCURACY_NOTE", "has_accuracy", false},
      {"FEATURE_NO_SYMBOL_ONLY", "features_real_geometry", false},
      {"FEATURE_LEGEND_FIELDS", "has_kind_period", false},
      {"EXTENT_MATCH", "features_within_survey", false},
      {"CRS_PROJECT_SET", "project_crs_set", false},
      {"EXPORT_SHP_READY", "export_crs_valid", false},
      {"CRS_WORK_BELT", "project_crs_authid", QStringLiteral("EPSG:5179")},
      {"SHEET_COVERS_TARGETS", "sheet_covers_targets", false},
      {"SHEET_ELEMENTS", "sheet_has_elements", false},
      {"REQUIRED_FIELDS", "required_fields_filled", false},
      {"GCP_POINT_ID_UNIQUE", "control_points_point_id_unique", false},
      {"FEATURE_NO_UNIQUE", "feature_poly_feature_no_unique", false},
      {"HERITAGE_PLEDGE_SCOPE", "HERITAGE_PLEDGE_SCOPE", false},
  };
  return rows;
}

QString writeRules(QTemporaryDir& dir, const QJsonArray& rules) {
  const QString path = dir.filePath(QStringLiteral("rules.json"));
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) return {};
  file.write(QJsonDocument(QJsonObject{{QStringLiteral("rules"), rules}}).toJson());
  return path;
}
}  // namespace

class TestChecklistRules : public QObject {
  Q_OBJECT
private slots:
  void cleanStatePassesEveryRule() {
    ChecklistEngine engine;
    QVERIFY2(engine.loadRules(rulesFile()), qPrintable(rulesFile()));
    for (const CheckResult& r : engine.evaluate(cleanState()))
      QVERIFY2(r.passed, qPrintable(r.id));
  }

  void everyRuleHasAViolationRow() {
    QStringList covered;
    for (const Violation& v : violations()) covered << QString::fromLatin1(v.ruleId);
    const QJsonArray rules = jsonRules();
    QVERIFY(!rules.isEmpty());
    for (const QJsonValue& rule : rules) {
      const QString id = rule.toObject().value(QStringLiteral("id")).toString();
      QVERIFY2(covered.contains(id), qPrintable(QStringLiteral("no violation row for %1").arg(id)));
    }
  }

  void singleViolationFailsOnlyThatRule_data() {
    QTest::addColumn<QString>("ruleId");
    QTest::addColumn<QString>("key");
    QTest::addColumn<QVariant>("value");
    for (const Violation& v : violations())
      QTest::newRow(v.ruleId) << QString::fromLatin1(v.ruleId) << QString::fromLatin1(v.key) << v.value.toVariant();
  }

  void singleViolationFailsOnlyThatRule() {
    QFETCH(QString, ruleId);
    QFETCH(QString, key);
    QFETCH(QVariant, value);
    ChecklistEngine engine;
    QVERIFY(engine.loadRules(rulesFile()));
    QJsonObject st = cleanState();
    st.insert(key, QJsonValue::fromVariant(value));
    QString expectedSeverity;
    for (const QJsonValue& rule : jsonRules()) {
      if (rule.toObject().value(QStringLiteral("id")).toString() == ruleId)
        expectedSeverity = rule.toObject().value(QStringLiteral("severity")).toString();
    }
    QStringList failed;
    for (const CheckResult& r : engine.evaluate(st)) {
      if (r.passed) continue;
      failed << r.id;
      QCOMPARE(r.severity, expectedSeverity);
      QVERIFY2(!r.fixKo.isEmpty(), qPrintable(r.id));
    }
    QCOMPARE(failed, QStringList{ruleId});
  }

  void countMinReadsItsParameter() {
    QTemporaryDir dir;
    const QString path = writeRules(dir, {QJsonObject{{QStringLiteral("id"), QStringLiteral("THREE")},
                                                      {QStringLiteral("severity"), QStringLiteral("error")},
                                                      {QStringLiteral("check_type"), QStringLiteral("count_min:control_points:3")}}});
    ChecklistEngine engine;
    QVERIFY(engine.loadRules(path));
    QJsonObject st = cleanState();
    QVERIFY(!engine.evaluate(st).first().passed);  // 2 < 3
    st.insert(QStringLiteral("control_points_count"), 3);
    QVERIFY(engine.evaluate(st).first().passed);
  }

  void unknownCheckOrSeverityFailsClosed() {
    QTemporaryDir dir;
    const QString path = writeRules(dir, {QJsonObject{{QStringLiteral("id"), QStringLiteral("FUTURE")},
                                                      {QStringLiteral("severity"), QStringLiteral("fatal")},
                                                      {QStringLiteral("check_type"), QStringLiteral("no_such_check:x")}}});
    ChecklistEngine engine;
    QVERIFY(engine.loadRules(path));
    QCOMPARE(engine.unsupportedRuleIds(), QStringList{QStringLiteral("FUTURE")});
    const CheckResult r = engine.evaluate(cleanState()).first();
    QVERIFY(!r.passed);
    QCOMPARE(r.severity, QStringLiteral("error"));
    QVERIFY(!r.detailKo.isEmpty());
  }

  void failedLoadKeepsPreviousRules() {
    ChecklistEngine engine;
    QVERIFY(engine.loadRules(rulesFile()));
    const int count = engine.ruleCount();
    QVERIFY(!engine.loadRules(QDir::temp().filePath(QStringLiteral("ka-no-such-rules.json"))));
    QCOMPARE(engine.ruleCount(), count);
    QTemporaryDir dir;
    QVERIFY(!engine.loadRules(writeRules(dir, {})));  // an empty rule list is not a rule set
    QCOMPARE(engine.ruleCount(), count);
  }

  void offendersAndNotesReachTheResult() {
    ChecklistEngine engine;
    QVERIFY(engine.loadRules(rulesFile()));
    QJsonObject st = cleanState();
    st.insert(QStringLiteral("has_datum"), false);
    st.insert(QStringLiteral("project_crs_authid"), QStringLiteral("EPSG:4326"));
    QJsonObject offenders;
    offenders.insert(QStringLiteral("has_datum"),
                     QJsonArray{QJsonObject{{QStringLiteral("layer_id"), QStringLiteral("cp1")},
                                            {QStringLiteral("layer_name"), QStringLiteral("기준점")},
                                            {QStringLiteral("fid"), 7},
                                            {QStringLiteral("label"), QStringLiteral("G2")}}});
    st.insert(QStringLiteral("offenders"), offenders);
    st.insert(QStringLiteral("notes"), QJsonObject{{QStringLiteral("project_crs_authid"), QStringLiteral("지금 EPSG:4326")}});
    bool sawDatum = false, sawBelt = false;
    for (const CheckResult& r : engine.evaluate(st)) {
      if (r.id == QLatin1String("GCP_DATUM_META")) {
        sawDatum = true;
        QCOMPARE(r.targets.size(), 1);
        QCOMPARE(r.targets.first().featureId, qint64(7));
        QCOMPARE(r.targets.first().label, QStringLiteral("G2"));
        QCOMPARE(r.action, QStringLiteral("edit_attributes"));
      }
      if (r.id == QLatin1String("CRS_WORK_BELT")) {
        sawBelt = true;
        QVERIFY(!r.passed);
        QCOMPARE(r.detailKo, QStringLiteral("지금 EPSG:4326"));
      }
    }
    QVERIFY(sawDatum && sawBelt);
    const QString summary = ChecklistEngine::summaryText(engine.evaluate(st));
    QVERIFY(summary.contains(QStringLiteral("[error]")));
    QCOMPARE(ChecklistEngine::summaryText(engine.evaluate(cleanState())), QStringLiteral("OK\n"));
  }
};

QTEST_GUILESS_MAIN(TestChecklistRules)
#include "test_checklist_rules.moc"
