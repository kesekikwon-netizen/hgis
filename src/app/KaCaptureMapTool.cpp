#include "KaCrashGuard.h"
#include "KaCaptureMapTool.h"
#include "core/LayerOps.h"
#include <qgsmapcanvas.h>
#include <qgsvectorlayer.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsrubberband.h>
#include <qgsvertexmarker.h>
#include <qgssnappingutils.h>
#include <qgspointlocator.h>
#include <qgsgeometry.h>
#include <qgscoordinatetransform.h>
#include <qgsexception.h>
#include <qgsproject.h>
#include <qgsmapsettings.h>
#include <QKeyEvent>
#include <QKeySequence>
#include <QColor>
#include <QWidget>
#include <cmath>

KaCaptureMapTool::KaCaptureMapTool(QgsMapCanvas* canvas)
    : QgsMapTool(canvas) {
  setCursor(Qt::CrossCursor);
}

KaCaptureMapTool::~KaCaptureMapTool() {
  destroyRubber();
  destroySnapMarker();
}

QgsMapTool::Flags KaCaptureMapTool::flags() const {
  return QgsMapTool::EditTool;
}

void KaCaptureMapTool::setMode(Mode mode) {
  m_mode = mode;
  resetSession();
}

void KaCaptureMapTool::setTargetLayer(QgsVectorLayer* layer) {
  if (m_layer != layer && m_draggingVertex) cancelVertexDrag();
  m_layer = layer;
}

void KaCaptureMapTool::setSnapEnabled(bool on) {
  m_snapEnabled = on;
  if (!on) destroySnapMarker();
}

void KaCaptureMapTool::setEasyDraw(bool on) {
  m_easyDraw = on;
  if (on) {
    m_snapEnabled = true;
    setCursor(Qt::CrossCursor);
  }
}

void KaCaptureMapTool::resetSession() {
  m_finishing = false;
  m_points.clear();
  cancelVertexDrag();
  destroyRubber();
}

void KaCaptureMapTool::destroyRubber() {
  if (!m_rubber) return;
  m_rubber->reset(Qgis::GeometryType::Line);
  delete m_rubber;
  m_rubber = nullptr;
}

void KaCaptureMapTool::destroySnapMarker() {
  delete m_snapMark;
  m_snapMark = nullptr;
}

void KaCaptureMapTool::updateSnapMarker(const QgsPointXY& mapPt, bool snapped, bool isIntersection) {
  QgsMapCanvas* c = canvas();
  if (!c || !m_snapEnabled || !snapped) {
    destroySnapMarker();
    return;
  }
  if (!m_snapMark) {
    m_snapMark = new QgsVertexMarker(c);
  }
  if (isIntersection) {
    m_snapMark->setIconType(QgsVertexMarker::ICON_X);
    m_snapMark->setIconSize(16);
    m_snapMark->setPenWidth(3);
    m_snapMark->setColor(QColor(220, 20, 60)); // 선과 선이 만나는 교차점은 진한 적색 X로 명확히 표시
    m_snapMark->setFillColor(QColor(255, 255, 255, 240));
  } else {
    m_snapMark->setIconType(QgsVertexMarker::ICON_CIRCLE);
    m_snapMark->setIconSize(14);
    m_snapMark->setPenWidth(2);
    m_snapMark->setColor(QColor(30, 103, 198));
    m_snapMark->setFillColor(QColor(255, 255, 255, 220));
  }
  m_snapMark->setCenter(mapPt);
  m_snapMark->show();
}

bool KaCaptureMapTool::nearPoint(const QgsPointXY& a, const QgsPointXY& b) const {
  double tol = 0.15;
  if (canvas()) {
    const double mupp = canvas()->mapUnitsPerPixel();
    if (mupp > 0) tol = std::max(mupp * 10.0, 0.05);
  }
  return a.sqrDist(b) <= tol * tol;
}

int KaCaptureMapTool::indexOfSketchVertex(const QgsPointXY& pt) const {
  for (int i = 0; i < m_points.size(); ++i) {
    if (nearPoint(m_points.at(i), pt)) return i;
  }
  return -1;
}

