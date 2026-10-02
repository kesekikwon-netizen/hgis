#include "FeaturePick.h"

#include "KaLogExcept.h"
#include "LayerOps.h"

#include <algorithm>
#include <cmath>
#include <tuple>

#include <QSet>

#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransform.h>
#include <qgscoordinatetransformcontext.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgsgeometry.h>
#include <qgspointxy.h>
#include <qgsrectangle.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayereditbuffer.h>

namespace FeaturePick {
namespace {

// 작을수록 먼저: 레이어 종류(조사 0, 참조·지적 1), 도형 종류(점·선 0, 품는 면·조각 1, 가까운 면 2),
// 크기(허용 오차 대비 거리 또는 면적), 레이어 순서.
using Rank = std::tuple<int, int, double, int>;

struct Ranked {
  Hit hit;
  Rank rank;
};

// A filter-rect request skips a feature whose geometry was changed in the edit buffer unless
// that geometry itself crosses the rectangle (saved features match by bounding box). A click
// inside a hole cut before 저장 would then find nothing. Those features are fetched here.
QgsFeatureIds missedBufferedFeatures(QgsVectorLayer* layer, const QgsRectangle& box, const QSet<QgsFeatureId>& seen) {
  QgsFeatureIds missed;
  const QgsVectorLayerEditBuffer* buffer = layer->editBuffer();
  if (!buffer) return missed;
  const QgsGeometryMap& changed = buffer->changedGeometries();
  for (auto it = changed.constBegin(); it != changed.constEnd(); ++it) {
    if (!seen.contains(it.key()) && !it.value().isNull() && it.value().boundingBox().intersects(box))
      missed.insert(it.key());
  }
  const QgsFeatureMap& added = buffer->addedFeatures();
  for (auto it = added.constBegin(); it != added.constEnd(); ++it) {
    if (!seen.contains(it.key()) && it->hasGeometry() && it->geometry().boundingBox().intersects(box))
      missed.insert(it.key());
  }
  return missed;
}

}  // namespace

bool isSurveyLayer(const QgsVectorLayer* layer) {
  if (!layer || !layer->isValid()) return false;
  return !LayerOps::isReferenceOrBasemapLayer(layer) && !LayerOps::isCadastralLayer(layer) &&
         !LayerOps::isReferenceLayer(layer);
}

QList<Hit> candidates(const QList<QgsMapLayer*>& layers, const QgsPointXY& mapPoint, double mapTolerance,
                      const QgsCoordinateReferenceSystem& mapCrs, const QgsCoordinateTransformContext& context) {
  QList<Ranked> ranked;
  QSet<const QgsMapLayer*> visited;
  int order = -1;
  for (QgsMapLayer* mapLayer : layers) {
    ++order;
    auto* layer = qobject_cast<QgsVectorLayer*>(mapLayer);
    if (!layer || !layer->isValid() || visited.contains(layer)) continue;
    visited.insert(layer);

    QgsPointXY point = mapPoint;
    double tolerance = mapTolerance;
    if (mapCrs.isValid() && layer->crs().isValid() && mapCrs != layer->crs()) {
      QgsCoordinateTransform toLayer(mapCrs, layer->crs(), context);
      toLayer.setBallparkTransformsAreAppropriate(true);
      try {
        point = toLayer.transform(mapPoint);
        const QgsPointXY edge = toLayer.transform(QgsPointXY(mapPoint.x() + mapTolerance, mapPoint.y()));
        tolerance = std::hypot(edge.x() - point.x(), edge.y() - point.y());
      } catch (...) {
        KA_LOG_EXCEPT();
        continue;
      }
    }
    if (!(tolerance > 0.0)) continue;

    const bool survey = isSurveyLayer(layer);
    const int tier = survey ? 0 : 1;
    const bool polygons = layer->geometryType() == Qgis::GeometryType::Polygon;
    const QgsGeometry probe = QgsGeometry::fromPointXY(point);
    const QgsRectangle box(point.x() - tolerance, point.y() - tolerance, point.x() + tolerance,
                           point.y() + tolerance);
    const double area = tolerance * tolerance;

    const auto consider = [&](const QgsFeature& feature) {
      if (!feature.hasGeometry()) return;
      const QgsGeometry geometry = feature.geometry();
      if (geometry.isEmpty()) return;
      if (polygons && survey) {
        // Inside a hole nothing of the shape is under the click, yet the hole is a pick of its own.
        if (const auto piece = PolygonPieces::innerPieceAt(geometry, point, tolerance)) {
          const QgsGeometry outline = PolygonPieces::outline(geometry, *piece);
          if (!outline.isNull()) {
            const Rank rank = outline.contains(probe)
                                  ? Rank(tier, 1, std::abs(outline.area()) / area, order)
                                  : Rank(tier, 2, std::max(0.0, outline.distance(probe)) / tolerance, order);
            ranked.append({Hit{layer, feature.id(), *piece}, rank});
          }
        }
      }
      if (polygons && geometry.contains(probe)) {
        ranked.append({Hit{layer, feature.id(), {}}, Rank(tier, 1, std::abs(geometry.area()) / area, order)});
        return;
      }
      const double distance = geometry.distance(probe);
      if (distance < 0.0 || distance > tolerance) return;  // negative: GEOS gave up
      ranked.append({Hit{layer, feature.id(), {}}, Rank(tier, polygons ? 2 : 0, distance / tolerance, order)});
    };

    QSet<QgsFeatureId> seen;
    QgsFeatureRequest request;
    request.setFilterRect(box);
    QgsFeatureIterator it = layer->getFeatures(request);
    QgsFeature feature;
    while (it.nextFeature(feature)) {
      seen.insert(feature.id());
      consider(feature);
    }
    const QgsFeatureIds missed = missedBufferedFeatures(layer, box, seen);
    if (!missed.isEmpty()) {
      QgsFeatureIterator extra = layer->getFeatures(QgsFeatureRequest().setFilterFids(missed));
      while (extra.nextFeature(feature)) consider(feature);
    }
  }

  // Survey shapes shadow reference and cadastral ones: those are picked only where nothing of
  // the survey is under the click, and repeated clicks cycle through survey shapes only.
  const bool anySurvey = std::any_of(ranked.cbegin(), ranked.cend(),
                                     [](const Ranked& entry) { return std::get<0>(entry.rank) == 0; });
  if (anySurvey) ranked.removeIf([](const Ranked& entry) { return std::get<0>(entry.rank) != 0; });
  std::stable_sort(ranked.begin(), ranked.end(),
                   [](const Ranked& a, const Ranked& b) { return a.rank < b.rank; });
  QList<Hit> out;
  out.reserve(ranked.size());
  for (const Ranked& entry : ranked) out.append(entry.hit);
  return out;
}

Hit at(const QList<QgsMapLayer*>& layers, const QgsPointXY& mapPoint, double mapTolerance,
       const QgsCoordinateReferenceSystem& mapCrs, const QgsCoordinateTransformContext& context) {
  const QList<Hit> all = candidates(layers, mapPoint, mapTolerance, mapCrs, context);
  return all.isEmpty() ? Hit() : all.first();
}

}  // namespace FeaturePick
