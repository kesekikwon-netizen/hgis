#include "FeaturePick.h"

#include "KaSessionLog.h"
#include "LayerOps.h"

#include <cmath>
#include <tuple>

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

namespace FeaturePick {

Hit at(const QList<QgsMapLayer*>& layers, const QgsPointXY& mapPoint, double mapTolerance,
       const QgsCoordinateReferenceSystem& mapCrs, const QgsCoordinateTransformContext& context) {
  // 작을수록 먼저: 레이어 종류(조사 0, 참조·지적 1), 도형 종류(점·선 0, 품는 면 1, 가까운 면 2),
  // 크기(허용 오차 대비 거리 또는 면적), 레이어 순서.
  using Rank = std::tuple<int, int, double, int>;
  Hit best;
  Rank bestRank;
  int order = -1;
  for (QgsMapLayer* mapLayer : layers) {
    ++order;
    auto* layer = qobject_cast<QgsVectorLayer*>(mapLayer);
    if (!layer || !layer->isValid()) continue;

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
        KaSessionLog::line(QStringLiteral("[except] core/FeaturePick.cpp:transform"));
        continue;
      }
    }
    if (!(tolerance > 0.0)) continue;

    const int tier = LayerOps::isReferenceOrBasemapLayer(layer) ? 1 : 0;
    const bool polygons = layer->geometryType() == Qgis::GeometryType::Polygon;
    const QgsGeometry probe = QgsGeometry::fromPointXY(point);
    QgsFeatureRequest request;
    request.setFilterRect(QgsRectangle(point.x() - tolerance, point.y() - tolerance,
                                       point.x() + tolerance, point.y() + tolerance));
    QgsFeatureIterator it = layer->getFeatures(request);
    QgsFeature feature;
    while (it.nextFeature(feature)) {
      if (!feature.hasGeometry()) continue;
      const QgsGeometry geometry = feature.geometry();
      if (geometry.isEmpty()) continue;
      Rank rank;
      if (polygons && geometry.contains(probe)) {
        rank = Rank(tier, 1, geometry.area() / (tolerance * tolerance), order);
      } else {
        const double distance = geometry.distance(probe);
        if (distance < 0.0 || distance > tolerance) continue;
        rank = Rank(tier, polygons ? 2 : 0, distance / tolerance, order);
      }
      if (!best.valid() || rank < bestRank) {
        best.layer = layer;
        best.fid = feature.id();
        bestRank = rank;
      }
    }
  }
  return best;
}

}  // namespace FeaturePick
