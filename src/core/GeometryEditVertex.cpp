#include "LayerOps.h"

#include <QHash>
#include <QList>

#include <qgsabstractgeometry.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgsgeometry.h>
#include <qgspoint.h>
#include <qgspointxy.h>
#include <qgsproject.h>
#include <qgsrectangle.h>
#include <qgsvectorlayer.h>

namespace {
// Shared vertices within this distance (map units, 1 mm in the metric work CRS) move together.
constexpr double kSharedVertexTolerance = 0.001;

bool refuseReadOnlyKinds(QgsVectorLayer* layer, QString* errorOut) {
  if (LayerOps::isCadastralLayer(layer)) {
    if (errorOut) *errorOut = QStringLiteral("지적도는 고칠 수 없습니다.");
    return true;
  }
  if (LayerOps::isReferenceLayer(layer)) {
    if (errorOut) *errorOut = QStringLiteral("참조 지도는 고칠 수 없습니다.");
    return true;
  }
  return false;
}
}  // namespace

bool LayerOps::moveFeatureVertex(QgsVectorLayer* layer, qint64 featureId, int vertex,
                                 double x, double y, QString* errorOut) {
  if (!layer || !layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("레이어가 없습니다.");
    return false;
  }
  if (refuseReadOnlyKinds(layer, errorOut)) return false;
  if (vertex < 0) {
    if (errorOut) *errorOut = QStringLiteral("꼭짓점이 없습니다.");
    return false;
  }
  const QgsFeatureId fid = static_cast<QgsFeatureId>(featureId);
  QgsFeature existing = layer->getFeature(fid);
  if (!existing.isValid() || !existing.hasGeometry()) {
    if (errorOut) *errorOut = QStringLiteral("고칠 도형을 찾지 못했습니다.");
    return false;
  }
  const bool startedHere = !layer->isEditable();
  if (startedHere && !layer->startEditing()) {
    if (errorOut) *errorOut = QStringLiteral("편집을 열 수 없습니다.");
    return false;
  }
  const bool topological = layer->project() && layer->project()->topologicalEditing();
  if (!applyVertexMove(layer, featureId, vertex, x, y, topological, errorOut)) {
    if (startedHere) layer->rollBack();
    return false;
  }
  return true;
}

bool LayerOps::applyVertexMove(QgsVectorLayer* layer, qint64 featureId, int vertex,
                               double x, double y, bool topological, QString* errorOut) {
  if (!layer || !layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("레이어가 없습니다.");
    return false;
  }
  if (refuseReadOnlyKinds(layer, errorOut)) return false;
  if (vertex < 0) {
    if (errorOut) *errorOut = QStringLiteral("꼭짓점이 없습니다.");
    return false;
  }
  if (!layer->isEditable()) {
    if (errorOut) *errorOut = QStringLiteral("편집을 열 수 없습니다.");
    return false;
  }
  const QgsFeatureId fid = static_cast<QgsFeatureId>(featureId);
  QgsFeature existing = layer->getFeature(fid);
  if (!existing.isValid() || !existing.hasGeometry()) {
    if (errorOut) *errorOut = QStringLiteral("고칠 도형을 찾지 못했습니다.");
    return false;
  }
  const QgsPoint origin = existing.geometry().vertexAt(vertex);
  const QgsPointXY from(origin.x(), origin.y());
  const double tol2 = kSharedVertexTolerance * kSharedVertexTolerance;

  struct Hit {
    QgsFeatureId id = FID_NULL;
    QgsGeometry geom;
    QList<int> verts;
  };
  QHash<QgsFeatureId, Hit> hits;
  const auto addHit = [&](const QgsFeature& feature, int vi) {
    Hit& hit = hits[feature.id()];
    hit.id = feature.id();
    if (hit.geom.isNull()) hit.geom = feature.geometry();
    if (!hit.verts.contains(vi)) hit.verts.append(vi);
  };

  if (topological) {
    // Only features whose bounding box touches the vertex can share it. The
    // spatial filter keeps each drag step from walking every feature of the layer.
    const QgsRectangle near(from.x() - kSharedVertexTolerance, from.y() - kSharedVertexTolerance,
                            from.x() + kSharedVertexTolerance, from.y() + kSharedVertexTolerance);
    QgsFeatureRequest request;
    request.setFilterRect(near);
    request.setNoAttributes();
    QgsFeature feature;
    QgsFeatureIterator it = layer->getFeatures(request);
    while (it.nextFeature(feature)) {
      if (!feature.hasGeometry()) continue;
      const QgsGeometry geom = feature.geometry();
      const int count = geom.constGet() ? static_cast<int>(geom.constGet()->nCoordinates()) : 0;
      for (int i = 0; i < count; ++i) {
        const QgsPoint pt = geom.vertexAt(i);
        if (QgsPointXY(pt.x(), pt.y()).sqrDist(from) <= tol2) addHit(feature, i);
      }
    }
  } else {
    addHit(existing, vertex);
    const QgsGeometry geom = existing.geometry();
    const int count = geom.constGet() ? static_cast<int>(geom.constGet()->nCoordinates()) : 0;
    if (geom.type() == Qgis::GeometryType::Polygon && count > 1) {
      if (vertex == 0) addHit(existing, count - 1);
      if (vertex == count - 1) addHit(existing, 0);
    }
  }
  if (!hits.contains(fid)) addHit(existing, vertex);

  for (auto it = hits.begin(); it != hits.end(); ++it) {
    Hit& hit = it.value();
    for (int vi : hit.verts) {
      if (!hit.geom.moveVertex(x, y, vi)) {
        if (errorOut) *errorOut = QStringLiteral("꼭짓점을 옮기지 못했습니다.");
        return false;
      }
    }
    if (!layer->changeGeometry(hit.id, hit.geom)) {
      if (errorOut) *errorOut = QStringLiteral("도형을 고치지 못했습니다.");
      return false;
    }
  }
  layer->updateExtents();
  layer->triggerRepaint();
  return true;
}
