#include "ChecklistEngine.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>

namespace {
QString verbOf(const QString& checkType) { return checkType.section(QLatin1Char(':'), 0, 0); }
QString argOf(const QString& checkType, int index) {
  return checkType.section(QLatin1Char(':'), index, index);
}

// Default when a hand-made or older state lacks the key. Meta and layouts fail closed,
// geometry and extent flags keep the historic pass default.
bool defaultFor(const QString& key) {
  static const QSet<QString> failClosed = {
      QStringLiteral("has_datum"), QStringLiteral("has_ellipsoid"),
      QStringLiteral("has_projection"), QStringLiteral("project_crs_set")};
  if (key.startsWith(QLatin1String("layout_exists:"))) return false;
  return !failClosed.contains(key);
}

QVector<CheckTarget> targetsFrom(const QJsonObject& state, const QString& key) {
  QVector<CheckTarget> out;
  const QJsonArray arr = state.value(QStringLiteral("offenders")).toObject().value(key).toArray();
  for (const QJsonValue& v : arr) {
    const QJsonObject o = v.toObject();
    CheckTarget t;
    t.layerId = o.value(QStringLiteral("layer_id")).toString();
    t.layerName = o.value(QStringLiteral("layer_name")).toString();
    t.featureId = static_cast<qint64>(o.value(QStringLiteral("fid")).toDouble(-1));
    t.label = o.value(QStringLiteral("label")).toString();
    out.push_back(t);
  }
  return out;
}
}  // namespace

ChecklistEngine::ChecklistEngine(QObject* parent) : QObject(parent) {}

bool ChecklistEngine::loadRules(const QString& jsonPath) {
  QFile f(jsonPath);
  if (!f.open(QIODevice::ReadOnly)) return false;
  const auto doc = QJsonDocument::fromJson(f.readAll());
  if (!doc.isObject()) return false;
  const auto arr = doc.object().value(QStringLiteral("rules")).toArray();
  QVector<Rule> rules;
  for (const auto& v : arr) {
    const auto o = v.toObject();
    Rule r;
    r.id = o.value(QStringLiteral("id")).toString();
    r.severity = o.value(QStringLiteral("severity")).toString();
    // An unknown severity must not quietly stop blocking submission.
    if (r.severity != QLatin1String("warn")) r.severity = QStringLiteral("error");
    r.messageKo = o.value(QStringLiteral("message_ko")).toString();
    r.checkType = o.value(QStringLiteral("check_type")).toString();
    r.fixKo = o.value(QStringLiteral("fix_ko")).toString();
    r.basisKo = o.value(QStringLiteral("basis_ko")).toString();
    r.action = o.value(QStringLiteral("action")).toString();
    if (r.id.isEmpty()) continue;
    rules.push_back(r);
  }
  if (rules.isEmpty()) return false;
  m_rules = rules;
  m_path = jsonPath;
  return true;
}

QString ChecklistEngine::stateKeyFor(const QString& ct) {
  const QString verb = verbOf(ct);
  if (verb == QLatin1String("layer_nonempty") || verb == QLatin1String("count_min"))
    return argOf(ct, 1) + QStringLiteral("_count");
  if (verb == QLatin1String("field_nonempty")) return QStringLiteral("has_") + argOf(ct, 2);
  if (verb == QLatin1String("field_any"))
    return QStringLiteral("has_") + argOf(ct, 2).split(QLatin1Char(','), Qt::SkipEmptyParts)
                                        .join(QLatin1Char('_'));
  if (verb == QLatin1String("project_crs_set")) return QStringLiteral("project_crs_set");
  if (verb == QLatin1String("crs_in")) return QStringLiteral("project_crs_authid");
  if (verb == QLatin1String("geometry_type"))
    return argOf(ct, 1).startsWith(QLatin1String("survey_area")) ? QStringLiteral("survey_is_polygon")
                                                                 : QStringLiteral("features_real_geometry");
  if (verb == QLatin1String("forbid_abstract_marker")) return QStringLiteral("has_abstract_marker");
  if (verb == QLatin1String("geometry_valid")) return QStringLiteral("geometries_valid");
  if (verb == QLatin1String("geometry_nonempty")) return QStringLiteral("geometries_nonempty");
  if (verb == QLatin1String("geometry_nonzero_area")) return QStringLiteral("geometries_nonzero_area");
  if (verb == QLatin1String("extent_within")) return QStringLiteral("features_within_survey");
  if (verb == QLatin1String("layout_exists")) return ct;
  if (verb == QLatin1String("export_ready")) return QStringLiteral("export_crs_valid");
  if (verb == QLatin1String("sheet_covers")) return QStringLiteral("sheet_covers_targets");
  if (verb == QLatin1String("sheet_elements")) return QStringLiteral("sheet_has_elements");
  if (verb == QLatin1String("fields_required")) return QStringLiteral("required_fields_filled");
  if (verb == QLatin1String("unique_field"))
    return argOf(ct, 1) + QLatin1Char('_') + argOf(ct, 2) + QStringLiteral("_unique");
  // state:<key>: a plain boolean another module puts into the state (HERITAGE_PLEDGE_SCOPE).
  if (verb == QLatin1String("state")) return argOf(ct, 1);
  return {};
}

