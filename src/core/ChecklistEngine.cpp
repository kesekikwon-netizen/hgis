#include "ChecklistEngine.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

ChecklistEngine::ChecklistEngine(QObject* parent) : QObject(parent) {}

bool ChecklistEngine::loadRules(const QString& jsonPath) {
  QFile f(jsonPath);
  if (!f.open(QIODevice::ReadOnly)) return false;
  const auto doc = QJsonDocument::fromJson(f.readAll());
  if (!doc.isObject()) return false;
  m_rules.clear();
  const auto arr = doc.object().value(QStringLiteral("rules")).toArray();
  for (const auto& v : arr) {
    const auto o = v.toObject();
    Rule r;
    r.id = o.value(QStringLiteral("id")).toString();
    r.severity = o.value(QStringLiteral("severity")).toString();
    r.messageKo = o.value(QStringLiteral("message_ko")).toString();
    r.checkType = o.value(QStringLiteral("check_type")).toString();
    m_rules.push_back(r);
  }
  return !m_rules.isEmpty();
}

bool ChecklistEngine::evalOne(const Rule& r, const QJsonObject& state) {
  const QString ct = r.checkType;
  if (ct.startsWith(QLatin1String("layer_nonempty:survey_area")))
    return state.value(QStringLiteral("survey_area_count")).toInt() > 0;
  if (ct.startsWith(QLatin1String("count_min:control_points:2")))
    return state.value(QStringLiteral("control_points_count")).toInt() >= 2;
  if (ct.startsWith(QLatin1String("field_nonempty:control_points:datum")))
    return state.value(QStringLiteral("has_datum")).toBool();
  if (ct.startsWith(QLatin1String("field_nonempty:control_points:ellipsoid")))
    return state.value(QStringLiteral("has_ellipsoid")).toBool();
  if (ct.startsWith(QLatin1String("field_nonempty:control_points:projection")))
    return state.value(QStringLiteral("has_projection")).toBool();
  if (ct.startsWith(QLatin1String("field_nonempty:control_points:origin")))
    return state.value(QStringLiteral("has_origin")).toBool(true);
  if (ct.startsWith(QLatin1String("field_nonempty:control_points:accuracy")))
    return state.value(QStringLiteral("has_accuracy")).toBool(true);
  if (ct.startsWith(QLatin1String("project_crs_set")))
    return state.value(QStringLiteral("project_crs_set")).toBool();
  if (ct.startsWith(QLatin1String("geometry_type:survey_area")))
    return state.value(QStringLiteral("survey_area_count")).toInt() == 0
        || state.value(QStringLiteral("survey_is_polygon")).toBool();
  if (ct.startsWith(QLatin1String("forbid_abstract_marker")))
    return !state.value(QStringLiteral("has_abstract_marker")).toBool();
  if (ct.startsWith(QLatin1String("field_any:feature_poly")))
    return state.value(QStringLiteral("has_kind_period")).toBool();
  if (ct.startsWith(QLatin1String("geometry_valid")))
    return state.value(QStringLiteral("geometries_valid")).toBool(true);
  if (ct.startsWith(QLatin1String("geometry_type:feature")))
    return state.value(QStringLiteral("geometries_valid")).toBool(true);
  if (ct.startsWith(QLatin1String("extent_within")))
    return state.value(QStringLiteral("features_within_survey")).toBool(true);
  if (ct.startsWith(QLatin1String("layout_exists")))
    return state.value(ct).toBool(false);
  if (ct.startsWith(QLatin1String("export_ready")))
    return state.value(QStringLiteral("survey_area_count")).toInt() > 0;
  return state.value(r.id).toBool(false);
}

QVector<CheckResult> ChecklistEngine::evaluate(const QJsonObject& projectState) const {
  QVector<CheckResult> out;
  for (const auto& r : m_rules) {
    CheckResult cr;
    cr.id = r.id;
    cr.severity = r.severity;
    cr.messageKo = r.messageKo;
    cr.passed = evalOne(r, projectState);
    out.push_back(cr);
  }
  return out;
}
