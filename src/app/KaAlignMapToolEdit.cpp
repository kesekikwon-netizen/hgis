// Point dragging and 「되돌리기」 for the alignment tools (split from KaAlignMapTool.cpp).
#include "KaAlignMapTool.h"
#include "core/LayerOps.h"

#include <qgsgeometry.h>
#include <qgsmapcanvas.h>
#include <qgsmapmouseevent.h>
#include <qgsmaptopixel.h>
#include <qgsrasterlayer.h>
#include <qgsvectorlayer.h>
#include <qgsvertexmarker.h>
#include <QDir>
#include <cmath>

namespace {

constexpr double kGrabPixels = 10.0;
constexpr int kDragSlopPixels = 3;

// Index of the point drawn nearest to `screen` within kGrabPixels, or -1.
int nearestMark(const QgsMapCanvas* canvas, const QVector<QgsPointXY>& pts, const QPoint& screen) {
  if (!canvas) return -1;
  const QgsMapToPixel& m2p = canvas->mapSettings().mapToPixel();
  int best = -1;
  double bestD = kGrabPixels;
  for (int i = 0; i < pts.size(); ++i) {
    const QgsPointXY s = m2p.transform(pts[i]);
    const double d = std::hypot(s.x() - screen.x(), s.y() - screen.y());
    if (d <= bestD) {
      bestD = d;
      best = i;
    }
  }
  return best;
}

bool movedBeyondSlop(const QPoint& from, const QPoint& to) {
  return (to - from).manhattanLength() >= kDragSlopPixels;
}

}  // namespace

void KaAlignPickTool::canvasPressEvent(QgsMapMouseEvent* e) {
  if (e->button() != Qt::LeftButton) return;
  m_dragIndex = nearestMark(canvas(), m_dragPoints, e->pos());
  if (m_dragIndex >= 0) {
    m_dragPress = e->pos();
    return;
  }
  bool snapped = false;
  emit picked(snapPoint(e, &snapped));
}

void KaAlignPickTool::canvasReleaseEvent(QgsMapMouseEvent* e) {
  if (m_dragIndex < 0 || e->button() != Qt::LeftButton) return;
  const int index = m_dragIndex;
  m_dragIndex = -1;
  bool snapped = false;
  const QgsPointXY pt = snapPoint(e, &snapped);
  updateSnapMark(pt, snapped);
  // A click without movement keeps its old meaning: pick a new left point there.
  if (movedBeyondSlop(m_dragPress, e->pos()))
    emit pointDragged(index, pt);
  else
    emit picked(pt);
}

int KaAlignMapTool::pairMarkAt(const QPoint& screen) const {
  QVector<QgsPointXY> pts;
  pts.reserve(m_pairs.size());
  for (const GeorefService::Pair& p : m_pairs) pts.append(QgsPointXY(p.mapX, p.mapY));
  return nearestMark(canvas(), pts, screen);
}

bool KaAlignMapTool::beginPairDrag(QgsMapMouseEvent* e) {
  if (m_haveFrom || m_phase == Phase::WaitTo) return false;
  const int index = pairMarkAt(e->pos());
  if (index < 0) return false;
  m_dragIndex = index;
  m_dragPress = e->pos();
  emit statusChanged(QStringLiteral("%1번 점을 끌어 옮기세요").arg(index + 1));
  return true;
}

void KaAlignMapTool::canvasReleaseEvent(QgsMapMouseEvent* e) {
  if (m_dragIndex < 0 || e->button() != Qt::LeftButton) return;
  const int index = m_dragIndex;
  m_dragIndex = -1;
  if (index >= m_pairs.size() || !movedBeyondSlop(m_dragPress, e->pos())) {
    rebuildPairMarks();
    emit statusChanged(statusText());
    return;
  }
  QgsPointXY mapPt;
  mapPointFromEvent(e, &mapPt, nullptr);
  m_pairs[index].mapX = mapPt.x();
  m_pairs[index].mapY = mapPt.y();
  m_affine = {};
  rebuildPairMarks();
  emit statusChanged(QStringLiteral("%1번 오른쪽 점을 옮겼습니다. 「이동」으로 다시 맞추세요").arg(index + 1));
  emit pairsChanged();
}

bool KaAlignMapTool::movePairSource(int index, double sx, double sy) {
  if (index < 0 || index >= m_pairs.size()) return false;
  m_pairs[index].srcX = sx;
  m_pairs[index].srcY = sy;
  m_affine = {};
  emit statusChanged(QStringLiteral("%1번 왼쪽 점을 옮겼습니다. 「이동」으로 다시 맞추세요").arg(index + 1));
  emit pairsChanged();
  return true;
}

bool KaAlignMapTool::ensureRasterBackup(QString* errorOut) {
  if (!m_raster || m_rasterBackup.valid) return true;
  QString err;
  const QString root = m_backupRoot.isEmpty() ? GeorefBackup::backupRootFor(QString()) : m_backupRoot;
  if (GeorefBackup::backupRaster(m_rasterPath, root, &m_rasterBackup, &err)) return true;
  if (errorOut)
    *errorOut = QStringLiteral("원래 좌표를 백업하지 못해 그림 파일을 바꾸지 않았습니다. %1").arg(err);
  return false;
}

bool KaAlignMapTool::restoreOriginals(QString* message) {
  m_pairs.clear();
  m_haveFrom = false;
  m_phase = Phase::WaitFrom;
  m_affine = {};
  m_dragIndex = -1;
  bool ok = true;
  QString text;
  if (!m_layer) {
    text = QStringLiteral("되돌릴 맞추기가 없습니다");
  } else if (!m_raster) {
    auto* vl = qobject_cast<QgsVectorLayer*>(m_layer.data());
    ok = vl && vl->startEditing();
    if (ok) {
      for (auto it = m_originals.constBegin(); it != m_originals.constEnd(); ++it) {
        QgsGeometry g = it.value();
        vl->changeGeometry(it.key(), g);
      }
      ok = vl->commitChanges();
      vl->triggerRepaint();
    }
    text = ok ? QStringLiteral("도면을 맞추기 전 자리로 되돌렸습니다")
              : QStringLiteral("찍은 점은 지웠지만 도면을 되돌리지 못했습니다");
  } else if (!m_rasterBackup.valid) {
    text = QStringLiteral("찍은 점을 지웠습니다. 그림 파일은 아직 바뀌지 않았습니다");
  } else {
    auto* rl = qobject_cast<QgsRasterLayer*>(m_layer.data());
    QString err;
    ok = GeorefBackup::restoreRasterLayer(rl, m_rasterBackup, m_originalRasterCrs, &err);
    if (ok) {
      LayerOps::setAlignPending(rl, true);
      rl->setOpacity(0.72);
      text = QStringLiteral("그림 좌표를 맞추기 전으로 되돌렸습니다 (백업: %1)")
                 .arg(QDir::toNativeSeparators(m_rasterBackup.folder));
    } else {
      text = QStringLiteral("찍은 점은 지웠지만 그림 좌표를 되돌리지 못했습니다. %1").arg(err);
    }
  }
  clearMarks();
  if (canvas()) LayerOps::refreshCanvasIfIdle(canvas());
  emit statusChanged(statusText());
  emit pairsChanged();
  if (message) *message = text;
  return ok;
}
