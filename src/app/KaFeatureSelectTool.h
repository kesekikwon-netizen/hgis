#pragma once

#include <qgsmaptool.h>
#include <qgsmapmouseevent.h>
#include <qgspointxy.h>
#include <qgsfeatureid.h>
#include <qgsfeature.h>

#include <QPointer>
#include <QList>
#include <optional>

#include "core/PolygonPieces.h"

class QgsMapCanvas;
class QgsVectorLayer;
class QgsRubberBand;
class QgsVertexMarker;
class QAction;
class QKeyEvent;
class KaVertexEditTool;

class KaFeatureSelectTool : public QgsMapTool {
  Q_OBJECT
public:
  struct SelectedItem {
    QPointer<QgsVectorLayer> layer;
    QgsFeatureId fid = FID_NULL;
  };

  explicit KaFeatureSelectTool(QgsMapCanvas* canvas);
  ~KaFeatureSelectTool() override;

  Flags flags() const override { return Flags(); }
  void canvasPressEvent(QgsMapMouseEvent* e) override;
  void canvasMoveEvent(QgsMapMouseEvent* e) override;
  void canvasReleaseEvent(QgsMapMouseEvent* e) override;
  void keyPressEvent(QKeyEvent* e) override;
  void activate() override;
  void deactivate() override;

  static QList<SelectedItem> allSelectedFeatures(QgsMapCanvas* canvas);

  // 자석은 새 점을 찍을 때 쓴다. 이미 있는 점을 끌 때는 커서 위치를 쓰고,
  // Ctrl을 누른 채 끌 때만 (끄는 도형 자신을 뺀) 다른 도형에 붙는다.
  void setSnapEnabled(bool on);
  // Update editing handles after an external Undo restores the feature geometry.
  void refreshSelectedGeometry();
  // Delete while a vertex is picked (clicked) removes that vertex instead of the shape.
  // True when a picked vertex took the key, even if the vertex could not be removed.
  bool deleteActiveVertex();
  // An inner piece of a polygon (a hole, or a part lying inside another part) is picked by
  // clicking it. Delete takes the picked vertex first, then the picked piece; the rest of
  // the shape stays. True when either took the key, so the whole shape is not deleted.
  bool deleteActivePick();
  // Turns the picked inner part into a hole of the shape around it. False for a hole.
  bool cutOutActivePiece();
  bool hasActivePiece() const;
  // A = 도형선택 from anywhere on the map. It never toggles the tool off, leaves the tape's
  // own A alone and does not drop a half-drawn shape.
  static void installKeyShortcut(QWidget* window, QAction* selectAction);
  // Only survey data can be picked: reference maps and cadastral layers are read-only.
  static bool isPickableLayer(const QgsVectorLayer* layer);

signals:
  void selectionChanged(int totalSelected);
  void statusMessage(const QString& msg);
  void featureGeometryEdited(QgsVectorLayer* layer, const QgsFeature& before);
  void requestMerge();
  void requestSplit();
  void requestClip();
  void requestMapContextMenu(const QPoint& canvasPos);

private:
  struct Pick {
    QString layerId;
    QgsFeatureId fid = FID_NULL;
    PolygonPieces::Piece piece;
    friend bool operator==(const Pick&, const Pick&) = default;
  };
  void handleContextMenu(QgsMapMouseEvent* e);
  // An invalid piece clears the pick and its red highlight.
  void setActivePiece(const PolygonPieces::Piece& piece);
  std::optional<PolygonPieces::Piece> pieceUnder(const QgsPointXY& mapPt) const;
  bool removeActivePiece(bool cutOut);
  // Nearest line/point first, then the smallest polygon under the cursor. Clicking the same
  // spot again moves to the next overlapping shape.
  void selectAtPoint(const QgsPointXY& mapPt, bool addToSelection, const QPoint& screenPos,
                     bool allowCycle);
  void selectInRect(const QgsRectangle& mapRect, bool addToSelection);
  // 도형 하나만 골랐으면 그 도형의 수정점을 띄운다. 여러 개면 지운다.
  void syncVertexTarget();
  // Where a dragged vertex goes: the cursor, or with Ctrl held a snap to other shapes.
  QgsPointXY dragPoint(QgsMapMouseEvent* e);
  void showSnapMark(const QgsPointXY& mapPt, bool snapped);

  // 도형을 고르면 곧바로 수정점이 나와야 한다는 요구에 맞춰, 선택 도구가 꼭짓점
  // 편집기를 직접 들고 있다. 지도 도구로 걸지 않고 기능만 불러 쓴다.
  KaVertexEditTool* m_vertex = nullptr;
  bool m_vertexDragging = false;
  bool m_vertexMoved = false;
  int m_vertexIndex = -1;
  QPoint m_vertexPressPos;
  QgsVertexMarker* m_snapMark = nullptr;

  QPoint m_lastPickPos;
  QList<Pick> m_lastPickCandidates;
  PolygonPieces::Piece m_activePiece;
  QgsRubberBand* m_pieceBand = nullptr;
  int m_lastPickIndex = -1;

  bool m_dragging = false;
  QPoint m_pressPos;
  QgsPointXY m_pressMapPt;
  QgsRubberBand* m_rubberBand = nullptr;
};
