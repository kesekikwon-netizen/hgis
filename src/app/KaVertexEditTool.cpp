#include "KaCrashGuard.h"
#include "KaVertexEditTool.h"
#include "core/LayerOps.h"

#include <cmath>
#include <limits>

#include <QAction>
#include <QKeyEvent>
#include <QMenu>

#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgsgeometry.h>
#include <qgsmapcanvas.h>
#include <qgsproject.h>
#include <qgspoint.h>
#include <qgspointlocator.h>
#include <qgsrubberband.h>
#include <qgssnappingutils.h>
#include <qgsvectorlayer.h>
#include <qgsvertexid.h>
#include <qgsvertexmarker.h>
#include <qgswkbtypes.h>

KaVertexEditTool::KaVertexEditTool(QgsMapCanvas* canvas) : QgsMapTool(canvas) {
  setCursor(Qt::ArrowCursor);
}

KaVertexEditTool::~KaVertexEditTool() {
  clearSelection();
  delete m_outline;
}

void KaVertexEditTool::setLayer(QgsVectorLayer* layer) {
  if (m_layer == layer) return;
  clearSelection();
  m_layer = layer;
}

// 선택 도구가 고른 도형을 그대로 편집 대상으로 삼는다. 도형을 고르면 곧바로
// 수정점이 보여야 하므로 여기서 마커까지 띄운다.
void KaVertexEditTool::setTarget(QgsVectorLayer* layer, QgsFeatureId fid) {
  if (m_layer == layer && m_fid == fid) {
    showVertexMarkers();
    return;
  }
  clearSelection();
  m_layer = layer;
  m_fid = fid;
  if (m_layer && !FID_IS_NULL(m_fid)) showVertexMarkers();
}

void KaVertexEditTool::clearTarget() {
  clearSelection();
}

void KaVertexEditTool::previewVertexMove(int index, const QgsPointXY& toLayerPt) {
  if (index < 0) return;
  QgsGeometry geom = selectedGeometry();
  if (geom.isNull()) return;
  // 면의 첫 점과 닫는 점은 같은 자리여야 한다. 하나를 옮기면 같은 고리의 짝도 옮긴다.
  const int partner = LayerOps::ringClosingVertex(geom, index);
  geom.moveVertex(toLayerPt.x(), toLayerPt.y(), index);
  if (partner >= 0) geom.moveVertex(toLayerPt.x(), toLayerPt.y(), partner);
  refreshRubber(geom);
  if (index < m_marks.size() && m_marks[index])
    m_marks[index]->setCenter(toMap(toLayerPt));
}

void KaVertexEditTool::activate() {
  if (canvas()) {
    m_savedMenuPolicy = canvas()->contextMenuPolicy();
    canvas()->setContextMenuPolicy(Qt::PreventContextMenu);
  }
  QgsMapTool::activate();
  emit statusMessage(
      QStringLiteral("도형을 클릭한 뒤 꼭짓점을 끌어 고치세요. 선에서 우클릭하면 "
                     "점추가·점삭제, Delete는 점 삭제입니다."));
}

void KaVertexEditTool::deactivate() {
  if (canvas())
    canvas()->setContextMenuPolicy(m_savedMenuPolicy);
  clearSelection();
  QgsMapTool::deactivate();
}

double KaVertexEditTool::mapTolerance(int px) const {
  if (!mCanvas) return 0.0;
  return mCanvas->mapUnitsPerPixel() * px;
}

// 도형은 레이어 CRS로 저장돼 있고 마우스는 지도 CRS로 들어온다. 둘이 다르면
// (작업 5187 · 레이어 5186 등) 지도 좌표로 그냥 재면 아무 도형도 안 잡히고,
// 꼭짓점을 옮기면 엉뚱한 곳으로 날아간다. 반드시 레이어 좌표로 바꿔서 다룬다.
QgsPointXY KaVertexEditTool::toLayer(const QgsPointXY& mapPt) const {
  if (!m_layer) return mapPt;
  return const_cast<KaVertexEditTool*>(this)->toLayerCoordinates(m_layer.data(), mapPt);
}

