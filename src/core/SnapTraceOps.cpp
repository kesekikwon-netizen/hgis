#include "core/SnapTraceOps.h"
#include "core/SnapTraceGraph.h"

#include <QColor>
#include <qgsvertexmarker.h>

namespace SnapTraceOps {
using detail::BoundaryGraph;
using detail::eligibleLayers;
using detail::toCanvasAndFetch;

QVector<QgsPointXY> nearbyVertices(QgsMapCanvas* canvas, const QgsPointXY& cursorMapPt, const QgsPointXY* activeSnapPt,
                                   double radiusPx, int maxCount) {
  QVector<QgsPointXY> result;
  if (!canvas || radiusPx <= 0.0 || maxCount <= 0 || !std::isfinite(cursorMapPt.x()) || !std::isfinite(cursorMapPt.y()))
    return result;
  const double mupp = canvas->mapUnitsPerPixel() > 0.0 ? canvas->mapUnitsPerPixel() : 0.05;
  const double radiusMap = radiusPx * mupp, radiusSq = radiusMap * radiusMap;
  const double dedupSq = std::pow(std::max(mupp * 2.5, 0.01), 2);
  const QgsRectangle box(cursorMapPt.x() - radiusMap, cursorMapPt.y() - radiusMap, cursorMapPt.x() + radiusMap,
                         cursorMapPt.y() + radiusMap);
  const QgsCoordinateReferenceSystem canvasCrs = canvas->mapSettings().destinationCrs();
  QVector<QPair<double, QgsPointXY>> cands;
  const double dedup = std::sqrt(dedupSq);
  detail::PointGrid seen{dedup, {}};
  for (QgsVectorLayer* vl : eligibleLayers(canvas)) {
    QVector<QgsGeometry> geoms;
    if (!toCanvasAndFetch(vl, box, canvasCrs, nullptr, FID_NULL, geoms)) continue;
    for (const QgsGeometry& geom : geoms) {
      for (auto vit = geom.vertices_begin(); vit != geom.vertices_end(); ++vit) {
        const QgsPointXY pt((*vit).x(), (*vit).y());
        const double dSq = cursorMapPt.sqrDist(pt);
        if (dSq > radiusSq || (activeSnapPt && activeSnapPt->sqrDist(pt) <= dedupSq)) continue;
        const QgsRectangle around(pt.x() - dedup, pt.y() - dedup, pt.x() + dedup, pt.y() + dedup);
        const QVector<int> near = seen.candidates(around);
        if (std::any_of(near.begin(), near.end(), [&](int i) { return cands[i].second.sqrDist(pt) <= dedupSq; }))
          continue;
        seen.insert(pt, cands.size());
        cands.append({dSq, pt});
      }
    }
  }
  std::sort(cands.begin(), cands.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
  for (int i = 0, take = std::min(static_cast<int>(cands.size()), maxCount); i < take; ++i)
    result.append(cands[i].second);
  return result;
}

void clearPreviewMarkers(QList<QgsVertexMarker*>& markers) {
  qDeleteAll(markers);
  markers.clear();
}

void syncPreviewMarkers(QgsMapCanvas* canvas, QList<QgsVertexMarker*>& markers, const QVector<QgsPointXY>& points) {
  if (!canvas || points.isEmpty()) {
    clearPreviewMarkers(markers);
    return;
  }
  while (markers.size() > points.size())
    delete markers.takeLast();
  while (markers.size() < points.size()) {
    auto* m = new QgsVertexMarker(canvas);
    m->setIconType(QgsVertexMarker::ICON_CIRCLE);
    m->setIconSize(9);
    m->setPenWidth(2);
    m->setColor(QColor(217, 119, 6, 230));
    m->setFillColor(QColor(255, 251, 235, 210));
    markers.append(m);
  }
  for (int i = 0; i < points.size(); ++i) {
    markers[i]->setCenter(points[i]);
    markers[i]->show();
  }
}

QVector<QgsPointXY> traceBetween(QgsMapCanvas* canvas, const QgsPointXY& fromMapPt, const QgsPointXY& toMapPt,
                                 int maxIntermediate) {
  if (!canvas || maxIntermediate <= 0) return {};
  const double chord = fromMapPt.distance(toMapPt);
  const double mupp = canvas->mapUnitsPerPixel() > 0.0 ? canvas->mapUnitsPerPixel() : 0.05;
  const double snapTol = std::max(mupp * 16.0, 0.25), mergeTol = std::max(mupp * 0.4, 0.005);
  const double skipSq = std::pow(std::max(mupp * 2.0, 0.02), 2), vTol = std::max(mergeTol * 4.0, mupp * 4.0);
  if (!std::isfinite(chord) || chord <= snapTol * 0.5) return {};
  QgsRectangle box(fromMapPt, toMapPt);
  box.grow(std::max(chord * 0.6, snapTol * 2.0));
  const QgsCoordinateReferenceSystem canvasCrs = canvas->mapSettings().destinationCrs();
  BoundaryGraph graph{mergeTol, mergeTol * mergeTol, {}, {}, {}};
  for (QgsVectorLayer* vl : eligibleLayers(canvas)) {
    // Stop reading once the graph is full: the rest would be skipped anyway (넓게 본 시 전체 지적).
    const int seen = detail::forEachInCanvasBox(vl, box, canvasCrs, nullptr, FID_NULL, [&](const QgsGeometry& g) {
      if (graph.full()) return false;
      graph.addGeometry(g, box);
      return true;
    });
    if (seen > 0) graph.linkTJunctions();
  }
  return graph.trace(fromMapPt, toMapPt, snapTol, vTol, skipSq, maxIntermediate);
}

QgsGeometry snapGeometryToNearestLines(QgsMapCanvas* canvas, QgsVectorLayer* targetLayer, QgsFeatureId ignoreFid,
                                       const QgsGeometry& geom, int* movedOut, int* insertedOut) {
  if (movedOut) *movedOut = 0;
  if (insertedOut) *insertedOut = 0;
  if (!targetLayer || geom.isEmpty()) return geom;
  const QgsCoordinateReferenceSystem canvasCrs = (canvas && canvas->mapSettings().destinationCrs().isValid())
                                                     ? canvas->mapSettings().destinationCrs()
                                                     : targetLayer->crs();
  const bool needXf = canvasCrs.isValid() && targetLayer->crs().isValid() && targetLayer->crs() != canvasCrs;
  const auto ctx =
      QgsProject::instance() ? QgsProject::instance()->transformContext() : QgsCoordinateTransformContext();
  QgsGeometry gMap = geom;
  if (needXf) {
    QgsCoordinateTransform toMap(targetLayer->crs(), canvasCrs, ctx);
    toMap.setBallparkTransformsAreAppropriate(true);
    try {
      if (gMap.transform(toMap) != Qgis::GeometryOperationResult::Success) return geom;
    } catch (...) {
      KA_LOG_EXCEPT();
      return geom;
    }
  }
  const QgsRectangle bbox = gMap.boundingBox();
  const double diag = std::hypot(bbox.width(), bbox.height());
  const double mupp = (canvas && canvas->mapUnitsPerPixel() > 0.0) ? canvas->mapUnitsPerPixel() : 0.08;
  const double snapTol = std::clamp(std::max(mupp * 48.0, diag * 0.24), 2.5, 25.0);
  const double vTol = snapTol * 0.78, mergeTol = std::max(mupp * 0.3, 0.01), skipSq = std::pow(mergeTol * 2.5, 2);
  QgsRectangle searchBox = bbox;
  searchBox.grow(snapTol * 1.5);
  BoundaryGraph graph{mergeTol, mergeTol * mergeTol, {}, {}, {}};
  for (QgsVectorLayer* vl : eligibleLayers(canvas)) {
    QVector<QgsGeometry> geoms;
    if (toCanvasAndFetch(vl, searchBox, canvasCrs, targetLayer, ignoreFid, geoms))
      graph.addGeometries(geoms, searchBox);
  }
  if (graph.nodes.isEmpty()) return geom;

  auto processRing = [&](const QVector<QgsPointXY>& inRing, bool closed) -> QVector<QgsPointXY> {
    const int n = closed && inRing.size() > 1 && inRing.first() == inRing.last() ? inRing.size() - 1 : inRing.size();
    if (n < (closed ? 3 : 2)) return inRing;
    QVector<QgsPointXY> snapped(n);
    for (int i = 0; i < n; ++i) {
      bool moved = false;
      snapped[i] = graph.snapPoint(inRing[i], vTol, snapTol, &moved);
      if (moved && movedOut) ++(*movedOut);
    }
    QVector<QgsPointXY> built;
    for (int i = 0; i < n; ++i) {
      built.append(snapped[i]);
      if (!closed && i + 1 >= n) break;
      const QgsPointXY& nextPt = snapped[(i + 1) % n];
      const QVector<QgsPointXY> mids = graph.trace(snapped[i], nextPt, snapTol, vTol, skipSq, 24);
      if (!mids.isEmpty()) {
        built.append(mids);
        if (insertedOut) *insertedOut += mids.size();
      }
    }
    QVector<QgsPointXY> clean;
    for (const QgsPointXY& pt : built) {
      if (!clean.isEmpty() && clean.last().sqrDist(pt) <= graph.mergeSq) continue;
      if (clean.size() >= 2 && clean[clean.size() - 2].sqrDist(pt) <= graph.mergeSq) {
        clean.removeLast();
        continue;
      }
      clean.append(pt);
    }
    if (closed && clean.size() >= 3) {
      if (clean.first().sqrDist(clean.last()) <= graph.mergeSq) clean.removeLast();
      if (clean.size() >= 3) clean.append(clean.first());
    }
    return clean.size() >= (closed ? 4 : 2) ? clean : inRing;
  };

  QgsGeometry outGeom = gMap;
  if (gMap.type() == Qgis::GeometryType::Polygon) {
    QgsPolygonXY poly = gMap.isMultipart() ? gMap.asMultiPolygon().value(0) : gMap.asPolygon();
    if (!poly.isEmpty()) {
      poly[0] = processRing(poly[0], true);
      outGeom = QgsGeometry::fromPolygonXY(poly);
    }
  } else if (gMap.type() == Qgis::GeometryType::Line) {
    QgsPolylineXY line = gMap.isMultipart() ? gMap.asMultiPolyline().value(0) : gMap.asPolyline();
    if (!line.isEmpty()) outGeom = QgsGeometry::fromPolylineXY(processRing(line, false));
  }
  if (needXf) {
    QgsCoordinateTransform toLayer(canvasCrs, targetLayer->crs(), ctx);
    toLayer.setBallparkTransformsAreAppropriate(true);
    try {
      outGeom.transform(toLayer);
    } catch (...) {
      KA_LOG_EXCEPT();
      return geom;
    }
  }
  return outGeom;
}

bool snapFeatureToNearestLines(QgsMapCanvas* canvas, QgsVectorLayer* targetLayer, QgsFeatureId targetFid, int* movedOut,
                               int* insertedOut) {
  if (!targetLayer || !targetLayer->isValid() || FID_IS_NULL(targetFid)) return false;
  const QgsFeature feat = targetLayer->getFeature(targetFid);
  if (!feat.isValid() || !feat.hasGeometry()) return false;
  int moved = 0, inserted = 0;
  QgsGeometry snapped = snapGeometryToNearestLines(canvas, targetLayer, targetFid, feat.geometry(), &moved, &inserted);
  if (movedOut) *movedOut = moved;
  if (insertedOut) *insertedOut = inserted;
  if ((moved == 0 && inserted == 0) || snapped.isEmpty()) return false;
  if (!targetLayer->isEditable() && !targetLayer->startEditing()) return false;
  targetLayer->beginEditCommand(QStringLiteral("가까운 선에 붙이기"));
  const bool ok = targetLayer->changeGeometry(targetFid, snapped);
  if (ok)
    targetLayer->endEditCommand();
  else
    targetLayer->destroyEditCommand();
  if (ok) targetLayer->triggerRepaint();
  return ok;
}

int snapCanvasShapesToNearestLines(QgsMapCanvas* canvas, QgsVectorLayer* fallbackLayer, QgsFeatureId fallbackFid,
                                   QString* statusOut) {
  QgsProject* proj = QgsProject::instance();
  if (!proj) return 0;
  QList<QPair<QgsVectorLayer*, QgsFeatureId>> targets;
  for (QgsMapLayer* ml : proj->mapLayers().values()) {
    auto* vl = qobject_cast<QgsVectorLayer*>(ml);
    if (!vl || !vl->isValid() || LayerOps::layerKeyOf(vl).isEmpty() || LayerOps::isReferenceLayer(vl) ||
        LayerOps::isCadastralLayer(vl))
      continue;
    for (QgsFeatureId fid : vl->selectedFeatureIds())
      targets.append({vl, fid});
  }
  if (targets.isEmpty() && fallbackLayer && !FID_IS_NULL(fallbackFid) &&
      fallbackLayer->getFeature(fallbackFid).hasGeometry()) {
    fallbackLayer->selectByIds({fallbackFid});
    targets.append({fallbackLayer, fallbackFid});
  }
  if (targets.isEmpty()) {
    for (const QString& key : {QStringLiteral("survey_area"), QStringLiteral("feature_poly"),
                               QStringLiteral("feature_line"), QStringLiteral("trial_trench")}) {
      for (QgsVectorLayer* vl : LayerOps::findAllByLayerKey(proj, key)) {
        if (!vl || !vl->isValid()) continue;
        QgsFeature f, lastF;
        QgsFeatureIterator it = vl->getFeatures();
        while (it.nextFeature(f))
          if (f.hasGeometry()) lastF = f;
        if (lastF.isValid()) {
          vl->selectByIds({lastF.id()});
          targets.append({vl, lastF.id()});
          break;
        }
      }
      if (!targets.isEmpty()) break;
    }
  }
  if (targets.isEmpty()) {
    if (statusOut) *statusOut = QStringLiteral("먼저 지적도·도면 위에 도형을 그리거나 도형선택으로 도형을 고르세요.");
    return 0;
  }
  int changedCount = 0, totalMoved = 0, totalInserted = 0;
  for (const auto& [vl, fid] : targets) {
    int m = 0, ins = 0;
    if (snapFeatureToNearestLines(canvas, vl, fid, &m, &ins)) {
      ++changedCount;
      totalMoved += m;
      totalInserted += ins;
    }
  }
  if (statusOut) {
    *statusOut = changedCount > 0
                     ? QStringLiteral(
                           "도형을 가장 가까운 선에 붙였습니다 (꼭짓점 %1개 맞춤 · 사이점 %2개 채움). Ctrl+Z로 되돌릴 "
                           "수 있습니다.")
                           .arg(totalMoved)
                           .arg(totalInserted)
                     : QStringLiteral("이미 가장 가까운 선에 붙어 있거나 주변에 붙일 기준 선(지적·도면)이 없습니다.");
  }
  return changedCount;
}

}  // namespace SnapTraceOps
