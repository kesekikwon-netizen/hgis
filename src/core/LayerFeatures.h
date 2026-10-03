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

inline bool any(QgsVectorLayer* layer) {
  if (!layer || !layer->isValid()) return false;
  if (layer->featureCount() > 0) return true;
  QgsFeature f;
  return layer->getFeatures(idsOnly().setLimit(1)).nextFeature(f);
}

inline long long count(QgsVectorLayer* layer) {
  if (!layer || !layer->isValid()) return 0;
  const long long cached = layer->featureCount();
  if (cached > 0) return cached;
  QgsFeatureIterator it = layer->getFeatures(idsOnly());
  QgsFeature f;
  long long n = 0;
  while (it.nextFeature(f)) ++n;
  return n;
}

}  // namespace LayerFeatures
