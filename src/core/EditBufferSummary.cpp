#include "core/EditBufferSummary.h"

#include <QMap>
#include <QSet>
#include <QString>

#include <qgsmaplayer.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayereditbuffer.h>

namespace EditBufferSummary {

int uniqueEditedIds(const QgsVectorLayer* layer) {
  if (!layer) return 0;
  const QgsVectorLayerEditBuffer* buffer = layer->editBuffer();
  if (!buffer) return 0;
  // The maps are implicitly shared: these copies do not duplicate the features.
  QgsFeatureIds ids = buffer->deletedFeatureIds();
  const QgsFeatureMap added = buffer->addedFeatures();
  for (auto it = added.constBegin(); it != added.constEnd(); ++it) ids.insert(it.key());
  const QgsGeometryMap geometries = buffer->changedGeometries();
  for (auto it = geometries.constBegin(); it != geometries.constEnd(); ++it) ids.insert(it.key());
  const QgsChangedAttributesMap attributes = buffer->changedAttributeValues();
  for (auto it = attributes.constBegin(); it != attributes.constEnd(); ++it) ids.insert(it.key());
  return static_cast<int>(ids.size());
}

Summary summarize(const QgsProject* project) {
  Summary summary;
  if (!project) return summary;
  summary.projectDirty = project->isDirty();
  const QMap<QString, QgsMapLayer*> layers = project->mapLayers();
  for (auto it = layers.constBegin(); it != layers.constEnd(); ++it) {
    const auto* vector = qobject_cast<const QgsVectorLayer*>(it.value());
    if (!vector) continue;
    const int edited = uniqueEditedIds(vector);
    if (edited <= 0) continue;
    summary.features += edited;
    ++summary.layers;
  }
  return summary;
}

}  // namespace EditBufferSummary
