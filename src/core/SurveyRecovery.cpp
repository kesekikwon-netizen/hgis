#include "SurveyRecovery.h"

#include <QCryptographicHash>
#include <QList>

#include <algorithm>

#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayereditbuffer.h>

namespace {

void addText(QCryptographicHash& hash, const QString& text) {
  const QByteArray bytes = text.toUtf8();
  const qint64 size = bytes.size();
  hash.addData(QByteArrayView(reinterpret_cast<const char*>(&size), sizeof(size)));
  hash.addData(bytes);
}

void addNumber(QCryptographicHash& hash, qint64 value) {
  hash.addData(QByteArrayView(reinterpret_cast<const char*>(&value), sizeof(value)));
}

void addValue(QCryptographicHash& hash, const QVariant& value) {
  addNumber(hash, value.isNull() ? -1 : value.typeId());
  addText(hash, value.toString());
}

void addGeometry(QCryptographicHash& hash, const QgsGeometry& geometry) {
  const QByteArray wkb = geometry.isNull() ? QByteArray() : geometry.asWkb();
  addNumber(hash, wkb.size());
  hash.addData(wkb);
}

void addEditBuffer(QCryptographicHash& hash, const QgsVectorLayerEditBuffer* buffer) {
  if (!buffer) return;
  const QgsFeatureMap added = buffer->addedFeatures();
  addNumber(hash, added.size());
  for (auto it = added.cbegin(); it != added.cend(); ++it) {
    addNumber(hash, it.key());
    addGeometry(hash, it.value().geometry());
    const QgsAttributes attributes = it.value().attributes();
    for (const QVariant& value : attributes) addValue(hash, value);
  }
  QList<QgsFeatureId> deleted(buffer->deletedFeatureIds().cbegin(), buffer->deletedFeatureIds().cend());
  std::sort(deleted.begin(), deleted.end());
  addNumber(hash, deleted.size());
  for (QgsFeatureId id : deleted) addNumber(hash, id);
  const QgsChangedAttributesMap changed = buffer->changedAttributeValues();
  addNumber(hash, changed.size());
  for (auto it = changed.cbegin(); it != changed.cend(); ++it) {
    addNumber(hash, it.key());
    for (auto value = it.value().cbegin(); value != it.value().cend(); ++value) {
      addNumber(hash, value.key());
      addValue(hash, value.value());
    }
  }
  const QgsGeometryMap geometries = buffer->changedGeometries();
  addNumber(hash, geometries.size());
  for (auto it = geometries.cbegin(); it != geometries.cend(); ++it) {
    addNumber(hash, it.key());
    addGeometry(hash, it.value());
  }
  const QList<QgsField> fields = buffer->addedAttributes();
  addNumber(hash, fields.size());
  for (const QgsField& field : fields) {
    addText(hash, field.name());
    addNumber(hash, static_cast<qint64>(field.type()));
  }
  const QgsAttributeList dropped = buffer->deletedAttributeIds();
  addNumber(hash, dropped.size());
  for (int index : dropped) addNumber(hash, index);
}

}  // namespace

namespace SurveyRecovery {

QByteArray editSignature(QgsProject* project, const QStringList& layerIds) {
  if (!project) return {};
  QStringList ids = layerIds;
  ids.sort();
  QCryptographicHash hash(QCryptographicHash::Sha256);
  for (const QString& id : ids) {
    QgsMapLayer* layer = project->mapLayer(id);
    if (!layer) continue;
    addText(hash, id);
    addText(hash, layer->source());
    auto* vector = qobject_cast<QgsVectorLayer*>(layer);
    if (!vector) continue;
    addNumber(hash, vector->isValid() ? vector->featureCount() : -1);
    addNumber(hash, (vector->isEditable() ? 1 : 0) | (vector->isModified() ? 2 : 0));
    if (vector->isEditable()) addEditBuffer(hash, vector->editBuffer());
  }
  return hash.result();
}

bool snapshotNeeded(const QByteArray& lastSignature, const QByteArray& currentSignature) {
  return lastSignature.isEmpty() || currentSignature.isEmpty() || lastSignature != currentSignature;
}

}  // namespace SurveyRecovery
