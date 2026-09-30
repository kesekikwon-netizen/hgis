#pragma once

#include <QString>
#include <QStringList>

#include <limits>

#include <qgsfeatureid.h>

class QgsFeature;
class QgsVectorLayer;

// Helpers for one feature record (유구·유물 기록) after it is drawn: value choices,
// spelling, audit stamps and measures. Nothing here opens a dialog or commits:
// every change goes into the layer's edit buffer and Ctrl+S stays the only save.
namespace FeatureRecord {

// Distinct non-empty values of a text field, most used first (ties: alphabetical).
QStringList distinctValues(const QgsVectorLayer* layer, const QString& field, int limit = 200);

// Choices for the kind / period combos: the preset labels (periods oldest first, each
// written the way the survey already writes it), then the layer's own values that match
// no preset. Free text stays allowed. Kind presets (유구 종류) are not offered on an
// artifact layer (artifact_no): it lists its own values only.
QStringList choices(const QgsVectorLayer* layer, const QString& field);

// What to store for typed text: the spelling this layer already uses for the same key
// ("청동기 시대" -> the layer's "청동기시대"), else a preset label that differs only by
// spaces ("주거 지" -> 주거지), else the text with its spaces tidied. Never empties text.
QString canonicalValue(const QgsVectorLayer* layer, const QString& field, const QString& text);

// Local time with offset, seconds precision ("2026-09-29T10:15:30+09:00").
QString nowText();
// Fills uid (UUID), created_at and updated_at when the feature has those fields and
// they are empty. For a feature about to be added (before addFeature). True if set.
bool stampNew(QgsFeature& feature);
// Sets updated_at of an existing feature in the edit buffer. Call inside the same
// edit command as the change. False when the layer has no updated_at field.
bool touch(QgsVectorLayer* layer, QgsFeatureId fid);

// Planar measures in layer units (metres for the 5186/5187 work CRS). NaN when the
// geometry type does not have the measure or the layer CRS is geographic.
struct Measures {
  double area = std::numeric_limits<double>::quiet_NaN();
  double perimeter = std::numeric_limits<double>::quiet_NaN();  // polygon outline
  double length = std::numeric_limits<double>::quiet_NaN();     // line
};
Measures measure(const QgsVectorLayer* layer, const QgsFeature& feature);

}  // namespace FeatureRecord
