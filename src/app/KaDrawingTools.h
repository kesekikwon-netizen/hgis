#pragma once
// Layout view tools used by the drawing studio (KaDrawingStudio).
#include <QPointF>
#include <QPointer>
#include <QTimer>

#include <qgslayoutitemmap.h>
#include <qgslayoutviewtool.h>

class QgsLayoutView;
class QgsLayoutViewMouseEvent;
class QWheelEvent;

// Coordinate point picker: left click places a callout, right click removes the last one.
class KaLayoutCoordPointTool : public QgsLayoutViewTool {
  Q_OBJECT
public:
  explicit KaLayoutCoordPointTool(QgsLayoutView* view);
  void layoutPressEvent(QgsLayoutViewMouseEvent* event) override;

signals:
  void pointClicked(const QPointF& layoutPt);
  void undoRequested();
};

// Map adjust tool: drag pans the map content, the wheel zooms around the cursor.
// Overlay maps (above-labels and number maps) follow the base map.
class KaLayoutMapAdjustTool : public QgsLayoutViewTool {
  Q_OBJECT
public:
  explicit KaLayoutMapAdjustTool(QgsLayoutView* view);
  void layoutPressEvent(QgsLayoutViewMouseEvent* event) override;
  void layoutMoveEvent(QgsLayoutViewMouseEvent* event) override;
  void layoutReleaseEvent(QgsLayoutViewMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void deactivate() override;

signals:
  void mapViewChanged();

private slots:
  void flushZoom();

private:
  QgsLayoutItemMap* mapAt(const QPoint& viewPos) const;

  QTimer mZoomTimer;
  QPointer<QgsLayoutItemMap> mPendingMap;
  QPointF mPendingPoint;
  double mPendingFactor = 1.0;
  QPointer<QgsLayoutItem> mMoveItem;
  QPointF mMoveStart;
  bool mMoving = false;
};
