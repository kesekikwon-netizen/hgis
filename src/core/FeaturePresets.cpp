#include "FeaturePresets.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
struct BuiltInKind {
  const char* id;
  const char* label;
  const char* pattern;
  const char* line;
  const char* marker;
};
// Same values as data/styles/feature_presets.json (tests/test_presets.cpp compares them).
constexpr BuiltInKind kBuiltInKinds[] = {
    {"house", "주거지", "none", "solid", "square"},
    {"pit", "수혈", "bdiagonal", "0.6;1.2", "circle"},
    {"ditch", "구", "horizontal", "3;1.5", "diamond"},
    {"gully", "도랑", "vertical", "3;1.2;0.6;1.2", "cross_fill"},
    {"tomb", "분묘", "fdiagonal", "3;1.2;0.6;1.2;0.6;1.2", "triangle"},
    {"kiln", "가마", "cross", "7;1.5", "star"},
    {"artifact", "유물", "diagcross", "1.5;1.5", "pentagon"},
    {"other", "기타", "dots", "7;1.2;0.6;1.2", "hexagon"},
};

struct BuiltInPeriod {
  const char* id;
  const char* label;
  const char* color;
};
// Oldest first. Lightness rises monotonically (dark brown -> pale green), so periods stay
// apart in black-and-white print and under protan/deutan/tritan simulation, and the
// colours keep away from the survey default and heritage colours.
constexpr BuiltInPeriod kBuiltInPeriods[] = {
    {"paleolithic", "구석기", "#53291B"},    {"neolithic", "신석기", "#674028"},
    {"bronze", "청동기", "#7A5737"},         {"early_iron", "초기철기", "#8B7048"},
    {"proto_three", "원삼국", "#9C8A5B"},    {"three_kingdoms", "삼국", "#ABA571"},
    {"unified_silla", "통일신라", "#BAC08A"}, {"goryeo", "고려", "#C7DCA6"},
    {"joseon", "조선", "#D3F8C4"},           {"unknown", "미정", "none"},
};

QString validColor(const QString& text) {
  const QString t = text.trimmed();
  return QColor(t).isValid() ? t : QStringLiteral("none");
}
}  // namespace

FeaturePresets& FeaturePresets::instance() {
  static FeaturePresets s;
  return s;
}

bool FeaturePresets::load(const QString& jsonPath) {
  QFile f(jsonPath);
  if (!f.open(QIODevice::ReadOnly)) return false;
  const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
  if (!doc.isObject()) return false;
  const QJsonObject root = doc.object();
  QVector<Kind> kinds;
  QVector<Period> periods;
  for (const QJsonValue& v : root.value(QStringLiteral("kinds")).toArray()) {
    const QJsonObject o = v.toObject();
    Kind k;
    k.id = o.value(QStringLiteral("id")).toString();
    k.label = o.value(QStringLiteral("label")).toString().trimmed();
    k.pattern = o.value(QStringLiteral("pattern")).toString(QStringLiteral("none"));
    k.line = o.value(QStringLiteral("line")).toString(QStringLiteral("solid"));
    k.marker = o.value(QStringLiteral("marker")).toString(QStringLiteral("circle"));
    if (!k.label.isEmpty()) kinds.append(k);
  }
  for (const QJsonValue& v : root.value(QStringLiteral("periods")).toArray()) {
    const QJsonObject o = v.toObject();
    Period p;
    p.id = o.value(QStringLiteral("id")).toString();
    p.label = o.value(QStringLiteral("label")).toString().trimmed();
    p.color = validColor(o.value(QStringLiteral("color")).toString());
    if (!p.label.isEmpty()) periods.append(p);
  }
  if (kinds.isEmpty() || periods.isEmpty()) return false;
  m_kinds = kinds;
  m_periods = periods;
  return true;
}

QVector<FeaturePresets::Kind> FeaturePresets::builtInKinds() {
  QVector<Kind> out;
  for (const BuiltInKind& k : kBuiltInKinds)
    out.append({QString::fromUtf8(k.id), QString::fromUtf8(k.label), QString::fromUtf8(k.pattern),
                QString::fromUtf8(k.line), QString::fromUtf8(k.marker)});
  return out;
}

QVector<FeaturePresets::Period> FeaturePresets::builtInPeriods() {
  QVector<Period> out;
  for (const BuiltInPeriod& p : kBuiltInPeriods)
    out.append({QString::fromUtf8(p.id), QString::fromUtf8(p.label), QString::fromUtf8(p.color)});
  return out;
}

void FeaturePresets::loadBuiltIn() {
  m_kinds = builtInKinds();
  m_periods = builtInPeriods();
}

bool FeaturePresets::ensureLoaded() {
  if (isLoaded()) return true;
  if (!m_tried) {
    m_tried = true;
    const QDir app(QCoreApplication::applicationDirPath());
    const QString rel = QStringLiteral("data/styles/feature_presets.json");
    const QStringList candidates = {app.filePath(rel), app.filePath(QStringLiteral("../") + rel),
                                    app.filePath(QStringLiteral("../../") + rel), QDir::current().filePath(rel)};
    for (const QString& p : candidates) {
      if (QFile::exists(p) && load(p)) return true;
    }
  }
  loadBuiltIn();
  return isLoaded();
}

QStringList FeaturePresets::kindLabels() const {
  QStringList out;
  for (const Kind& k : m_kinds) out << k.label;
  return out;
}

QStringList FeaturePresets::periodLabels() const {
  QStringList out;
  for (const Period& p : m_periods) out << p.label;
  return out;
}

QString FeaturePresets::defaultKindLabel() const {
  for (const Kind& k : m_kinds) {
    if (k.id == QLatin1String("other")) return k.label;
  }
  return m_kinds.isEmpty() ? QStringLiteral("기타") : m_kinds.last().label;
}

QString FeaturePresets::kindKeyExpression() {
  return QStringLiteral("replace(trim(coalesce(to_string(\"kind\"),'')),' ','')");
}

QString FeaturePresets::periodKeyExpression() {
  return QStringLiteral("regexp_replace(replace(trim(coalesce(to_string(\"period\"),'')),' ',''),'시대$','')");
}

QString FeaturePresets::kindKey(const QString& text) {
  QString key = text.trimmed();
  key.remove(QLatin1Char(' '));
  return key;
}

QString FeaturePresets::periodKey(const QString& text) {
  QString key = kindKey(text);
  if (key.endsWith(QStringLiteral("시대"))) key.chop(2);
  return key;
}

const FeaturePresets::Kind* FeaturePresets::matchKind(const QString& text) const {
  const QString key = kindKey(text);
  if (key.isEmpty()) return nullptr;
  for (const Kind& k : m_kinds) {
    if (kindKey(k.label) == key || k.id.compare(key, Qt::CaseInsensitive) == 0) return &k;
  }
  return nullptr;
}

const FeaturePresets::Period* FeaturePresets::matchPeriod(const QString& text) const {
  const QString key = periodKey(text);
  if (key.isEmpty()) return nullptr;
  for (const Period& p : m_periods) {
    if (periodKey(p.label) == key || p.id.compare(key, Qt::CaseInsensitive) == 0) return &p;
  }
  return nullptr;
}

QColor FeaturePresets::periodColor(const QString& text) const {
  const Period* p = matchPeriod(text);
  return p && p->color != QLatin1String("none") ? QColor(p->color) : QColor();
}

int FeaturePresets::periodOrder(const QString& text) const {
  const Period* p = matchPeriod(text);
  return p ? static_cast<int>(p - m_periods.constData()) : static_cast<int>(m_periods.size());
}