QgsPointXY KaVertexEditTool::toMap(const QgsPointXY& layerPt) const {
  if (!m_layer) return layerPt;
  return const_cast<KaVertexEditTool*>(this)->toMapCoordinates(m_layer.data(), layerPt);
}

// 화면 px 허용오차를 레이어 단위로 환산한다.
double KaVertexEditTool::layerTolerance(int px) const {
  if (!mCanvas) return 0.0;
  const double d = mapTolerance(px);
  if (!m_layer) return d;
  const QgsPointXY c = mCanvas->extent().center();
  const QgsPointXY a = toLayer(c);
  const QgsPointXY b = toLayer(QgsPointXY(c.x() + d, c.y()));
  const double scaled = std::hypot(b.x() - a.x(), b.y() - a.y());
  return scaled > 0.0 ? scaled : d;
}

QgsGeometry KaVertexEditTool::selectedGeometry() const {
  if (!m_layer || FID_IS_NULL(m_fid)) return QgsGeometry();
  QgsFeature f;
  if (!m_layer->getFeatures(QgsFeatureRequest(m_fid)).nextFeature(f)) return QgsGeometry();
  return f.geometry();
}

void KaVertexEditTool::clearSelection() {
  qDeleteAll(m_marks);
  m_marks.clear();
  if (m_outline) {
    delete m_outline;
    m_outline = nullptr;
  }
  // 없음은 FID_NULL 로 적는다. 그려 놓고 아직 저장하지 않은 도형은 번호가 음수라서,
  // 음수를 「없음」으로 읽으면 그런 도형은 수정점이 나오지 않는다.
  m_fid = FID_NULL;
  m_dragIndex = -1;
  m_dragging = false;
}

void KaVertexEditTool::refreshRubber(const QgsGeometry& geom) {
  if (!mCanvas) return;
  if (!m_outline) {
    m_outline = new QgsRubberBand(mCanvas, geom.type());
    m_outline->setColor(QColor(30, 103, 198, 200));
    m_outline->setWidth(2);
    m_outline->setFillColor(QColor(30, 103, 198, 40));
  }
  m_outline->setToGeometry(geom, m_layer);
}

void KaVertexEditTool::showVertexMarkers() {
  qDeleteAll(m_marks);
  m_marks.clear();
  if (!mCanvas) return;
  const QgsGeometry geom = selectedGeometry();
  if (geom.isNull()) return;
  refreshRubber(geom);

  int i = 0;
  for (auto it = geom.vertices_begin(); it != geom.vertices_end(); ++it, ++i) {
    const QgsPoint p = *it;
    auto* m = new QgsVertexMarker(mCanvas);
    m->setIconType(QgsVertexMarker::ICON_BOX);
    m->setIconSize(14);
    m->setPenWidth(2);
    m->setColor(QColor(30, 103, 198));
    m->setFillColor(QColor(255, 255, 255));
    m->setCenter(toMap(QgsPointXY(p.x(), p.y())));
    m_marks.append(m);
  }
}

QgsPointXY KaVertexEditTool::snapMapPoint(QgsMapMouseEvent* e, bool* snapped) const {
  if (snapped) *snapped = false;
  QgsPointXY pt;
  if (!e || !mCanvas) return QgsPointXY();
  try {
    pt = e->mapPoint();
  } catch (...) {
    KaCrashGuard::logLine(QStringLiteral("[except] app/KaVertexEditTool.cpp:178"));
    pt = const_cast<KaVertexEditTool*>(this)->toMapCoordinates(e->pos());
  }
  if (!m_snapEnabled || !mCanvas->snappingUtils()) return pt;
  const QgsPointLocator::Match hit = mCanvas->snappingUtils()->snapToMap(e->pos());
  if (hit.isValid()) {
    if (snapped) *snapped = true;
    return hit.point();
  }
  return pt;
}

int KaVertexEditTool::vertexNear(const QgsPointXY& mapPt_, int tolPx) const {
  const QgsGeometry geom = selectedGeometry();
  if (geom.isNull()) return -1;
  const QgsPointXY mapPt = toLayer(mapPt_);
  const double tol = layerTolerance(tolPx);
  int best = -1;
  double bestD = tol;
  int i = 0;
  for (auto it = geom.vertices_begin(); it != geom.vertices_end(); ++it, ++i) {
    const QgsPoint p = *it;
    const double d = std::hypot(p.x() - mapPt.x(), p.y() - mapPt.y());
    if (d <= bestD) {
      bestD = d;
      best = i;
    }
  }
  return best;
}

