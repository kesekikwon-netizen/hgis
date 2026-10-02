#include "KaTrenchMoveTool.h"
#include "KaCanvasGridOverlay.h"
#include "core/LayerOps.h"
#include "core/TrenchLayerEdit.h"

#include <algorithm>
#include <cmath>

#include <QKeyEvent>

#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgsmapcanvas.h>
#include <qgsmapmouseevent.h>
#include <qgsrubberband.h>
#include <qgsvectorlayer.h>

namespace {
QString trenchLabel(const QString& name) {
  return name.isEmpty() ? QStringLiteral("트렌치") : name;
}
}  // namespace

KaTrenchMoveTool::KaTrenchMoveTool(QgsMapCanvas* canvas) : QgsMapTool(canvas) {
  setCursor(Qt::ArrowCursor);
}

KaTrenchMoveTool::~KaTrenchMoveTool() {
  disconnect(m_layerWatch);
  delete m_rubber.data();
  delete m_singleRubber.data();
}

void KaTrenchMoveTool::setLayer(QgsVectorLayer* layer) {
  disconnect(m_layerWatch);
  m_layer = layer;
  clearSingleSelection();
  // Ctrl+Z / Ctrl+Y change trenches behind this tool: keep the highlight on the real shape.
  if (layer)
    m_layerWatch = connect(layer, &QgsVectorLayer::layerModified, this, [this]() { refreshSelection(); });
}

void KaTrenchMoveTool::setSnapMeters(double meters) { m_snapM = meters > 0.0 ? meters : 0.0; }

void KaTrenchMoveTool::setGridOverlay(KaCanvasGridOverlay* grid) { m_grid = grid; }

void KaTrenchMoveTool::setMode(Mode mode) {
  m_mode = mode;
  cancelDrag();
  clearSingleSelection();
  setCursor(mode == Mode::Single ? Qt::ArrowCursor : Qt::CrossCursor);
}

QgsPointXY KaTrenchMoveTool::snapPt(const QgsPointXY& p) const {
  if (m_grid && m_grid->isEnabled())
    return m_grid->snapToGrid(p);
  if (!(m_snapM > 1e-9))
    return p;
  return QgsPointXY(std::round(p.x() / m_snapM) * m_snapM,
                    std::round(p.y() / m_snapM) * m_snapM);
}

QgsFeatureId KaTrenchMoveTool::hitTrench(const QgsPointXY& p, QString* nameOut) const {
  if (!m_layer)
    return FID_NULL;
  QgsFeature f;
  QgsFeatureIterator it = m_layer->getFeatures();
  while (it.nextFeature(f)) {
    const QgsGeometry g = f.geometry();
    if (g.isNull())
      continue;
    if (g.boundingBox().contains(p) && g.contains(&p)) {
      if (nameOut)
        *nameOut = f.attribute(QStringLiteral("name")).toString();
      return f.id();
    }
  }
  return FID_NULL;
}

void KaTrenchMoveTool::rebuildRubber(const QgsPointXY& offset) {
  if (!canvas() || !m_layer)
    return;
  if (!m_rubber) {
    m_rubber = new QgsRubberBand(canvas(), Qgis::GeometryType::Polygon);
    m_rubber->setWidth(2);
    m_rubber->setColor(QColor(220, 38, 38));
    m_rubber->setFillColor(QColor(220, 38, 38, 40));
  }
  m_rubber->reset(Qgis::GeometryType::Polygon);
  QgsFeatureIterator it = m_layer->getFeatures();
  QgsFeature f;
  bool first = true;
  while (it.nextFeature(f)) {
    QgsGeometry g = f.geometry();
    if (g.isNull())
      continue;
    g.translate(offset.x(), offset.y());
    m_rubber->addGeometry(g, nullptr, first);
    first = false;
  }
}

