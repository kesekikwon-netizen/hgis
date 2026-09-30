#include "FeatureRecord.h"

#include "FeaturePresets.h"

#include <QDateTime>
#include <QHash>
#include <QTimeZone>
#include <QUuid>
#include <algorithm>

#include <qgis.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgsfields.h>
#include <qgsgeometry.h>
#include <qgsvectorlayer.h>

namespace FeatureRecord {
namespace {
QString keyOf(const QString& field, const QString& text) {
  return field == QLatin1String("period") ? FeaturePresets::periodKey(text) : FeaturePresets::kindKey(text);
}

bool isEmptyValue(const QVariant& value) { return value.isNull() || value.toString().trimmed().isEmpty(); }
}  // namespace

QStringList distinctValues(const QgsVectorLayer* layer, const QString& field, int limit) {
  if (!layer || !layer->isValid()) return {};
  const int index = layer->fields().lookupField(field);
  if (index < 0) return {};
  QHash<QString, int> counts;
  QgsFeatureRequest request;
  request.setFlags(Qgis::FeatureRequestFlag::NoGeometry);
  request.setSubsetOfAttributes(QgsAttributeList{index});
  QgsFeatureIterator it = layer->getFeatures(request);
  QgsFeature f;
  while (it.nextFeature(f)) {
    const QVariant v = f.attribute(index);
    if (isEmptyValue(v)) continue;
    ++counts[v.toString().trimmed()];
  }
  QStringList values = counts.keys();
  std::sort(values.begin(), values.end(), [&counts](const QString& a, const QString& b) {
    const int ca = counts.value(a);
    const int cb = counts.value(b);
    return ca != cb ? ca > cb : a < b;
  });
  if (limit > 0 && values.size() > limit) values = values.mid(0, limit);
  return values;
}

QStringList choices(const QgsVectorLayer* layer, const QString& field) {
  FeaturePresets& presets = FeaturePresets::instance();
  presets.ensureLoaded();
  // The kind presets are 유구 kinds; an artifact layer (artifact_no) offers its own values.
  const bool artifacts = layer && layer->fields().lookupField(QStringLiteral("artifact_no")) >= 0;
  const QStringList labels = field == QLatin1String("kind")     ? (artifacts ? QStringList() : presets.kindLabels())
                             : field == QLatin1String("period") ? presets.periodLabels()
                                                                 : QStringList();
  const QStringList existing = distinctValues(layer, field);
  QStringList out;
  QStringList keys;
  const auto add = [&](const QString& value) {
    const QString key = keyOf(field, value);
    if (keys.contains(key)) return;  // one entry per key: the survey's own spelling wins
    keys << key;
    out << value;
  };
  // Preset order (periods oldest first), each written the way this survey already writes it.
  for (const QString& label : labels) {
    const QString key = keyOf(field, label);
    const auto used = std::find_if(existing.cbegin(), existing.cend(),
                                   [&](const QString& v) { return keyOf(field, v) == key; });
    add(used != existing.cend() ? *used : label);
  }
  for (const QString& value : existing) add(value);
  return out;
}

QString canonicalValue(const QgsVectorLayer* layer, const QString& field, const QString& text) {
  const QString tidy = text.simplified();
  const QString key = keyOf(field, tidy);
  if (key.isEmpty()) return tidy;
  for (const QString& value : distinctValues(layer, field)) {
    if (keyOf(field, value) == key) return value;
  }
  FeaturePresets& presets = FeaturePresets::instance();
  presets.ensureLoaded();
  const QStringList labels = field == QLatin1String("kind")     ? presets.kindLabels()
                             : field == QLatin1String("period") ? presets.periodLabels()
                                                                 : QStringList();
  for (const QString& label : labels) {
    if (FeaturePresets::kindKey(label) == FeaturePresets::kindKey(tidy)) return label;
  }
  return tidy;
}

QString nowText() {
  const QDateTime now = QDateTime::currentDateTime();
  return now.toTimeZone(QTimeZone::fromSecondsAheadOfUtc(now.offsetFromUtc())).toString(Qt::ISODate);
}

bool stampNew(QgsFeature& feature) {
  const QgsFields fields = feature.fields();
  bool changed = false;
  const auto fill = [&](const char* name, const QString& value) {
    const int index = fields.lookupField(QString::fromLatin1(name));
    if (index < 0 || !isEmptyValue(feature.attribute(index))) return;
    feature.setAttribute(index, value);
    changed = true;
  };
  const QString now = nowText();
  fill("uid", QUuid::createUuid().toString(QUuid::WithoutBraces));
  fill("created_at", now);
  fill("updated_at", now);
  return changed;
}

bool touch(QgsVectorLayer* layer, QgsFeatureId fid) {
  if (!layer || !layer->isEditable()) return false;
  const int index = layer->fields().lookupField(QStringLiteral("updated_at"));
  if (index < 0) return false;
  return layer->changeAttributeValue(fid, index, nowText());
}

Measures measure(const QgsVectorLayer* layer, const QgsFeature& feature) {
  Measures out;
  if (!layer || !feature.hasGeometry() || layer->crs().isGeographic()) return out;
  const QgsGeometry geometry = feature.geometry();
  if (geometry.type() == Qgis::GeometryType::Polygon) {
    out.area = geometry.area();
    out.perimeter = geometry.length();  // QGIS: the outline length for polygons
  } else if (geometry.type() == Qgis::GeometryType::Line) {
    out.length = geometry.length();
  }
  return out;
}

}  // namespace FeatureRecord