int KaVertexEditTool::segmentNear(const QgsPointXY& mapPt_, QgsPointXY* onLine, int tolPx) const {
  const QgsGeometry geom = selectedGeometry();
  if (geom.isNull()) return -1;
  const QgsPointXY mapPt = toLayer(mapPt_);
  QgsPointXY closest;
  int afterVertex = -1;
  const double sqrDist = geom.closestSegmentWithContext(mapPt, closest, afterVertex);
  if (sqrDist < 0.0) return -1;
  const double tol = layerTolerance(tolPx);
  if (std::sqrt(sqrDist) > tol) return -1;
  if (onLine) *onLine = closest;
  return afterVertex;
}

void KaVertexEditTool::selectAt(const QgsPointXY& mapPt_) {
  const auto editable = [](QgsVectorLayer* layer) {
    if (!layer || !layer->isValid() || layer->readOnly()) return false;
    const auto type = layer->geometryType();
    if (type != Qgis::GeometryType::Line && type != Qgis::GeometryType::Polygon) return false;
    const QString key = LayerOps::layerKeyOf(layer);
    return key == QLatin1String("survey_area") || key == QLatin1String("feature_poly") ||
           key == QLatin1String("feature_line") || key == QLatin1String("section_line") ||
           key == QLatin1String("trial_trench");
  };
  QList<QgsVectorLayer*> layers;
  if (editable(m_layer)) layers.append(m_layer);
  if (QgsProject::instance()) {
    const auto found = QgsProject::instance()->layers<QgsVectorLayer*>();
    for (QgsVectorLayer* layer : found)
      if (editable(layer) && !layers.contains(layer)) layers.append(layer);
  }
  if (layers.isEmpty()) {
    emit statusMessage(QStringLiteral("수정할 조사 도형이 없습니다."));
    return;
  }
  QgsVectorLayer* hitLayer = nullptr;
  QgsFeatureId hit = FID_NULL;
  double bestD = std::numeric_limits<double>::max();
  for (QgsVectorLayer* layer : layers) {
    m_layer = layer;
    const QgsPointXY mapPt = toLayer(mapPt_);
    const double tol = layerTolerance(10);
    const QgsRectangle box(mapPt.x() - tol, mapPt.y() - tol, mapPt.x() + tol, mapPt.y() + tol);
    QgsFeatureRequest req;
    req.setFilterRect(box);
    QgsFeatureIterator it = layer->getFeatures(req);
    QgsFeature f;
    const QgsGeometry probe = QgsGeometry::fromPointXY(mapPt);
    while (it.nextFeature(f)) {
      if (!f.hasGeometry()) continue;
      const double d = f.geometry().distance(probe);
      if (d <= tol && d < bestD) {
        bestD = d;
        hit = f.id();
        hitLayer = layer;
      }
    }
  }
  if (FID_IS_NULL(hit) || !hitLayer) {
    if (layers.size() == 1) m_layer = layers.first();
    clearSelection();
    emit statusMessage(QStringLiteral("도형을 찾지 못했습니다. 선이나 면 위를 클릭하세요."));
    return;
  }
  clearSelection();
  m_layer = hitLayer;
  m_fid = hit;
  showVertexMarkers();
  emit statusMessage(
      QStringLiteral("도형 선택 — 꼭짓점 %1개. 점을 끌어 옮기세요.").arg(m_marks.size()));
}

bool KaVertexEditTool::moveVertexTo(int index, const QgsPointXY& to) {
  m_lastEditError.clear();
  if (!m_layer || FID_IS_NULL(m_fid) || index < 0) return false;
  QgsFeature before;
  if (!m_layer->getFeatures(QgsFeatureRequest(m_fid)).nextFeature(before)) {
    m_lastEditError = QStringLiteral("수정할 도형을 찾지 못했습니다. 도형을 다시 선택하세요.");
    emit statusMessage(m_lastEditError);
    return false;
  }
  const bool topological = m_layer->project() && m_layer->project()->topologicalEditing();
  QString error;
  if (!LayerOps::runEditCommand(m_layer, QStringLiteral("꼭짓점 이동"), [&]() {
        return LayerOps::applyVertexMove(m_layer, static_cast<qint64>(m_fid), index, to.x(),
                                         to.y(), topological, nullptr);
      }, &error)) {
    m_lastEditError = error;
    emit statusMessage(error);
    return false;
  }
  emit featureGeometryEdited(m_layer, before);
  return true;
}