void KaTrenchMoveTool::rebuildSingleRubber(QgsFeatureId fid, const QgsPointXY& offset) {
  if (!canvas() || !m_layer || FID_IS_NULL(fid))
    return;
  if (!m_singleRubber) {
    m_singleRubber = new QgsRubberBand(canvas(), Qgis::GeometryType::Polygon);
    m_singleRubber->setWidth(3);
    m_singleRubber->setColor(QColor(30, 103, 198));
    m_singleRubber->setFillColor(QColor(30, 103, 198, 60));
  }
  m_singleRubber->reset(Qgis::GeometryType::Polygon);
  QgsFeature f = m_layer->getFeature(fid);
  if (!f.isValid() || !f.hasGeometry())
    return;
  QgsGeometry g = f.geometry();
  g.translate(offset.x(), offset.y());
  m_singleRubber->addGeometry(g, nullptr, true);
}

void KaTrenchMoveTool::cancelDrag() {
  m_dragging = false;
  m_dragSingle = false;
  if (m_rubber)
    m_rubber->reset(Qgis::GeometryType::Polygon);
}

void KaTrenchMoveTool::clearSingleSelection() {
  m_selFid = FID_NULL;
  m_selName.clear();
  if (m_singleRubber)
    m_singleRubber->reset(Qgis::GeometryType::Polygon);
}

void KaTrenchMoveTool::refreshSelection() {
  if (FID_IS_NULL(m_selFid) || m_dragSingle)
    return;
  if (!m_layer || !m_layer->getFeature(m_selFid).isValid()) {
    clearSingleSelection();
    return;
  }
  rebuildSingleRubber(m_selFid, QgsPointXY(0, 0));
}

void KaTrenchMoveTool::afterEdit() {
  if (canvas()) LayerOps::refreshCanvasIfIdle(canvas());  // never cancels a WMS job in flight
  emit trenchesEdited();
}

bool KaTrenchMoveTool::applyTranslate(const QgsFeatureIds& fids, double dx, double dy) {
  if (!m_layer || (dx == 0.0 && dy == 0.0))
    return false;
  QString error;
  const QString title = fids.isEmpty() ? QStringLiteral("시굴격자 전체 이동") : QStringLiteral("트렌치 이동");
  if (!TrenchLayerEdit::translate(m_layer, fids, dx, dy, title, &error)) {
    emit statusMessage(error.isEmpty() ? QStringLiteral("트렌치를 옮기지 못했습니다. 기존 배치는 유지됩니다.")
                                       : error);
    return false;
  }
  afterEdit();
  return true;
}

void KaTrenchMoveTool::deleteTrench(QgsFeatureId fid, const QString& name) {
  if (!m_layer || FID_IS_NULL(fid))
    return;
  QString error;
  if (!TrenchLayerEdit::deleteTrench(m_layer, fid, &error)) {
    emit statusMessage(error.isEmpty() ? QStringLiteral("트렌치를 지우지 못했습니다.") : error);
    return;
  }
  if (m_selFid == fid)
    clearSingleSelection();
  afterEdit();
  emit statusMessage(QStringLiteral("%1 삭제 — Ctrl+Z로 되돌릴 수 있습니다. 남은 트렌치 %2개.")
                         .arg(trenchLabel(name))
                         .arg(m_layer->featureCount()));
}

bool KaTrenchMoveTool::deleteSelectedTrench() {
  if (FID_IS_NULL(m_selFid)) return false;
  deleteTrench(m_selFid, m_selName);
  return true;
}

void KaTrenchMoveTool::canvasPressEvent(QgsMapMouseEvent* e) {
  if (!e) return;
  if (e->button() == Qt::RightButton) {
    // 우클릭 = 그 트렌치 하나만 바로 삭제(양쪽 모드 공통). 확인 창 없이 Ctrl+Z로 되돌린다.
    cancelDrag();
    QString name;
    const QgsFeatureId fid = hitTrench(e->mapPoint(), &name);
    if (FID_IS_NULL(fid)) {
      emit statusMessage(QStringLiteral("지울 트렌치 위에서 우클릭하세요."));
      return;
    }
    deleteTrench(fid, name);
    return;
  }
  if (e->button() != Qt::LeftButton)
    return;
  const QgsPointXY click = e->mapPoint();

  if (m_mode == Mode::Single) {
    QString name;
    const QgsFeatureId fid = hitTrench(click, &name);
    if (FID_IS_NULL(fid)) {
      clearSingleSelection();
      emit statusMessage(QStringLiteral("트렌치를 클릭해 선택하세요. 끌면 이동, Delete·우클릭이면 삭제됩니다."));
      return;
    }
    m_selFid = fid;
    m_selName = name;
    // Same snapped reference as the drop point, so the move is a whole number of grid steps.
    m_from = snapPt(click);
    m_dragSingle = true;
    rebuildSingleRubber(fid, QgsPointXY(0, 0));
    emit statusMessage(QStringLiteral("%1 선택 — 끌어서 이동, Delete = 삭제").arg(trenchLabel(name)));
    return;
  }

  m_from = snapPt(click);
  m_dragging = true;
  rebuildRubber(QgsPointXY(0, 0));
  emit statusMessage(QStringLiteral("격자를 끌어 옮기세요. 놓으면 전체가 이동합니다."));
}

