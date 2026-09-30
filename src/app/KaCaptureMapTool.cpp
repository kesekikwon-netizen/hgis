#include "KaCrashGuard.h"
#include "core/KaLogExcept.h"
#include "KaCaptureMapTool.h"
#include "KaEditTolerance.h"
#include "core/EditGeometryRepair.h"
#include <qgsmapcanvas.h>
#include <qgsvectorlayer.h>
#include <qgsrubberband.h>
#include <qgsvertexmarker.h>
#include <qgssnappingutils.h>
#include <qgspointlocator.h>
#include <qgsgeometry.h>
#include <qgscoordinatetransform.h>
#include <qgsproject.h>
#include <qgsmapsettings.h>
#include <QKeyEvent>
#include <QKeySequence>
#include <QColor>
#include <QTimer>
#include <QWidget>
#include <algorithm>
#include <cmath>

KaCaptureMapTool::KaCaptureMapTool(QgsMapCanvas* canvas)
    : QgsMapTool(canvas) {
  setCursor(Qt::CrossCursor);
  m_dwell = new QTimer(this);
  m_dwell->setSingleShot(true);
  m_dwell->setInterval(KaEditTolerance::kEasyDrawDwellMs);
  connect(m_dwell, &QTimer::timeout, this, &KaCaptureMapTool::addDwellPoint);
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
  m_layer = layer;
}

void KaCaptureMapTool::setSnapEnabled(bool on) {
  m_snapEnabled = on;
  if (!on) {
    stopDwell();
    destroySnapMarker();
  }
}

void KaCaptureMapTool::setEasyDraw(bool on) {
  m_easyDraw = on;
  if (on) {
    m_snapEnabled = true;
    setCursor(Qt::CrossCursor);
  } else {
    stopDwell();
  }
}

void KaCaptureMapTool::resetSession() {
  m_finishing = false;
  m_points.clear();
  stopDwell();
  destroyRubber();
  announceSketch();
}

void KaCaptureMapTool::announceSketch() {
  if (m_announcedCount == m_points.size()) return;
  m_announcedCount = m_points.size();
  emit sketchChanged(m_announcedCount);
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
    if (mupp > 0) tol = std::max(mupp * KaEditTolerance::kSketchVertexPx, 0.05);
  }
  return a.sqrDist(b) <= tol * tol;
}

int KaCaptureMapTool::indexOfSketchVertex(const QgsPointXY& pt) const {
  for (int i = 0; i < m_points.size(); ++i) {
    if (nearPoint(m_points.at(i), pt)) return i;
  }
  return -1;
}

bool KaCaptureMapTool::mapPointFromEvent(QgsMapMouseEvent* e, QgsPointXY* out, bool* snappedOut,
                                         bool* onVertexOut) {
  if (!e || !out || !canvas()) return false;
  bool snapped = false;
  bool isInter = false;
  bool onVertex = false;
  try {
    *out = e->mapPoint();
  } catch (...) {
    KA_LOG_EXCEPT();
    try {
      *out = toMapCoordinates(e->pos());
    } catch (...) {
      KA_LOG_EXCEPT();
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
      // Intersections come back as vertex matches too; only an edge match is "on a line".
      onVertex = hit.hasVertex() || hit.hasLineEndpoint();
    }
  }
  if (!std::isfinite(out->x()) || !std::isfinite(out->y())) return false;
  updateSnapMarker(*out, snapped, isInter);
  if (snappedOut) *snappedOut = snapped;
  if (onVertexOut) *onVertexOut = onVertex;
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
  announceSketch();
}

void KaCaptureMapTool::deactivate() {
  if (canvas())
    canvas()->setContextMenuPolicy(m_savedMenuPolicy);
  stopDwell();
  destroyRubber();
  destroySnapMarker();
  if (!m_finishing)
    m_points.clear();
  m_finishing = false;
  // No signal here: the canvas also deactivates its tool while the window is being torn
  // down. Whoever switched tools hears it through QgsMapCanvas::mapToolSet.
  m_announcedCount = m_points.size();
  QgsMapTool::deactivate();
}