bool KaVertexEditTool::deleteVertexAt(int index) {
  m_lastEditError.clear();
  if (!m_layer || FID_IS_NULL(m_fid) || index < 0) return false;
  QgsGeometry geom = selectedGeometry();
  if (geom.isNull() || !geom.constGet()) return false;
  // 구멍이 있는 면은 점이 든 고리 하나만 센다. 전체로 세면 세모난 바깥 고리에서도 점이 지워진다.
  QgsVertexId ring;
  const int count = geom.vertexIdFromVertexNr(index, ring)
                        ? geom.constGet()->vertexCount(ring.part, ring.ring)
                        : static_cast<int>(geom.constGet()->nCoordinates());
  // 면은 닫는 점을 빼고 3점, 선은 2점이 최소다. 그 아래로는 도형이 깨진다.
  const int minCount = geom.type() == Qgis::GeometryType::Polygon ? 5 : 3;
  if (count < minCount) {
    emit statusMessage(geom.type() == Qgis::GeometryType::Polygon
                           ? QStringLiteral("면은 꼭짓점 3개보다 줄일 수 없습니다.")
                           : QStringLiteral("선은 꼭짓점 2개보다 줄일 수 없습니다."));
    return false;
  }
  if (!geom.deleteVertex(index)) return false;
  return applyGeometryChange(geom, QStringLiteral("꼭짓점 삭제"));
}

bool KaVertexEditTool::insertVertexAt(int index, const QgsPointXY& at) {
  m_lastEditError.clear();
  if (!m_layer || FID_IS_NULL(m_fid) || index < 0) return false;
  QgsGeometry geom = selectedGeometry();
  if (geom.isNull()) return false;
  if (!geom.insertVertex(at.x(), at.y(), index)) return false;
  return applyGeometryChange(geom, QStringLiteral("꼭짓점 추가"));
}

bool KaVertexEditTool::applyGeometryChange(QgsGeometry geom, const QString& commandText) {
  const QPointer<QgsVectorLayer> layer = m_layer;
  if (!layer) return false;
  const auto fail = [this](const QString& message) {
    m_lastEditError = message;
    emit statusMessage(message);
    return false;
  };
  // A command already owned by another operation must not be ended or discarded.
  if (layer->isEditCommandActive())
    return fail(QStringLiteral("다른 도형 편집이 진행 중입니다. 편집을 마친 뒤 다시 시도하세요."));

  QgsFeature before;
  if (!layer->getFeatures(QgsFeatureRequest(m_fid)).nextFeature(before))
    return fail(QStringLiteral("수정할 도형을 찾지 못했습니다. 도형을 다시 선택하세요."));
  if (before.geometry().equals(geom)) return true;
  QString error;
  if (!LayerOps::runEditCommand(layer, commandText, [&]() {
        return layer->changeGeometry(before.id(), geom);
      }, &error))
    return fail(error);
  if (!layer) return false;
  emit featureGeometryEdited(layer.data(), before);
  return true;
}

