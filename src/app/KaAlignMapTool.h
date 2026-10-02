#pragma once

#include "core/GeorefService.h"
#include "core/GeorefBackup.h"
#include <qgsmaptool.h>
#include <qgsmapmouseevent.h>
#include <qgspointxy.h>
#include <qgscoordinatereferencesystem.h>
#include <QHash>
#include <QPointer>
#include <QVector>

class QgsRubberBand;
class QgsVertexMarker;
class QgsMapLayer;
class QgsVectorLayer;
class QgsRasterLayer;
class QgsGeometry;

class KaAlignPickTool : public QgsMapTool {
  Q_OBJECT
public:
  explicit KaAlignPickTool(QgsMapCanvas* canvas);
  ~KaAlignPickTool() override;
  void canvasPressEvent(QgsMapMouseEvent* e) override;
  void canvasMoveEvent(QgsMapMouseEvent* e) override;
  void canvasReleaseEvent(QgsMapMouseEvent* e) override;
  void deactivate() override;
  // Already picked source points; pressing on one drags it instead of picking anew.
  void setDragPoints(const QVector<QgsPointXY>& pts) { m_dragPoints = pts; }
signals:
  void picked(const QgsPointXY& pt);
  void pointDragged(int index, const QgsPointXY& pt);

private:
  QgsPointXY snapPoint(QgsMapMouseEvent* e, bool* snapped);
  void updateSnapMark(const QgsPointXY& pt, bool snapped);
  QgsVertexMarker* m_snapMark = nullptr;
  QVector<QgsPointXY> m_dragPoints;
  int m_dragIndex = -1;
  QPoint m_dragPress;
};

class KaAlignMapTool : public QgsMapTool {
  Q_OBJECT
public:
  explicit KaAlignMapTool(QgsMapCanvas* canvas);
  ~KaAlignMapTool() override;

  bool beginLayer(QgsMapLayer* layer, const QgsCoordinateReferenceSystem& workCrs,
                  QString* errorOut = nullptr);
  void endSession();
  bool hasSession() const;
  QgsMapLayer* targetLayer() const { return m_layer; }
  QgsMapLayer* sourceDisplayLayer() const;
  QString rasterSourcePath() const { return m_rasterPath; }
  void setSourcePoint(double sx, double sy);
  void setMapHint(double mx, double my, bool valid);
  bool hasPendingSource() const { return m_haveFrom; }
  double pendingSrcX() const { return m_srcX; }
  double pendingSrcY() const { return m_srcY; }
  const QVector<GeorefService::Pair>& pairs() const { return m_pairs; }

  bool fitToDisplay();
  bool removeLastPair();
  bool removePairAt(int index);
  // Clears the points and puts the target back as it was when the session began. A raster
  // is restored from the backup taken before its first world-file write; *message says
  // honestly what happened (restored, nothing written yet, or failed).
  bool restoreOriginals(QString* message = nullptr);
  bool applyMove(QString* errorOut = nullptr);
  bool saveAligned(QString* savedPath, QString* errorOut);
  // Fine-tune an existing pair by dragging its left (source) point.
  bool movePairSource(int index, double sx, double sy);

  // Folder that receives the raster backup (survey folder/정합백업). Set before beginLayer.
  void setBackupRoot(const QString& root) { m_backupRoot = root; }
  QString backupFolder() const { return m_rasterBackup.valid ? m_rasterBackup.folder : QString(); }

  int pairCount() const { return m_pairs.size(); }
  QString statusText() const;
  bool isRasterSession() const { return m_raster; }

  Flags flags() const override;
  void canvasPressEvent(QgsMapMouseEvent* e) override;
  void canvasMoveEvent(QgsMapMouseEvent* e) override;
  void canvasReleaseEvent(QgsMapMouseEvent* e) override;
  void keyPressEvent(QKeyEvent* e) override;
  void deactivate() override;

signals:
  void statusChanged(const QString& text);
  void pairsChanged();
  void cursorMoved(const QgsPointXY& mapPt);
  void sessionEnded();

private:
  enum class Phase { Idle, WaitFrom, WaitTo };

  void clearMarks();
  void rebuildPairMarks();
  bool applyPreview(QString* errorOut = nullptr);
  void captureOriginals(QgsVectorLayer* vl);
  bool mapPointFromEvent(QgsMapMouseEvent* e, QgsPointXY* out, bool* snapped);
  void updateSnapMark(const QgsPointXY& pt, bool snapped);
  QgsCoordinateReferenceSystem workCrs() const;
  bool ensureRasterBackup(QString* errorOut);
  int pairMarkAt(const QPoint& screen) const;
  bool beginPairDrag(QgsMapMouseEvent* e);
  bool saveVectorAligned(QString* savedPath, QString* errorOut);  // KaAlignMapToolSave.cpp
  void finishDrawingSession();  // 도면: 맞춤 복제본을 빼고 그 도면 레이어를 모두 다시 보인다

  QPointer<QgsMapLayer> m_layer;
  QPointer<QgsVectorLayer> m_hiddenSource;
  QPointer<QgsVectorLayer> m_displayClone;
  bool m_raster = false;
  int m_pixelW = 0;
  int m_pixelH = 0;
  QString m_rasterPath;
  double m_srcX = 0;
  double m_srcY = 0;
  double m_hintX = 0;
  double m_hintY = 0;
  bool m_hasHint = false;
  QgsCoordinateReferenceSystem m_workCrs;
  QVector<GeorefService::Pair> m_pairs;
  GeorefService::Affine m_affine;
  QHash<qint64, QgsGeometry> m_originals;
  Phase m_phase = Phase::Idle;
  QgsPointXY m_fromMap;
  bool m_haveFrom = false;
  QgsRubberBand* m_rubber = nullptr;
  QgsVertexMarker* m_snapMark = nullptr;
  QVector<QgsVertexMarker*> m_marks;
  QString m_backupRoot;
  GeorefBackup::RasterBackup m_rasterBackup;
  QgsCoordinateReferenceSystem m_originalRasterCrs;
  QString m_savedVectorPath;
  int m_dragIndex = -1;
  QPoint m_dragPress;
  QStringList m_cadHidden;  // 도면 정합 동안 숨긴 같은 도면의 다른 레이어
  QString m_cadDrawing;     // 맞추는 도면의 id. 원본 레이어가 지워져도 남아 정리와 저장 거절에 쓴다
};