void KaTrenchMoveTool::canvasMoveEvent(QgsMapMouseEvent* e) {
  if (!e) return;
  if (m_mode == Mode::Single) {
    if (!m_dragSingle || FID_IS_NULL(m_selFid))
      return;
    const QgsPointXY dest = snapPt(e->mapPoint());
    rebuildSingleRubber(m_selFid, QgsPointXY(dest.x() - m_from.x(), dest.y() - m_from.y()));
    return;
  }
  if (m_dragging) {
    const QgsPointXY dest = snapPt(e->mapPoint());
    rebuildRubber(QgsPointXY(dest.x() - m_from.x(), dest.y() - m_from.y()));
  }
}

void KaTrenchMoveTool::canvasReleaseEvent(QgsMapMouseEvent* e) {
  if (!e || e->button() != Qt::LeftButton)
    return;
  const QgsPointXY dest = snapPt(e->mapPoint());
  const double dx = dest.x() - m_from.x();
  const double dy = dest.y() - m_from.y();
  const double mupp = canvas() ? std::max(canvas()->mapUnitsPerPixel(), 1e-6) : 1.0;
  if (m_mode == Mode::Whole && m_dragging) {
    m_dragging = false;
    const bool moved = std::hypot(dx, dy) > 2.0 * mupp && applyTranslate({}, dx, dy);
    if (m_rubber)
      m_rubber->reset(Qgis::GeometryType::Polygon);
    if (moved)
      emit statusMessage(QStringLiteral("격자 이동 완료 — Ctrl+Z로 되돌립니다. 다시 끌거나 우클릭으로 하나씩 지우세요."));
    return;
  }
  if (m_mode != Mode::Single || !m_dragSingle || FID_IS_NULL(m_selFid))
    return;
  m_dragSingle = false;
  if (std::hypot(dx, dy) > 2.0 * mupp && applyTranslate({m_selFid}, dx, dy)) {
    emit statusMessage(QStringLiteral("%1 이동 완료 — Ctrl+Z로 되돌립니다. Delete = 삭제, 다른 트렌치 클릭 = 선택 변경")
                           .arg(trenchLabel(m_selName)));
  }
  rebuildSingleRubber(m_selFid, QgsPointXY(0, 0));
}

void KaTrenchMoveTool::keyPressEvent(QKeyEvent* e) {
  if (e && (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) && deleteSelectedTrench()) {
    e->accept();
    return;
  }
  if (e && e->key() == Qt::Key_Escape) {
    cancelDrag();
    clearSingleSelection();
    e->accept();
    return;
  }
  QgsMapTool::keyPressEvent(e);
}

void KaTrenchMoveTool::activate() {
  QgsMapTool::activate();
  cancelDrag();
  const bool single = m_mode == Mode::Single;
  setCursor(single ? Qt::ArrowCursor : Qt::CrossCursor);
  emit statusMessage(single
      ? QStringLiteral("개별 편집: 트렌치 클릭 = 선택, 끌기 = 이동, Delete·우클릭 = 삭제 (Ctrl+Z로 되돌림)")
      : QStringLiteral("전체 이동: 격자를 끌어 옮기세요. 우클릭 = 개별 삭제 (Ctrl+Z로 되돌림)"));
}

void KaTrenchMoveTool::deactivate() {
  cancelDrag();
  clearSingleSelection();
  QgsMapTool::deactivate();
}