bool ChecklistEngine::isSupportedCheckType(const QString& checkType) {
  const QString verb = verbOf(checkType);
  if (verb == QLatin1String("count_min")) {
    bool ok = false;
    argOf(checkType, 2).toInt(&ok);
    return ok && !argOf(checkType, 1).isEmpty();
  }
  if (verb == QLatin1String("export_ready")) return argOf(checkType, 1) == QLatin1String("crs");
  return !stateKeyFor(checkType).isEmpty();
}

bool ChecklistEngine::evalOne(const Rule& r, const QJsonObject& state, bool* supported) {
  *supported = isSupportedCheckType(r.checkType);
  if (!*supported) return false;
  const QString ct = r.checkType;
  const QString verb = verbOf(ct);
  const QString key = stateKeyFor(ct);
  if (verb == QLatin1String("layer_nonempty")) return state.value(key).toInt() > 0;
  if (verb == QLatin1String("count_min")) return state.value(key).toInt() >= argOf(ct, 2).toInt();
  if (verb == QLatin1String("forbid_abstract_marker")) return !state.value(key).toBool(false);
  if (verb == QLatin1String("crs_in")) {
    const QString auth = state.value(key).toString();
    if (auth.isEmpty()) return true;  // CRS_PROJECT_SET reports a missing CRS.
    for (const QString& code : argOf(ct, 1).split(QLatin1Char(','), Qt::SkipEmptyParts)) {
      if (auth.compare(QStringLiteral("EPSG:") + code.trimmed(), Qt::CaseInsensitive) == 0) return true;
    }
    return false;
  }
  return state.value(key).toBool(defaultFor(key));
}

QVector<CheckResult> ChecklistEngine::evaluate(const QJsonObject& projectState) const {
  QVector<CheckResult> out;
  const QJsonObject notes = projectState.value(QStringLiteral("notes")).toObject();
  for (const auto& r : m_rules) {
    CheckResult cr;
    cr.id = r.id;
    cr.severity = r.severity;
    cr.messageKo = r.messageKo;
    cr.fixKo = r.fixKo;
    cr.basisKo = r.basisKo;
    cr.action = r.action;
    bool supported = true;
    cr.passed = evalOne(r, projectState, &supported);
    if (!supported) {
      cr.detailKo = QStringLiteral("이 프로그램이 모르는 검사(%1)라 통과로 볼 수 없습니다.").arg(r.checkType);
    } else if (!cr.passed) {
      const QString key = stateKeyFor(r.checkType);
      cr.targets = targetsFrom(projectState, key);
      cr.detailKo = notes.value(key).toString();
    }
    out.push_back(cr);
  }
  return out;
}

QStringList ChecklistEngine::unsupportedRuleIds() const {
  QStringList ids;
  for (const auto& r : m_rules) {
    if (!isSupportedCheckType(r.checkType)) ids << r.id;
  }
  return ids;
}

int ChecklistEngine::failedCount(const QVector<CheckResult>& results, const QString& severity) {
  int n = 0;
  for (const auto& r : results) {
    if (!r.passed && r.severity == severity) ++n;
  }
  return n;
}

QString ChecklistEngine::summaryText(const QVector<CheckResult>& results) {
  QString summary;
  for (const auto& r : results) {
    if (r.passed) continue;
    summary += QStringLiteral("- [%1] %2").arg(r.severity, r.messageKo);
    if (!r.detailKo.isEmpty()) summary += QStringLiteral(" (%1)").arg(r.detailKo);
    summary += QLatin1Char('\n');
  }
  return summary.isEmpty() ? QStringLiteral("OK\n") : summary;
}
