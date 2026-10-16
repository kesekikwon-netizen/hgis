#pragma once

#include "core/KaLogExcept.h"
#include "core/LayerOps.h"

#include <algorithm>
#include <functional>
#include <cmath>
#include <limits>
#include <queue>

#include <QHash>

#include <qgsabstractgeometry.h>
#include <qgscoordinatetransform.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgsgeometry.h>
#include <qgslayertree.h>
#include <qgsmapcanvas.h>
#include <qgsmaplayer.h>
#include <qgsmapsettings.h>
#include <qgspoint.h>
#include <qgsproject.h>
#include <qgsrectangle.h>
#include <qgsvectorlayer.h>
#include <qgsvertexid.h>

namespace SnapTraceOps::detail {

inline QList<QgsVectorLayer*> eligibleLayers(QgsMapCanvas* canvas, QgsVectorLayer* excludeIfOnly = nullptr) {
  QList<QgsVectorLayer*> cadRef, survey;
  QgsProject* proj = QgsProject::instance();
  QList<QgsMapLayer*> raw = canvas ? canvas->layers() : QList<QgsMapLayer*>();
  if (proj) {
    QgsLayerTree* root = proj->layerTreeRoot();
    for (QgsMapLayer* ml : proj->mapLayers().values()) {
      if (!ml || raw.contains(ml)) continue;
      if (auto* node = root ? root->findLayer(ml->id()) : nullptr; !node || node->isVisible()) raw.append(ml);
    }
  }
  for (QgsMapLayer* ml : raw) {
    auto* vl = qobject_cast<QgsVectorLayer*>(ml);
    if (!vl || !vl->isValid() || !LayerOps::isSnapSourceLayer(vl) || vl == excludeIfOnly) continue;
    if (LayerOps::isCadastralLayer(vl) || LayerOps::isReferenceLayer(vl))
      cadRef.append(vl);
    else
      survey.append(vl);
  }
  cadRef.append(survey);
  return cadRef;
}

// Hands each geometry in canvasBox (in canvas CRS) to visit until it returns false. Returns how many it saw.
inline int forEachInCanvasBox(QgsVectorLayer* vl, const QgsRectangle& canvasBox,
                              const QgsCoordinateReferenceSystem& canvasCrs, QgsVectorLayer* exLayer,
                              QgsFeatureId exFid, const std::function<bool(const QgsGeometry&)>& visit) {
  if (!vl) return 0;
  QgsRectangle reqBox = canvasBox;
  const bool needXf = canvasCrs.isValid() && vl->crs().isValid() && vl->crs() != canvasCrs;
  QgsCoordinateTransform toLayer, toCanvas;
  if (needXf) {
    const auto ctx =
        QgsProject::instance() ? QgsProject::instance()->transformContext() : QgsCoordinateTransformContext();
    toLayer = QgsCoordinateTransform(canvasCrs, vl->crs(), ctx);
    toCanvas = QgsCoordinateTransform(vl->crs(), canvasCrs, ctx);
    toLayer.setBallparkTransformsAreAppropriate(true);
    toCanvas.setBallparkTransformsAreAppropriate(true);
    try {
      reqBox = toLayer.transformBoundingBox(canvasBox);
    } catch (...) {
      KA_LOG_EXCEPT();
    }
  }
  int seen = 0;
  QgsFeature feat;
  QgsFeatureIterator it = vl->getFeatures(QgsFeatureRequest().setFilterRect(reqBox).setNoAttributes());
  while (it.nextFeature(feat)) {
    if ((vl == exLayer && feat.id() == exFid) || !feat.hasGeometry()) continue;
    QgsGeometry g = feat.geometry();
    if (needXf) {
      try {
        if (g.transform(toCanvas) != Qgis::GeometryOperationResult::Success) continue;
      } catch (...) {
        KA_LOG_EXCEPT();
        continue;
      }
    }
    ++seen;
    if (!visit(g)) break;
  }
  return seen;
}

inline bool toCanvasAndFetch(QgsVectorLayer* vl, const QgsRectangle& canvasBox,
                             const QgsCoordinateReferenceSystem& canvasCrs, QgsVectorLayer* exLayer, QgsFeatureId exFid,
                             QVector<QgsGeometry>& out) {
  forEachInCanvasBox(vl, canvasBox, canvasCrs, exLayer, exFid, [&out](const QgsGeometry& g) { out.append(g); return true; });
  return !out.isEmpty();
}

// Points by square cell: "within r of p" reads nearby cells, not every point (마우스마다 돌므로).
struct PointGrid {
  double cell = 0.0;
  QHash<quint64, QVector<int>> cells;
  static quint64 keyOf(qint64 cx, qint64 cy) {
    return (static_cast<quint64>(static_cast<quint32>(cx)) << 32) | static_cast<quint32>(cy);
  }
  qint64 cellOf(double v) const { return static_cast<qint64>(std::floor(v / cell)); }
  void insert(const QgsPointXY& p, int index) { cells[keyOf(cellOf(p.x()), cellOf(p.y()))].append(index); }
  // Indices of points that may lie in rect (callers grow it). Ascending, no repeats. A long side (km 단위
  // 경계선) can span more cells than are filled: then the filled ones are walked instead.
  QVector<int> candidates(const QgsRectangle& rect) const {
    QVector<int> out;
    const qint64 x0 = cellOf(rect.xMinimum()), x1 = cellOf(rect.xMaximum());
    const qint64 y0 = cellOf(rect.yMinimum()), y1 = cellOf(rect.yMaximum());
    if (static_cast<double>(x1 - x0 + 1) * static_cast<double>(y1 - y0 + 1) > cells.size()) {
      for (auto it = cells.constBegin(); it != cells.constEnd(); ++it) {
        const qint64 cx = static_cast<qint32>(it.key() >> 32), cy = static_cast<qint32>(it.key() & 0xffffffffu);
        if (cx >= x0 && cx <= x1 && cy >= y0 && cy <= y1) out += it.value();
      }
    } else {
      for (qint64 cx = x0; cx <= x1; ++cx)
        for (qint64 cy = y0; cy <= y1; ++cy) out += cells.value(keyOf(cx, cy));
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
  }
};

inline QgsPointXY projectOnSegment(const QgsPointXY& p, const QgsPointXY& a, const QgsPointXY& b) {
  const double dx = b.x() - a.x(), dy = b.y() - a.y(), lenSq = dx * dx + dy * dy;
  if (lenSq <= 1e-12) return a;
  const double t = std::clamp(((p.x() - a.x()) * dx + (p.y() - a.y()) * dy) / lenSq, 0.0, 1.0);
  return QgsPointXY(a.x() + t * dx, a.y() + t * dy);
}

struct BoundaryGraph {
  static constexpr int kMaxNodes = 900;
  double mergeTol = 0.01, mergeSq = 0.0001;
  QVector<QgsPointXY> nodes;
  QVector<QVector<QPair<int, double>>> adj;
  QVector<QPair<int, int>> segs;
  PointGrid grid;  // nodes by cell; the cell is never smaller than mergeTol