void KaVertexEditTool::showLineVertexMenu(QgsMapMouseEvent* e) {
  if (!e || !m_layer || !mCanvas) return;
  const QgsPointXY mapPt = snapMapPoint(e);
  if (FID_IS_NULL(m_fid))
    selectAt(mapPt);
  const int vIdx = vertexNear(mapPt, 20);
  QgsPointXY onLine;
  const int segAfter = segmentNear(mapPt, &onLine, 16);
  const int nearDel = vIdx >= 0 ? vIdx : vertexNear(mapPt, 28);
  if (FID_IS_NULL(m_fid) && vIdx < 0 && segAfter < 0) {
    emit statusMessage(QStringLiteral("선이나 면 위에서 우클릭하세요."));
    return;
  }

  QMenu menu;
  QAction* addAct = menu.addAction(QStringLiteral("점추가"));
  QAction* delAct = menu.addAction(QStringLiteral("점삭제"));
  addAct->setEnabled(!FID_IS_NULL(m_fid) && segAfter >= 0);
  delAct->setEnabled(!FID_IS_NULL(m_fid) && nearDel >= 0);
  QAction* chosen = menu.exec(mCanvas->mapToGlobal(e->pos()));
  if (!chosen) return;
  if (chosen == addAct && segAfter >= 0 && insertVertexAt(segAfter, onLine)) {
    showVertexMarkers();
    emit statusMessage(QStringLiteral("꼭짓점을 넣었습니다."));
    return;
  }
  if (chosen == delAct && nearDel >= 0 && deleteVertexAt(nearDel)) {
    showVertexMarkers();
    emit statusMessage(QStringLiteral("꼭짓점을 지웠습니다."));
  }
}

void KaVertexEditTool::canvasPressEvent(QgsMapMouseEvent* e) {
  if (!e) return;
  const QgsPointXY mapPt = snapMapPoint(e);

  if (e->button() == Qt::RightButton) {
    showLineVertexMenu(e);
    return;
  }
  if (e->button() != Qt::LeftButton) return;

  if (!FID_IS_NULL(m_fid)) {
    const int idx = vertexNear(mapPt, 20);
    if (idx >= 0) {
      m_dragIndex = idx;
      m_dragging = true;
      return;  // 끌기 시작 — 선택을 바꾸지 않는다
    }
  }
  selectAt(mapPt);
}

void KaVertexEditTool::canvasMoveEvent(QgsMapMouseEvent* e) {
  if (!e) return;
  if (!m_dragging || m_dragIndex < 0) return;
  // 끄는 동안에는 고무줄과 표시만 움직인다. 커밋은 놓을 때 한 번.
  previewVertexMove(m_dragIndex, toLayer(snapMapPoint(e)));
}

void KaVertexEditTool::canvasReleaseEvent(QgsMapMouseEvent* e) {
  if (!e || !m_dragging) return;
  m_dragging = false;
  const int idx = m_dragIndex;
  m_dragIndex = -1;
  if (idx < 0) return;
  if (moveVertexTo(idx, toLayer(snapMapPoint(e)))) {
    showVertexMarkers();
    emit statusMessage(QStringLiteral("꼭짓점을 옮겼습니다. Ctrl+Z로 되돌릴 수 있습니다."));
  } else {
    showVertexMarkers();
    emit statusMessage(m_lastEditError.isEmpty() ? QStringLiteral("꼭짓점을 옮기지 못했습니다.")
                                               : m_lastEditError);
  }
}

void KaVertexEditTool::canvasDoubleClickEvent(QgsMapMouseEvent* e) {
  if (!e || FID_IS_NULL(m_fid) || e->button() != Qt::LeftButton) return;
  QgsPointXY onLine;
  const int after = segmentNear(snapMapPoint(e), &onLine);
  if (after < 0) return;
  if (insertVertexAt(after, onLine)) {
    showVertexMarkers();
    emit statusMessage(QStringLiteral("꼭짓점을 넣었습니다."));
  }
}

void KaVertexEditTool::keyPressEvent(QKeyEvent* e) {
  if (!e) return;
  if (e->key() == Qt::Key_Escape) {
    clearSelection();
    emit statusMessage(QStringLiteral("선택을 풀었습니다."));
    return;
  }
  if (e->key() != Qt::Key_Delete && e->key() != Qt::Key_Backspace) return;
  if (FID_IS_NULL(m_fid) || !mCanvas) return;
  // 마우스가 얹힌 꼭짓점을 지운다.
  const QPoint cursor = mCanvas->mapFromGlobal(QCursor::pos());
  const QgsPointXY mapPt = mCanvas->getCoordinateTransform()->toMapCoordinates(cursor);
  const int idx = vertexNear(mapPt);
  if (idx >= 0 && deleteVertexAt(idx)) {
    showVertexMarkers();
    emit statusMessage(QStringLiteral("꼭짓점을 지웠습니다."));
  }
}
