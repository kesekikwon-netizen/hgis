#pragma once

#include <qgsmaptool.h>
#include <qgsmapmouseevent.h>
#include <qgspointxy.h>
#include <qgsfeatureid.h>
#include <qgsfeature.h>

#include <QPointer>
#include <QList>

class QgsMapCanvas;
class QgsMapLayer;
class QgsVectorLayer;
class QgsRubberBand;
class KaVertexEditTool;

class KaFeatureSelectTool : public QgsMapTool {
  Q_OBJECT
public:
  struct SelectedItem {
    QPointer<QgsVectorLayer> layer;
    QgsFeatureId fid = -1;
  };

  explicit KaFeatureSelectTool(QgsMapCanvas* canvas);
  ~KaFeatureSelectTool() override;

  Flags flags() const override { return Flags(); }
  void canvasPressEvent(QgsMapMouseEvent* e) override;
  void canvasMoveEvent(QgsMapMouseEvent* e) override;
  void canvasReleaseEvent(QgsMapMouseEvent* e) override;
  void activate() override;
  void deactivate() override;

  static QList<SelectedItem> allSelectedFeatures(QgsMapCanvas* canvas);

  // 자석은 새 점을 찍을 때 쓴다. 이미 있는 점을 끌 때는 커서 위치를 쓴다.
  void setSnapEnabled(bool on);
  // Update editing handles after an external Undo restores the feature geometry.
  void refreshSelectedGeometry();

signals:
  void selectionChanged(int totalSelected);
  void statusMessage(const QString& msg);
  void featureGeometryEdited(QgsVectorLayer* layer, const QgsFeature& before);
  void requestMerge();
  void requestSplit();
  void requestClip();
  void requestMapContextMenu(const QPoint& canvasPos);

private:
  void handleContextMenu(QgsMapMouseEvent* e);
  void selectAtPoint(const QgsPointXY& mapPt, bool addToSelection);
  void selectInRect(const QgsRectangle& mapRect, bool addToSelection);
  QList<QgsMapLayer*> pickableLayers() const;
  // 도형 하나만 골랐으면 그 도형의 수정점을 띄운다. 여러 개면 지운다.
  void syncVertexTarget();

  // 도형을 고르면 곧바로 수정점이 나와야 한다는 요구에 맞춰, 선택 도구가 꼭짓점
  // 편집기를 직접 들고 있다. 지도 도구로 걸지 않고 기능만 불러 쓴다.
  KaVertexEditTool* m_vertex = nullptr;
  bool m_vertexDragging = false;
  int m_vertexIndex = -1;

  bool m_dragging = false;
  QPoint m_pressPos;
  QgsPointXY m_pressMapPt;
  QgsRubberBand* m_rubberBand = nullptr;
};
