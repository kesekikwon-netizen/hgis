#include "GeometryEditOps.h"
#include "LayerOps.h"

#include <QHash>
#include <QVector>

#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgsgeometry.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorlayer.h>

#include <algorithm>

namespace {
QgsAttributeList primaryKeys(const QgsVectorLayer* layer) {
  const QgsVectorDataProvider* provider = layer ? layer->dataProvider() : nullptr;
  return provider ? provider->pkAttributeIndexes() : QgsAttributeList();
}

QString cellText(const QVariant& value) {
  return value.isNull() ? QString() : value.toString().trimmed();
}

// Drawing order: saved features by id, then unsaved ones (QGIS numbers them -1, -2, …).
bool drawnBefore(QgsFeatureId a, QgsFeatureId b) {
  if ((a >= 0) != (b >= 0)) return a >= 0;
  return a >= 0 ? a < b : a > b;
}

QList<QgsFeatureId> inDrawOrder(const QgsFeatureIds& ids) {
  QList<QgsFeatureId> order(ids.cbegin(), ids.cend());
  std::sort(order.begin(), order.end(), drawnBefore);
  return order;
}
}  // namespace

QgsFeatureId GeometryEditOps::mergeKeeperId(const QgsFeatureIds& ids) {
  if (ids.isEmpty()) return FID_NULL;
  return inDrawOrder(ids).first();
}

QList<GeometryEditOps::MergeConflict> GeometryEditOps::mergeConflicts(const QgsVectorLayer* layer,
                                                                       const QgsFeatureIds& ids) {
  QList<MergeConflict> out;
  if (!layer || !layer->isValid() || ids.size() < 2) return out;
  QHash<QgsFeatureId, QgsAttributes> byId;
  QgsFeatureRequest request;
  request.setFilterFids(ids);
  request.setFlags(Qgis::FeatureRequestFlag::NoGeometry);
  QgsFeature feature;
  QgsFeatureIterator it = layer->getFeatures(request);
  while (it.nextFeature(feature)) byId.insert(feature.id(), feature.attributes());
  QList<QgsAttributes> rows;  // drawing order: the keeper comes first
  for (QgsFeatureId id : inDrawOrder(ids))
    if (byId.contains(id)) rows.append(byId.value(id));
  if (rows.size() < 2) return out;
  const QgsFields fields = layer->fields();
  const QgsAttributeList keys = primaryKeys(layer);
  for (int i = 0; i < fields.count(); ++i) {
    // Keys are reassigned on save; the area label field is derived from the geometry.
    if (keys.contains(i) || fields.at(i).name() == QLatin1String("area_m2")) continue;
    const QString kept = cellText(rows.first().value(i));
    QStringList values;
    bool lost = false;
    for (const QgsAttributes& row : std::as_const(rows)) {
      const QString value = cellText(row.value(i));
      if (value.isEmpty()) continue;
      if (value != kept) lost = true;
      if (!values.contains(value)) values.append(value);
    }
    if (!lost) continue;
    const QgsField& field = fields.at(i);
    out.append({field.alias().isEmpty() ? field.name() : field.alias(), values});
  }
  return out;
}

QString GeometryEditOps::mergeConflictSummary(const QList<MergeConflict>& conflicts, int maxFields) {
  if (conflicts.isEmpty()) return {};
  QStringList lines;
  lines << QStringLiteral("고른 도형의 속성이 서로 다릅니다. 가장 먼저 그린 도형의 값만 남습니다.");
  const int shown = qMin(qMax(1, maxFields), int(conflicts.size()));
  for (int i = 0; i < shown; ++i) {
    QStringList values = conflicts.at(i).values;
    if (values.size() > 4) {
      values = values.mid(0, 4);
      values << QStringLiteral("…");
    }
    lines << QStringLiteral("· %1: %2").arg(conflicts.at(i).field, values.join(QStringLiteral(", ")));
  }
  if (conflicts.size() > shown)
    lines << QStringLiteral("· 그 밖에 %1개 항목").arg(conflicts.size() - shown);
  lines << QString() << QStringLiteral("묶은 뒤에도 Ctrl+Z로 되돌릴 수 있습니다.");
  return lines.join(QLatin1Char('\n'));
}

bool LayerOps::mergePolygonFeatures(QgsVectorLayer* layer, const QgsFeatureIds& featureIds, QString* errorOut) {
  const auto fail = [errorOut](const QString& text) {
    if (errorOut) *errorOut = text;
    return false;
  };
  if (!layer || !layer->isValid()) return fail(QStringLiteral("올바른 레이어가 아닙니다."));
  if (layer->geometryType() != Qgis::GeometryType::Polygon)
    return fail(QStringLiteral("폴리곤 레이어만 묶을 수 있습니다"));
  // An empty id set never means "every polygon": that once merged a whole layer.
  if (featureIds.size() < 2)
    return fail(QStringLiteral("묶을 폴리곤을 2개 이상 고르세요 (선택: %1개)").arg(featureIds.size()));

  QHash<QgsFeatureId, QgsFeature> picked;
  QVector<QgsGeometry> geoms;
  QgsFeature feature;
  QgsFeatureIterator it = layer->getFeatures(QgsFeatureRequest().setFilterFids(featureIds));
  while (it.nextFeature(feature)) {
    if (!feature.hasGeometry() || feature.geometry().isEmpty()) continue;
    QgsGeometry geometry = feature.geometry();
    if (!geometry.isGeosValid()) geometry = geometry.makeValid();
    if (geometry.isEmpty()) continue;
    geoms.append(geometry);
    picked.insert(feature.id(), feature);
  }
  if (geoms.size() < 2)
    return fail(QStringLiteral("묶을 폴리곤이 2개 이상 필요합니다 (선택: %1개)").arg(geoms.size()));

  QgsGeometry merged = QgsGeometry::unaryUnion(geoms);
  if (merged.isEmpty() || !merged.isGeosValid()) merged = QgsGeometry::collectGeometry(geoms);
  if (merged.isEmpty()) return fail(QStringLiteral("폴리곤 결합 실패"));
  if (!merged.isGeosValid()) merged = merged.makeValid();
  // 한 조각 표(조사구역·유구면)에 여러 조각 도형을 넣으면 저장 때 면적 0인 고장 난 도형이 된다.
  if (!QgsWkbTypes::isMultiType(layer->wkbType()) && merged.isMultipart()) {
    if (merged.constGet()->partCount() > 1)
      return fail(QStringLiteral("떨어진 구역은 하나로 묶을 수 없습니다. 두 구역은 따로 저장되고 제출에도 함께 들어갑니다. "
                                 "맞닿거나 겹친 구역만 하나로 묶습니다."));
    merged.convertToSingleType();
  }

  // The polygon drawn first keeps its record; keys are left for the provider.
  QgsFeatureIds ids;
  for (auto pickedIt = picked.cbegin(); pickedIt != picked.cend(); ++pickedIt) ids.insert(pickedIt.key());
  QgsAttributes attributes = picked.value(GeometryEditOps::mergeKeeperId(ids)).attributes();
  for (int key : primaryKeys(layer)) {
    if (key >= 0 && key < attributes.size()) attributes[key] = QVariant();
  }
  QgsFeature out(layer->fields());
  out.setAttributes(attributes);
  out.setGeometry(merged);
  // One undo command in the edit buffer (Ctrl+Z); the survey file is written on save.
  return runEditCommand(layer, QStringLiteral("폴리곤 묶기"), [&]() {
    return layer->deleteFeatures(ids) && layer->addFeature(out);
  }, errorOut);
}
