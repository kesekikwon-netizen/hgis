#pragma once

#include <qgsmaptool.h>
#include <qgspointxy.h>
#include <qgsmapmouseevent.h>
#include <qgsfeatureid.h>
#include <qgsrubberband.h>
#include <QMetaObject>
#include <QPointer>

class QgsVectorLayer;
class QKeyEvent;

// trial_trench 편집 도구. ArcGIS 그래픽 레이어처럼 다룬다:
//  - Single 모드(기본): 트렌치를 클릭해 선택 → 끌어서 하나만 이동,
//    Delete/Backspace 또는 우클릭 = 그 트렌치만 삭제
//  - Whole 모드: 끌어다 놓기 = 격자 전체 이동
// Every move/delete is one undoable edit command (Ctrl+Z); the survey file is written only
// when the user saves.
class KaTrenchMoveTool : public QgsMapTool {
  Q_OBJECT
public:
  enum class Mode { Single, Whole };

  explicit KaTrenchMoveTool(QgsMapCanvas* canvas);
  ~KaTrenchMoveTool() override;
  void setLayer(QgsVectorLayer* layer);
  void setSnapMeters(double meters);
  void setGridOverlay(class KaCanvasGridOverlay* grid);
  void setMode(Mode mode);
  Mode mode() const { return m_mode; }
  QgsFeatureId selectedTrench() const { return m_selFid; }
  // Deletes the picked trench (one Ctrl+Z step). False when nothing is picked. The window's
  // Delete shortcut reaches the canvas before this tool, so MainWindow routes Delete here.
  bool deleteSelectedTrench();

  Flags flags() const override { return Flags(); }
  void canvasPressEvent(QgsMapMouseEvent* e) override;
  void canvasMoveEvent(QgsMapMouseEvent* e) override;
  void canvasReleaseEvent(QgsMapMouseEvent* e) override;
  void keyPressEvent(QKeyEvent* e) override;
  void activate() override;
  void deactivate() override;

signals:
  void statusMessage(const QString& text);
  // A move or delete went onto the layer's undo stack (survey now unsaved).
  void trenchesEdited();

private:
  void rebuildRubber(const QgsPointXY& offset);
  void rebuildSingleRubber(QgsFeatureId fid, const QgsPointXY& offset);
  void cancelDrag();
  void clearSingleSelection();
  void refreshSelection();
  // Moves the given trenches (all when empty) as one undoable command.
  bool applyTranslate(const QgsFeatureIds& fids, double dx, double dy);
  QgsFeatureId hitTrench(const QgsPointXY& p, QString* nameOut = nullptr) const;
  void deleteTrench(QgsFeatureId fid, const QString& name);
  QgsPointXY snapPt(const QgsPointXY& p) const;
  void afterEdit();

  QPointer<QgsVectorLayer> m_layer;
  QMetaObject::Connection m_layerWatch;
  // The canvas deletes its scene items when it goes first (it is a child of the central
  // widget, this tool of the window); QPointer keeps the destructor from deleting twice.
  QPointer<QgsRubberBand> m_rubber;        // 전체 이동 고스트
  QPointer<QgsRubberBand> m_singleRubber;  // 선택/개별 이동 하이라이트
  Mode m_mode = Mode::Single;
  bool m_dragging = false;
  bool m_dragSingle = false;
  // Edit-buffer features have negative ids, so "none" is FID_NULL, never -1.
  QgsFeatureId m_selFid = FID_NULL;
  QString m_selName;
  QgsPointXY m_from;
  double m_snapM = 0.0;
  class KaCanvasGridOverlay* m_grid = nullptr;
};