void KaCaptureMapTool::canvasPressEvent(QgsMapMouseEvent* e) {
  if (!e || !canvas() || m_finishing) return;
  stopDwell();

  if (e->button() == Qt::RightButton) {
    e->accept();
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
    if (m_easyDraw && !m_points.isEmpty()) {
      const int idx = indexOfSketchVertex(mapPt);
      if (idx >= 0) {
        m_points.resize(idx + 1);
        rebuildRubber(&mapPt);
        announceSketch();
        return;
      }
    }
    if (m_points.isEmpty() || !nearPoint(m_points.last(), mapPt))
      m_points.append(mapPt);
    rebuildRubber(&mapPt);
    announceSketch();
  }
}

void KaCaptureMapTool::canvasReleaseEvent(QgsMapMouseEvent* e) {
  // Right-click finishes on press; the release is only kept away from the canvas.
  if (e && e->button() == Qt::RightButton) e->accept();
}

void KaCaptureMapTool::canvasDoubleClickEvent(QgsMapMouseEvent* e) {
  if (!e || m_finishing || m_mode == Mode::Point) return;
  e->accept();
  stopDwell();
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
  bool onVertex = false;
  if (!mapPointFromEvent(e, &mapPt, &snapped, &onVertex)) return;
  if (m_points.isEmpty() || m_mode == Mode::Point) {
    stopDwell();
    return;
  }
  if (m_easyDraw && snapped && indexOfSketchVertex(mapPt) < 0) {
    if (onVertex) {
      // A corner of another shape is an unambiguous point: take it right away.
      stopDwell();
      m_points.append(mapPt);
      announceSketch();
    } else {
      // Sweeping along an edge must not drop a point every few pixels.
      startDwell(mapPt);
    }
  } else {
    stopDwell();
  }
  rebuildRubber(&mapPt);
}

void KaCaptureMapTool::startDwell(const QgsPointXY& mapPt) {
  m_dwellPoint = mapPt;
  m_dwellValid = true;
  if (m_dwell) m_dwell->start();
}

void KaCaptureMapTool::stopDwell() {
  m_dwellValid = false;
  if (m_dwell) m_dwell->stop();
}

void KaCaptureMapTool::addDwellPoint() {
  if (!m_dwellValid || !m_easyDraw || m_finishing || m_points.isEmpty()) return;
  m_dwellValid = false;
  if (indexOfSketchVertex(m_dwellPoint) >= 0) return;
  m_points.append(m_dwellPoint);
  rebuildRubber(&m_dwellPoint);
  announceSketch();
}

bool KaCaptureMapTool::undoLastVertex() {
  if (m_finishing || m_points.isEmpty()) return false;
  stopDwell();
  m_points.removeLast();
  if (m_points.isEmpty())
    destroyRubber();
  else if (canvas()) {
    const QgsPointXY cur = toMapCoordinates(canvas()->mouseLastXY());
    rebuildRubber(&cur);
  }
  announceSketch();
  return true;
}

void KaCaptureMapTool::finishSketch() {
  finish();
}

void KaCaptureMapTool::cancelSketch() {
  cancel();
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
  stopDwell();
  m_repairNotice.clear();

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
    double spacing = 0.15;
    if (canvas() && canvas()->mapUnitsPerPixel() > 0)
      spacing = std::max(canvas()->mapUnitsPerPixel() * KaEditTolerance::kSketchVertexPx, 0.05);
    const EditGeometryRepair::PolygonResult closed =
        EditGeometryRepair::closePolygon(m_points, spacing);
    geom = closed.geometry;
    ok = !geom.isEmpty() && geom.type() == Qgis::GeometryType::Polygon;
    if (ok) m_repairNotice = EditGeometryRepair::notice(closed);
  }

  if (!ok) {
    m_finishing = false;
    m_repairNotice.clear();
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
          m_repairNotice.clear();
          emit captureCanceled();
          return;
        }
      } catch (...) {
        KA_LOG_EXCEPT();
        m_finishing = false;
        m_repairNotice.clear();
        emit captureCanceled();
        return;
      }
    }
  }

  m_points.clear();
  destroyRubber();
  m_finishing = false;
  announceSketch();
  emit geometryCaptured(geom);
}

void KaCaptureMapTool::cancel() {
  const bool hadSketch = !m_points.isEmpty();
  m_points.clear();
  stopDwell();
  destroyRubber();
  m_finishing = false;
  announceSketch();
  if (hadSketch) emit sketchCanceled();
}