bool KaCaptureMapTool::mapPointFromEvent(QgsMapMouseEvent* e, QgsPointXY* out, bool* snappedOut) {
  if (!e || !out || !canvas()) return false;
  bool snapped = false;
  bool isInter = false;
  try {
    *out = e->mapPoint();
  } catch (...) {
    KaCrashGuard::logLine(QStringLiteral("[except] app/KaCaptureMapTool.cpp:127"));
    try {
      *out = toMapCoordinates(e->pos());
    } catch (...) {
      KaCrashGuard::logLine(QStringLiteral("[except] app/KaCaptureMapTool.cpp:129"));
      return false;
    }
  }
  if (m_snapEnabled && canvas()->snappingUtils()) {
    const QgsPointLocator::Match hit = canvas()->snappingUtils()->snapToMap(e->pos());
    if (hit.isValid()) {
      *out = hit.point();
      snapped = true;
      // QGIS PointLocator에서 교차점(intersection)에 스냅된 경우 hit.layer()는 null이다.
      isInter = (hit.layer() == nullptr);
    }
  }
  if (!std::isfinite(out->x()) || !std::isfinite(out->y())) return false;
  updateSnapMarker(*out, snapped, isInter);
  if (snappedOut) *snappedOut = snapped;
  return true;
}

void KaCaptureMapTool::rebuildRubber(const QgsPointXY* cursorOrNull) {
  QgsMapCanvas* c = canvas();
  if (!c) return;

  if (!m_rubber) {
    Qgis::GeometryType gt = Qgis::GeometryType::Line;
    if (m_mode == Mode::Polygon) gt = Qgis::GeometryType::Polygon;
    else if (m_mode == Mode::Point) gt = Qgis::GeometryType::Point;
    m_rubber = new QgsRubberBand(c, gt);
    m_rubber->setWidth(3);
    m_rubber->setSecondaryStrokeColor(QColor(255, 255, 255, 200));
    m_rubber->setColor(QColor(30, 103, 198));
    m_rubber->setFillColor(QColor(30, 103, 198, 80));
  }

  Qgis::GeometryType gt = Qgis::GeometryType::Line;
  if (m_mode == Mode::Polygon) gt = Qgis::GeometryType::Polygon;
  else if (m_mode == Mode::Point) gt = Qgis::GeometryType::Point;
  m_rubber->reset(gt);

  for (const QgsPointXY& p : m_points)
    m_rubber->addPoint(p, false);

  if (cursorOrNull && m_mode != Mode::Point)
    m_rubber->addPoint(*cursorOrNull, true);
  else if (!m_points.isEmpty())
    m_rubber->addPoint(m_points.last(), true);
}

void KaCaptureMapTool::activate() {
  m_finishing = false;
  m_points.clear();
  destroyRubber();
  if (canvas()) {
    canvas()->setMouseTracking(true);
    canvas()->freeze(false);
    canvas()->setRenderFlag(true);
    m_savedMenuPolicy = canvas()->contextMenuPolicy();
    canvas()->setContextMenuPolicy(Qt::PreventContextMenu);
  }
  QgsMapTool::activate();
  setCursor(Qt::CrossCursor);
}

void KaCaptureMapTool::deactivate() {
  if (canvas())
    canvas()->setContextMenuPolicy(m_savedMenuPolicy);
  cancelVertexDrag();
  destroyRubber();
  destroySnapMarker();
  if (!m_finishing)
    m_points.clear();
  m_finishing = false;
  QgsMapTool::deactivate();
}

