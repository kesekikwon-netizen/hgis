#include "KaDrawingTools.h"

#include "core/LayerOps.h"
#include "core/LayoutService.h"

#include <QCursor>
#include <QList>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

#include <qgslayout.h>
#include <qgslayoutview.h>
#include <qgslayoutviewmouseevent.h>

namespace {
constexpr const char* kIdMap = "ka_map";
constexpr const char* kIdMapAbove = "ka_map_above";
constexpr const char* kIdMapNumbers = "ka_map_numbers";

bool isOverlayMap(const QgsLayoutItemMap* map) {
  if (!map) return false;
  const QString id = map->id();
  return id == QLatin1String(kIdMapAbove) || id == QLatin1String(kIdMapNumbers);
}
}  // namespace

KaLayoutCoordPointTool::KaLayoutCoordPointTool(QgsLayoutView* view)
    : QgsLayoutViewTool(view, QStringLiteral("좌표점")) {
  setCursor(Qt::CrossCursor);
}

void KaLayoutCoordPointTool::layoutPressEvent(QgsLayoutViewMouseEvent* event) {
  if (!event) return;
  if (event->button() == Qt::RightButton) {
    emit undoRequested();
    event->accept();
    return;
  }
  if (event->button() != Qt::LeftButton) {
    event->ignore();
    return;
  }
  emit pointClicked(event->layoutPoint());
  event->accept();
}

KaLayoutMapAdjustTool::KaLayoutMapAdjustTool(QgsLayoutView* view)
    : QgsLayoutViewTool(view, QStringLiteral("지도 조정")) {
  setCursor(Qt::OpenHandCursor);
  mZoomTimer.setSingleShot(true);
  mZoomTimer.setInterval(220);
  connect(&mZoomTimer, &QTimer::timeout, this, &KaLayoutMapAdjustTool::flushZoom);
}

void KaLayoutMapAdjustTool::layoutPressEvent(QgsLayoutViewMouseEvent* event) {
  if (!event || event->button() != Qt::LeftButton) {
    if (event) event->ignore();
    return;
  }
  auto* map = mapAt(event->pos());
  if (!map) {
    event->ignore();
    return;
  }
  flushZoom();
  mMoveItem = map;
  mMoveStart = event->layoutPoint();
  mMoving = true;
  setCursor(Qt::ClosedHandCursor);
  event->accept();
}

void KaLayoutMapAdjustTool::layoutMoveEvent(QgsLayoutViewMouseEvent* event) {
  if (!mMoving || !mMoveItem || !event) {
    if (event) event->ignore();
    return;
  }
  mMoveItem->setMoveContentPreviewOffset(event->layoutPoint().x() - mMoveStart.x(),
                                         event->layoutPoint().y() - mMoveStart.y());
  mMoveItem->update();
  event->accept();
}

void KaLayoutMapAdjustTool::layoutReleaseEvent(QgsLayoutViewMouseEvent* event) {
  if (!event || event->button() != Qt::LeftButton || !mMoving || !mMoveItem) {
    if (event) event->ignore();
    return;
  }
  mMoveItem->setMoveContentPreviewOffset(0, 0);
  const double dx = event->layoutPoint().x() - mMoveStart.x();
  const double dy = event->layoutPoint().y() - mMoveStart.y();
  if (std::abs(dx) > 0.2 || std::abs(dy) > 0.2)
    mMoveItem->moveContent(-dx, -dy);
  mMoveItem = nullptr;
  mMoving = false;
  setCursor(Qt::OpenHandCursor);
  emit mapViewChanged();
  event->accept();
}

void KaLayoutMapAdjustTool::wheelEvent(QWheelEvent* event) {
  if (!event) return;
  event->accept();
  auto* map = mapAt(event->position().toPoint());
  if (!map) return;

  const int delta = event->angleDelta().y();
  if (delta == 0) return;
  const bool zoomIn = delta > 0;
  double step = 1.0 + ((LayerOps::kWheelZoomFactor - 1.0) *
                       std::min(1.0, std::fabs(static_cast<double>(delta)) / 120.0));
  if (event->modifiers() & Qt::ControlModifier)
    step = 1.0 + (step - 1.0) * 0.35;
  const double factor = zoomIn ? step : (1.0 / step);

  const QPointF scenePt = view() ? view()->mapToScene(event->position().toPoint()) : QPointF();
  mPendingMap = map;
  mPendingPoint = map->mapFromScene(scenePt);
  mPendingFactor *= factor;
  mZoomTimer.start();
}

void KaLayoutMapAdjustTool::deactivate() {
  flushZoom();
  if (mMoveItem)
    mMoveItem->setMoveContentPreviewOffset(0, 0);
  mMoving = false;
  mMoveItem = nullptr;
  QgsLayoutViewTool::deactivate();
}

void KaLayoutMapAdjustTool::flushZoom() {
  if (!mPendingMap || std::fabs(mPendingFactor - 1.0) < 1e-4) {
    mPendingFactor = 1.0;
    return;
  }
  const double cur = mPendingMap->scale();
  if (cur > 10.0) {
    // Zoom around the wheel position. zoomToExtent keeps the frame size,
    // setExtent would resize the map frame on paper.
    const QRectF box = mPendingMap->rect();
    const QgsRectangle ext = mPendingMap->extent();
    const bool canAnchor = box.width() > 0.0 && box.height() > 0.0 && ext.isFinite() &&
                           ext.width() > 0.0 && ext.height() > 0.0;
    if (canAnchor) {
      const double fx = (mPendingPoint.x() - box.left()) / box.width();
      const double fy = (mPendingPoint.y() - box.top()) / box.height();
      mPendingMap->zoomToExtent(LayoutService::zoomExtentAtAnchor(ext, fx, fy, mPendingFactor));
    } else {
      mPendingMap->setScale(cur / mPendingFactor, true);
    }
    if (mPendingMap->layout()) {
      QList<QgsLayoutItemMap*> maps;
      mPendingMap->layout()->layoutItems(maps);
      for (QgsLayoutItemMap* other : maps) {
        if (!other || other == mPendingMap || !isOverlayMap(other)) continue;
        other->setCrs(mPendingMap->crs());
        other->zoomToExtent(mPendingMap->extent());
      }
    }
  }
  mPendingFactor = 1.0;
  emit mapViewChanged();
}

QgsLayoutItemMap* KaLayoutMapAdjustTool::mapAt(const QPoint& viewPos) const {
  if (!view() || !layout()) return nullptr;
  const QPointF scenePt = view()->mapToScene(viewPos);
  QgsLayoutItemMap* hit = dynamic_cast<QgsLayoutItemMap*>(layout()->layoutItemAt(scenePt, true));
  QList<QgsLayoutItemMap*> maps;
  layout()->layoutItems(maps);
  // Number and above-label maps cover the same frame as the base map. If the
  // wheel picked one of them only the numbers would move instead of zooming.
  if (!hit || isOverlayMap(hit)) {
    for (QgsLayoutItemMap* map : maps) {
      if (map && map->id() == QLatin1String(kIdMap)) return map;
    }
  }
  if (hit) return hit;
  return maps.isEmpty() ? nullptr : maps.first();
}