  bool full() const { return nodes.size() >= kMaxNodes; }
  void ensureGrid(const QgsRectangle* box) {
    if (grid.cell > 0.0) return;
    const double span = box ? std::max(box->width(), box->height()) : 0.0;
    grid.cell = std::max(mergeTol * 2.0, std::isfinite(span) ? span / 128.0 : 0.0);
  }
  // The lowest-index node within mergeTol of p, as a scan over all nodes would find; -1 if none.
  int findNode(const QgsPointXY& p) const {
    const QgsRectangle around(p.x() - mergeTol, p.y() - mergeTol, p.x() + mergeTol, p.y() + mergeTol);
    for (int i : grid.candidates(around))
      if (nodes[i].sqrDist(p) <= mergeSq) return i;
    return -1;
  }
  int addNode(const QgsPointXY& p) {
    ensureGrid(nullptr);
    if (const int i = findNode(p); i >= 0) return i;
    nodes.append(p);
    adj.emplaceBack();
    grid.insert(p, nodes.size() - 1);
    return nodes.size() - 1;
  }
  void addEdge(int u, int v) {
    if (u == v) return;
    const double w = nodes[u].distance(nodes[v]);
    adj[u].append({v, w});
    adj[v].append({u, w});
    segs.append({u, v});
  }
  void addGeometry(const QgsGeometry& geom, const QgsRectangle& box) {
    ensureGrid(&box);
    const QgsAbstractGeometry* ag = geom.constGet();
    if (!ag) return;
    for (int p = 0; p < ag->partCount(); ++p)
      for (int r = 0; r < ag->ringCount(p); ++r)
        for (int v = 0, n = ag->vertexCount(p, r); v + 1 < n; ++v) {
          const QgsPoint p0 = ag->vertexAt(QgsVertexId(p, r, v)), p1 = ag->vertexAt(QgsVertexId(p, r, v + 1));
          const QgsPointXY a(p0.x(), p0.y()), b(p1.x(), p1.y());
          if (box.contains(a) || box.contains(b)) addEdge(addNode(a), addNode(b));
        }
  }
  // A corner of one parcel lying on another parcel's side joins that side (T-junction).
  void linkTJunctions() {
    const double reach = mergeTol * 2.0;  // sqrt(mergeSq * 4)
    const int origSegs = segs.size();
    for (int s = 0; s < origSegs; ++s) {
      const auto [u, v] = segs[s];
      QgsRectangle around(nodes[u], nodes[v]);
      around.grow(reach);
      for (int w : grid.candidates(around)) {
        if (w == u || w == v) continue;
        if (projectOnSegment(nodes[w], nodes[u], nodes[v]).sqrDist(nodes[w]) <= mergeSq * 4.0) {
          addEdge(u, w);
          addEdge(w, v);
        }
      }
    }
  }
  void addGeometries(const QVector<QgsGeometry>& geoms, const QgsRectangle& box) {
    for (const QgsGeometry& geom : geoms) {
      if (full()) break;
      addGeometry(geom, box);
    }
    linkTJunctions();
  }
  QgsPointXY snapPoint(const QgsPointXY& target, double vTol, double sTol, bool* moved) {
    int bestN = -1, bestS = -1;
    double bestND = vTol, bestSD = sTol;
    QgsPointXY bestProj = target;
    for (int i = 0; i < nodes.size(); ++i)
      if (const double d = nodes[i].distance(target); d <= bestND) {
        bestND = d;
        bestN = i;
      }
    for (int i = 0; i < segs.size(); ++i) {
      const QgsPointXY proj = projectOnSegment(target, nodes[segs[i].first], nodes[segs[i].second]);
      if (const double d = proj.distance(target); d < bestSD) {
        bestSD = d;
        bestS = i;
        bestProj = proj;
      }
    }
    QgsPointXY res = target;
    if (bestN >= 0 && bestND <= std::max(bestSD * 2.2, vTol * 0.45))
      res = nodes[bestN];
    else if (bestS >= 0)
      res = bestProj;
    if (moved) *moved = (res.sqrDist(target) > mergeSq);
    return res;
  }
  int attach(const QgsPointXY& target, double snapTol, double vTol) {
    int bestN = -1, bestS = -1;
    double bestND = snapTol, bestSD = snapTol;
    QgsPointXY bestProj;
    for (int i = 0; i < nodes.size(); ++i)
      if (const double d = nodes[i].distance(target); d <= bestND) {
        bestND = d;
        bestN = i;
      }
    if (bestN >= 0 && bestND <= vTol) return bestN;
    for (int i = 0; i < segs.size(); ++i) {
      const QgsPointXY proj = projectOnSegment(target, nodes[segs[i].first], nodes[segs[i].second]);
      if (const double d = proj.distance(target); d < bestSD) {
        bestSD = d;
        bestS = i;
        bestProj = proj;
      }
    }
    if (bestS < 0) return bestN;
    const auto [u, v] = segs[bestS];
    if (bestProj.distance(nodes[u]) <= vTol) return u;
    if (bestProj.distance(nodes[v]) <= vTol) return v;
    const int w = addNode(bestProj);
    addEdge(w, u);
    addEdge(w, v);
    return w;
  }
  QVector<QgsPointXY> trace(const QgsPointXY& fromPt, const QgsPointXY& toPt, double snapTol, double vTol,
                            double skipSq, int maxMids) {
    const double chord = fromPt.distance(toPt);
    if (nodes.size() < 3 || !std::isfinite(chord) || chord <= mergeTol * 2.0) return {};
    const int s = attach(fromPt, snapTol, vTol), t = attach(toPt, snapTol, vTol);
    if (s < 0 || t < 0 || s == t) return {};
    const double corridorSq = std::pow(std::max(snapTol * 1.5, chord * 0.45), 2);
    QVector<double> dist(nodes.size(), std::numeric_limits<double>::infinity());
    QVector<int> parent(nodes.size(), -1);
    using Item = QPair<double, int>;
    std::priority_queue<Item, QVector<Item>, std::greater<Item>> pq;
    pq.push({dist[s] = 0.0, s});
    while (!pq.empty()) {
      const auto [d, u] = pq.top();
      pq.pop();
      if (u == t) break;
      if (d > dist[u]) continue;
      for (const auto& [v, w] : adj[u]) {
        if (projectOnSegment(nodes[v], fromPt, toPt).sqrDist(nodes[v]) > corridorSq) continue;
        if (d + w < dist[v]) {
          parent[v] = u;
          pq.push({dist[v] = d + w, v});
        }
      }
    }
    if (!std::isfinite(dist[t]) || dist[t] > chord * 2.1) return {};
    QVector<int> chain;
    for (int cur = parent[t]; cur >= 0 && cur != s; cur = parent[cur])
      chain.prepend(cur);
    QVector<QgsPointXY> mids;
    for (int idx : chain)
      if (nodes[idx].sqrDist(fromPt) > skipSq && nodes[idx].sqrDist(toPt) > skipSq) mids.append(nodes[idx]);
    return mids.size() <= maxMids ? mids : QVector<QgsPointXY>{};
  }
};

}  // namespace SnapTraceOps::detail