void KaCaptureMapTool::canvasPressEvent(QgsMapMouseEvent* e) {
  if (!e || !canvas() || m_finishing) return;

  if (e->button() == Qt::RightButton) {
    e->accept();
    if (m_draggingVertex)
      cancelVertexDrag();
    else
      finish();
    return;
  }

  QgsPointXY mapPt;
  bool snapped = false;
  if (!mapPointFromEvent(e, &mapPt, &snapped)) return;

  if (e->button() == Qt::LeftButton) {
    e->accept();
    if (m_mode == Mode::Point) {
      m_points.clear();
      m_points.append(mapPt);
      finish();
      return;
    }
    if (m_points.isEmpty() && m_mode != Mode::Point) {
      QgsFeatureId fid = -1;
      int vertex = -1;
      try {
        if (hitSavedVertex(mapPt, &fid, &vertex)) {
          m_draggingVertex = true;
          m_dragFid = fid;
          m_dragVertex = vertex;
          previewMovedVertex(mapPt);
          return;
        }
      } catch (const QgsCsException&) {
        emit vertexMoveFailed(QStringLiteral("지도와 도형의 위치를 맞추지 못했습니다. 작업을 저장한 뒤 조사를 다시 열어 주세요."));
        return;
      }
    }
    if (m_easyDraw && !m_points.isEmpty()) {
      const int idx = indexOfSketchVertex(mapPt);
      if (idx >= 0) {
        m_points.resize(idx + 1);
        rebuildRubber(&mapPt);
        return;
      }
    }
    if (m_points.isEmpty() || !nearPoint(m_points.last(), mapPt))
      m_points.append(mapPt);
    rebuildRubber(&mapPt);
  }
}

void KaCaptureMapTool::canvasReleaseEvent(QgsMapMouseEvent* e) {
  if (!e || !canvas() || m_finishing) return;
  if (e->button() == Qt::LeftButton && m_draggingVertex) {
    e->accept();
    QgsPointXY mapPt;
    if (mapPointFromEvent(e, &mapPt))
      finishVertexDrag(mapPt);
    else {
      cancelVertexDrag();
      emit vertexMoveFailed(QStringLiteral("꼭짓점 위치를 읽지 못했습니다. 지도 안에서 다시 옮겨 주세요."));
    }
    return;
  }
  if (e->button() != Qt::RightButton) return;
  e->accept();
  if (!m_points.isEmpty())
    finish();
}

void KaCaptureMapTool::canvasDoubleClickEvent(QgsMapMouseEvent* e) {
  if (!e || m_finishing || m_mode == Mode::Point) return;
  e->accept();
  QgsPointXY mapPt;
  if (mapPointFromEvent(e, &mapPt)) {
    if (m_points.isEmpty() || m_points.last() != mapPt)
      m_points.append(mapPt);
  }
  finish();
}

void KaCaptureMapTool::canvasMoveEvent(QgsMapMouseEvent* e) {
  if (!e || m_finishing) return;
  QgsPointXY mapPt;
  bool snapped = false;
  if (!mapPointFromEvent(e, &mapPt, &snapped)) return;
  if (m_draggingVertex) {
    previewMovedVertex(mapPt);
    return;
  }
  if (m_points.isEmpty() || m_mode == Mode::Point) {
    return;
  }
  if (m_easyDraw && snapped && indexOfSketchVertex(mapPt) < 0)
    m_points.append(mapPt);
  rebuildRubber(&mapPt);
}

bool KaCaptureMapTool::undoLastVertex() {
  if (m_finishing || m_points.isEmpty()) return false;
  m_points.removeLast();
  if (m_points.isEmpty())
    destroyRubber();
  else if (canvas()) {
    const QgsPointXY cur = toMapCoordinates(canvas()->mouseLastXY());
    rebuildRubber(&cur);
  }
  return true;
}

void KaCaptureMapTool::keyPressEvent(QKeyEvent* e) {
  if (!e || m_finishing) return;
  if (e->key() == Qt::Key_Escape) {
    cancel();
    e->accept();
    return;
  }
  if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
    finish();
    e->accept();
    return;
  }
  if (e->matches(QKeySequence::Undo) ||
      ((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_Z)) {
    if (undoLastVertex())
      e->accept();
    return;
  }
  if (e->key() == Qt::Key_Backspace || e->key() == Qt::Key_Delete) {
    if (undoLastVertex())
      e->accept();
  }
}

