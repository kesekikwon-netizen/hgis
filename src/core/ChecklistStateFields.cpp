#include "ChecklistStateChecks.h"
#include "LayerOps.h"

#include <QHash>

#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

namespace ChecklistStateChecks {
namespace {
QString text(const QgsFeature& f, const QString& field) {
  const int index = f.fields().lookupField(field);
  return index < 0 ? QString() : f.attribute(index).toString().trimmed();
}

bool hasField(const QgsVectorLayer* layer, const QString& field) {
  return layer->fields().lookupField(field) >= 0;
}

// Required text fields from docs/domain/data-model.md. A layer without the field
// (an imported shapefile) is not reported: the checklist cannot invent a schema.
struct Required {
  const char* key;
  const char* field;
};
constexpr Required kRequired[] = {{"survey_area", "survey_name"},
                                  {"control_points", "point_id"},
                                  {"section_line", "section_id"},
                                  {"feature_line", "kind"}};
}  // namespace

void scanFields(QgsProject* project, QJsonObject& st, Offenders& off) {
  // Every feature needs kind and period; one good feature must not hide the others.
  bool kindPeriod = true;
  for (QgsVectorLayer* layer : LayerOps::domainLayersForKey(project, QStringLiteral("feature_poly"))) {
    if (layer->featureCount() <= 0) continue;
    QgsFeatureIterator it = layer->getFeatures();
    QgsFeature f;
    while (it.nextFeature(f)) {
      if (text(f, QStringLiteral("kind")).isEmpty() || text(f, QStringLiteral("period")).isEmpty()) {
        kindPeriod = false;
        off.add(QStringLiteral("has_kind_period"), layer, f.id(), featureLabel(f));
      }
    }
  }
  st.insert(QStringLiteral("has_kind_period"), kindPeriod);

  // Control-point meta is checked on every point, not "any point has it".
  int points = 0;
  bool datum = true, ellipsoid = true, projection = true, origin = true, accuracy = true;
  bool uniqueIds = true;
  QHash<QString, int> seenIds;
  for (QgsVectorLayer* layer : LayerOps::domainLayersForKey(project, QStringLiteral("control_points"))) {
    if (layer->featureCount() <= 0) continue;
    QgsFeatureIterator it = layer->getFeatures();
    QgsFeature f;
    while (it.nextFeature(f)) {
      ++points;
      const QString label = featureLabel(f);
      const auto need = [&](const char* field, const char* key, bool& flag) {
        if (!text(f, QString::fromLatin1(field)).isEmpty()) return;
        flag = false;
        off.add(QString::fromLatin1(key), layer, f.id(), label);
      };
      need("datum", "has_datum", datum);
      need("ellipsoid", "has_ellipsoid", ellipsoid);
      need("projection", "has_projection", projection);
      need("origin", "has_origin", origin);
      if (text(f, QStringLiteral("accuracy_m")).isEmpty() && text(f, QStringLiteral("accuracy")).isEmpty()) {
        accuracy = false;
        off.add(QStringLiteral("has_accuracy"), layer, f.id(), label);
      }
      const QString id = text(f, QStringLiteral("point_id"));
      if (!id.isEmpty() && ++seenIds[id] > 1) {
        uniqueIds = false;
        off.add(QStringLiteral("control_points_point_id_unique"), layer, f.id(), id);
      }
    }
  }
  // No points: the meta rules fail with GCP_MIN_TWO; origin and accuracy stay quiet.
  st.insert(QStringLiteral("has_datum"), points > 0 && datum);
  st.insert(QStringLiteral("has_ellipsoid"), points > 0 && ellipsoid);
  st.insert(QStringLiteral("has_projection"), points > 0 && projection);
  st.insert(QStringLiteral("has_origin"), origin);
  st.insert(QStringLiteral("has_accuracy"), accuracy);
  st.insert(QStringLiteral("control_points_point_id_unique"), uniqueIds);

  bool required = true;
  QStringList missing;
  for (const Required& r : kRequired) {
    const QString field = QString::fromLatin1(r.field);
    for (QgsVectorLayer* layer : LayerOps::domainLayersForKey(project, QString::fromLatin1(r.key))) {
      if (layer->featureCount() <= 0 || !hasField(layer, field)) continue;
      QgsFeatureIterator it = layer->getFeatures();
      QgsFeature f;
      bool layerMissing = false;
      while (it.nextFeature(f)) {
        if (!text(f, field).isEmpty()) continue;
        required = false;
        layerMissing = true;
        off.add(QStringLiteral("required_fields_filled"), layer, f.id(),
                QStringLiteral("%1 · %2 없음").arg(featureLabel(f), field));
      }
      if (layerMissing && !missing.contains(field)) missing << field;
    }
  }
  st.insert(QStringLiteral("required_fields_filled"), required);
  if (!missing.isEmpty())
    off.note(QStringLiteral("required_fields_filled"), QStringLiteral("빈 항목: %1").arg(missing.join(QStringLiteral(", "))));
}

}  // namespace ChecklistStateChecks
