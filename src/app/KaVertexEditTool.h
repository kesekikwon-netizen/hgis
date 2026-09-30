#pragma once

#include <qgsfeatureid.h>
#include <qgsfeature.h>
#include <qgsmapmouseevent.h>
#include <qgsmaptool.h>
#include <qgspointxy.h>

#include <QPointer>

#include "KaEditTolerance.h"

class QgsVectorLayer;
class QgsRubberBand;
class QgsVertexMarker;
class QKeyEvent;

// 꼭짓점 편집기. 「도형선택」(KaFeatureSelectTool)이 이 기능을 들고 쓴다.
//
// QGIS의 QgsVertexTool은 qgis_app 안에 있어 SDK가 내보내지 않는다(qgis_gui에
// 심볼이 없음). Architecture B에서 링크할 수 없으므로 조사 도메인에 필요한
// 최소 기능만 QgsMapTool 위에 직접 만든다:
//   - 도형 클릭 = 선택(꼭짓점 네모가 보임)
//   - 꼭짓점을 끌면 그 점만 이동 → 놓을 때 편집 버퍼에 한 번 넣는다(Ctrl+Z로 되돌림,
//     파일에는 「저장」 때 쓴다). 끄는 동안 기본은 커서 위치, Ctrl을 누르면 자석.
//   - 선 위 우클릭 = 점추가 / 점삭제 메뉴
//   - 변 위를 더블클릭하면 그 자리에 꼭짓점 추가
//   - 꼭짓점을 클릭해 고른 뒤 Delete = 그 꼭짓점 삭제
//     (면은 4점, 선은 2점 미만으로는 줄이지 않는다)
// 이 클래스를 지도 도구로 따로 켜는 진입점은 없다(「도형수정」을 따로 두지 않는다).
class KaVertexEditTool : public QgsMapTool {
  Q_OBJECT
public:
  explicit KaVertexEditTool(QgsMapCanvas* canvas);
  ~KaVertexEditTool() override;

  void setLayer(QgsVectorLayer* layer);
  QgsVectorLayer* layer() const { return m_layer; }
  void setSnapEnabled(bool on) { m_snapEnabled = on; }

  Flags flags() const override { return Flags(); }
  void canvasPressEvent(QgsMapMouseEvent* e) override;
  void canvasMoveEvent(QgsMapMouseEvent* e) override;
  void canvasReleaseEvent(QgsMapMouseEvent* e) override;
  void canvasDoubleClickEvent(QgsMapMouseEvent* e) override;
  void keyPressEvent(QKeyEvent* e) override;
  void activate() override;
  void deactivate() override;


  // 「도형선택」이 편집 기능만 빌려 쓴다. 지도 도구로 걸지 않고 호출만 한다.
  // 이 도구를 따로 켜는 버튼은 없다 — 도형을 고르면 수정점이 바로 나와야 한다는
  // 요구에 맞춰, 선택 도구가 이 기능들을 그대로 불러 쓴다.
  void setTarget(QgsVectorLayer* layer, QgsFeatureId fid);
  void clearTarget();
  bool hasTarget() const { return !m_layer.isNull() && m_fid >= 0; }
  void showVertexMarkers();
  // 끄는 동안 화면만 미리 보여 준다. 저장은 놓을 때 moveVertexTo 에서 한 번.
  void previewVertexMove(int index, const QgsPointXY& toLayerPt);
  int vertexNear(const QgsPointXY& mapPt, int tolPx = KaEditTolerance::kVertexGrabPx) const;
  int segmentNear(const QgsPointXY& mapPt, QgsPointXY* onLine,
                  int tolPx = KaEditTolerance::kSegmentPx) const;
  bool moveVertexTo(int index, const QgsPointXY& to);
  bool deleteVertexAt(int index);
  bool insertVertexAt(int index, const QgsPointXY& at);
  QString lastEditError() const { return m_lastEditError; }
  QgsGeometry selectedGeometry() const;
  // 도형은 레이어 CRS, 마우스는 지도 CRS. 섞으면 도형이 안 잡힌다.
  QgsPointXY toLayer(const QgsPointXY& mapPt) const;
  QgsPointXY snapMapPoint(QgsMapMouseEvent* e, bool* snapped = nullptr) const;
  // Snaps to other shapes only: the edited feature's own vertices and edges are ignored,
  // otherwise a dragged corner would jump back onto itself.
  QgsPointXY snapMapPointExcludingTarget(QgsMapMouseEvent* e, bool* snapped = nullptr) const;
  // The vertex clicked last is highlighted; Delete removes it. -1 = none.
  void setActiveVertex(int index);
  int activeVertex() const { return m_activeIndex; }

signals:
  void statusMessage(const QString& text);
  // Emitted for an applied buffer change too when its commit fails: it must
  // remain recoverable through Undo while the user retries saving.
  void featureGeometryEdited(QgsVectorLayer* layer, const QgsFeature& before);

private:
  void selectAt(const QgsPointXY& mapPt);
  void clearSelection();
  void showLineVertexMenu(QgsMapMouseEvent* e);
  double mapTolerance(int px) const;
  double layerTolerance(int px) const;
  QgsPointXY toMap(const QgsPointXY& layerPt) const;
  void refreshRubber(const QgsGeometry& geom);
  void styleMarker(int index);
  bool applyGeometryChange(QgsGeometry geom, const QString& commandText);

  QPointer<QgsVectorLayer> m_layer;
  QgsFeatureId m_fid = -1;
  QgsRubberBand* m_outline = nullptr;
  QVector<QgsVertexMarker*> m_marks;
  int m_dragIndex = -1;
  int m_activeIndex = -1;
  bool m_dragging = false;
  bool m_snapEnabled = true;
  QString m_lastEditError;
  Qt::ContextMenuPolicy m_savedMenuPolicy = Qt::DefaultContextMenu;
};
