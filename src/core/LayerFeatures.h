#pragma once
// 「Has this layer any feature, and how many」 for decisions on the submit flow. A layer's cached
// featureCount() is not enough: QGIS can fail its own GeoPackage count ("unable to open database
// file"; which table fails differs per run, CI 2026-10-03) and the count goes stale when another
// layer object saves. When the count is not positive the features are read through the layer,
// which also honours unsaved edits (docs/ERROR_REGRESSION.md).
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgsvectorlayer.h>

namespace LayerFeatures {

inline QgsFeatureRequest idsOnly() {
  QgsFeatureRequest request;
  request.setNoAttributes();
  request.setFlags(Qgis::FeatureRequestFlag::NoGeometry);
  return request;
}

// One log line when the read finds what the count missed, so a log shows which path happened.
inline void noteMissedCount(const QgsVectorLayer* layer, long long cached) {
  qInfo("LayerFeatures: %s has features though its count says %lld", qUtf8Printable(layer->name()), cached);
}

inline bool any(QgsVectorLayer* layer) {
  if (!layer || !layer->isValid()) return false;
  const long long cached = layer->featureCount();
  if (cached > 0) return true;
  QgsFeature f;
  const bool found = layer->getFeatures(idsOnly().setLimit(1)).nextFeature(f);
  if (found) noteMissedCount(layer, cached);
  return found;
}

inline long long count(QgsVectorLayer* layer) {
  if (!layer || !layer->isValid()) return 0;
  const long long cached = layer->featureCount();
  if (cached > 0) return cached;
  QgsFeatureIterator it = layer->getFeatures(idsOnly());
  QgsFeature f;
  long long n = 0;
  while (it.nextFeature(f)) ++n;
  if (n > 0) noteMissedCount(layer, cached);
  return n;
}

}  // namespace LayerFeatures