void KaCaptureMapTool::finish() {
  if (m_finishing) return;

  const int need = (m_mode == Mode::Point) ? 1 : (m_mode == Mode::Line ? 2 : 3);
  if (m_points.size() < need) {
    if (!m_points.isEmpty())
      emit captureCanceled();
    return;
  }

  m_finishing = true;

  QgsGeometry geom;
  bool ok = false;
  if (m_mode == Mode::Point) {
    geom = QgsGeometry::fromPointXY(m_points.first());
    ok = !geom.isEmpty();
  } else if (m_mode == Mode::Line) {
    QgsPolylineXY line;
    for (const QgsPointXY& p : m_points) {
      if (line.isEmpty() || !nearPoint(line.last(), p))
        line.append(p);
    }
    geom = QgsGeometry::fromPolylineXY(line);
    ok = !geom.isEmpty();
  } else {
    QgsPolylineXY ring;
    for (const QgsPointXY& p : m_points) {
      if (ring.isEmpty() || !nearPoint(ring.last(), p))
        ring.append(p);
    }
    if (ring.size() >= 3 && ring.first() != ring.last())
      ring.append(ring.first());
    geom = QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring);
    ok = !geom.isEmpty();
    if (ok && !geom.isGeosValid()) {
      const QgsGeometry fixed = geom.makeValid();
      if (!fixed.isEmpty())
        geom = fixed;
    }
    if (!geom.isEmpty() && (geom.isMultipart() || geom.type() != Qgis::GeometryType::Polygon)) {
      QgsGeometry best;
      double bestA = -1;
      const QVector<QgsGeometry> parts = geom.asGeometryCollection();
      for (const QgsGeometry& part : parts) {
        if (part.type() != Qgis::GeometryType::Polygon) continue;
        if (part.isMultipart()) {
          const QgsMultiPolygonXY mp = part.asMultiPolygon();
          for (const QgsPolygonXY& poly : mp) {
            const QgsGeometry one = QgsGeometry::fromPolygonXY(poly);
            const double a = one.area();
            if (a > bestA) {
              bestA = a;
              best = one;
            }
          }
        } else {
          const double a = part.area();
          if (a > bestA) {
            bestA = a;
            best = part;
          }
        }
      }
      if (!best.isEmpty())
        geom = best;
    }
    ok = !geom.isEmpty() && geom.type() == Qgis::GeometryType::Polygon;
  }

  if (!ok) {
    m_finishing = false;
    emit captureCanceled();
    return;
  }

  if (m_layer && canvas()) {
    const QgsCoordinateReferenceSystem src = canvas()->mapSettings().destinationCrs();
    const QgsCoordinateReferenceSystem dst = m_layer->crs();
    if (src.isValid() && dst.isValid() && src != dst) {
      try {
        QgsCoordinateTransform xf(src, dst, QgsProject::instance()
                                                ? QgsProject::instance()->transformContext()
                                                : QgsCoordinateTransformContext());
        xf.setBallparkTransformsAreAppropriate(true);
        if (geom.transform(xf) != Qgis::GeometryOperationResult::Success) {
          m_finishing = false;
          emit captureCanceled();
          return;
        }
      } catch (...) {
        KaCrashGuard::logLine(QStringLiteral("[except] app/KaCaptureMapTool.cpp:431"));
        m_finishing = false;
        emit captureCanceled();
        return;
      }
    }
  }

  m_points.clear();
  destroyRubber();
  m_finishing = false;
  emit geometryCaptured(geom);
}

bool KaCaptureMapTool::hitSavedVertex(const QgsPointXY& mapPt, QgsFeatureId* fid, int* vertex) {
  if (!fid || !vertex || !m_layer || !m_layer->isValid() || !canvas())
    return false;
  const double px = std::max(1e-6, canvas()->mapSettings().mapUnitsPerPixel());
  const double tol2 = (px * 16.0) * (px * 16.0);
  const QgsPointXY layerPt = toLayerCoordinates(m_layer, mapPt);
  QgsFeature f;
  QgsFeatureIterator it = m_layer->getFeatures();
  double best = 1e300;
  QgsFeatureId bestFid = -1;
  int bestV = -1;
  while (it.nextFeature(f)) {
    if (!f.hasGeometry())
      continue;
    int at = -1, before = -1, after = -1;
    double d2 = 0.0;
    const QgsPointXY closest = f.geometry().closestVertex(layerPt, at, before, after, d2);
    if (at >= 0)
      d2 = mapPt.sqrDist(toMapCoordinates(m_layer, closest));
    if (at < 0 || d2 > tol2 || d2 >= best)
      continue;
    best = d2;
    bestFid = f.id();
    bestV = at;
  }
  if (bestFid < 0)
    return false;
  *fid = bestFid;
  *vertex = bestV;
  return true;
}

void KaCaptureMapTool::previewMovedVertex(const QgsPointXY& mapPt) {
  if (!m_layer || m_dragFid < 0 || m_dragVertex < 0 || !canvas())
    return;
  QgsFeature f = m_layer->getFeature(m_dragFid);
  if (!f.isValid() || !f.hasGeometry())
    return;
  QgsGeometry g = f.geometry();
  QgsPointXY layerPt;
  try {
    layerPt = toLayerCoordinates(m_layer, mapPt);
  } catch (const QgsCsException&) {
    cancelVertexDrag();
    emit vertexMoveFailed(QStringLiteral("지도와 도형의 위치를 맞추지 못했습니다. 작업을 저장한 뒤 조사를 다시 열어 주세요."));
    return;
  }
  if (!g.moveVertex(layerPt.x(), layerPt.y(), m_dragVertex))
    return;
  if (!m_rubber) {
    const Qgis::GeometryType gt = m_layer->geometryType();
    m_rubber = new QgsRubberBand(canvas(), gt);
    m_rubber->setStrokeColor(QColor(30, 103, 198));
    m_rubber->setFillColor(QColor(30, 103, 198, 50));
    m_rubber->setWidth(2);
  }
  m_rubber->setToGeometry(g, m_layer);
}

void KaCaptureMapTool::finishVertexDrag(const QgsPointXY& mapPt) {
  if (!m_draggingVertex) return;
  if (!m_layer || m_dragFid < 0 || m_dragVertex < 0) {
    cancelVertexDrag();
    emit vertexMoveFailed(QStringLiteral("꼭짓점을 옮기지 못했습니다. 고칠 도형을 다시 선택해 주세요."));
    return;
  }
  const QPointer<QgsVectorLayer> layer = m_layer;
  const QgsFeatureId fid = m_dragFid;
  const int vertex = m_dragVertex;
  // Finish the gesture before editing/commit signals can invoke another UI action.
  cancelVertexDrag();
  QgsPointXY layerPt;
  try {
    layerPt = toLayerCoordinates(layer, mapPt);
  } catch (const QgsCsException&) {
    emit vertexMoveFailed(QStringLiteral("지도와 도형의 위치를 맞추지 못했습니다. 작업을 저장한 뒤 조사를 다시 열어 주세요."));
    return;
  }
  QString err;
  const bool ok = LayerOps::moveFeatureVertex(layer, static_cast<qint64>(fid), vertex,
                                              layerPt.x(), layerPt.y(), &err);
  if (!ok) {
    emit vertexMoveFailed(QStringLiteral("꼭짓점을 옮기지 못했습니다. %1 고칠 도형을 다시 선택해 주세요.").arg(err));
    return;
  }
  if (!layer) {
    emit vertexMoveFailed(QStringLiteral("꼭짓점을 저장하지 못했습니다. 조사 레이어가 열려 있는지 확인해 주세요."));
    return;
  }
  if (!layer->commitChanges(false)) {
    // QGIS retains the complete edit buffer on failure so the user can retry.
    emit vertexMoveFailed(QStringLiteral("꼭짓점 변경을 저장하지 못했습니다. 편집 내용은 화면에 남아 있습니다. "
                                        "저장 위치의 권한과 여유 공간을 확인한 뒤 Ctrl+S로 다시 저장해 주세요."));
    return;
  }
  if (!layer) return;
  if (layer->geometryType() == Qgis::GeometryType::Polygon)
    LayerOps::applyAreaM2Labels(layer);
  if (layer) layer->triggerRepaint();
  emit vertexMoved();
}

void KaCaptureMapTool::cancelVertexDrag() {
  m_draggingVertex = false;
  m_dragFid = -1;
  m_dragVertex = -1;
  destroyRubber();
}

void KaCaptureMapTool::cancel() {
  m_points.clear();
  cancelVertexDrag();
  destroyRubber();
  m_finishing = false;
  emit captureCanceled();
}
