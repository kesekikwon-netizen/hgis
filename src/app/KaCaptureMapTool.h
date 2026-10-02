#pragma once
#include <qgsmaptool.h>
#include <qgsgeometry.h>
#include <qgspointxy.h>
#include <qgsmapmouseevent.h>
#include <QVector>
#include <QPointer>

class QgsRubberBand;
class QgsVertexMarker;
class QgsVectorLayer;
class QgsMapCanvas;
class QTimer;

// Sketches one new shape at a time. Saved shapes are changed in 도형선택, never here,
// so the first click always starts a new shape, even on top of an existing vertex
// (neighbouring features may share that corner).
class KaCaptureMapTool : public QgsMapTool {
  Q_OBJECT
public:
  enum class Mode { Polygon, Line, Point };

  explicit KaCaptureMapTool(QgsMapCanvas* canvas);
  ~KaCaptureMapTool() override;

  void setMode(Mode mode);
  void setTargetLayer(QgsVectorLayer* layer);
  void setSnapEnabled(bool on);
  void setEasyDraw(bool on);
  bool easyDraw() const { return m_easyDraw; }
  void resetSession();
  bool undoLastVertex();
  Mode mode() const { return m_mode; }
  int pointCount() const { return m_points.size(); }
  bool hasSketch() const { return !m_points.isEmpty(); }
  // Set right before geometryCaptured when closing the polygon had to change the sketch
  // (parts of a self-intersecting ring left out, near-zero area). Empty otherwise.
  QString lastRepairNotice() const { return m_repairNotice; }

  Flags flags() const override;
  void canvasPressEvent(QgsMapMouseEvent* e) override;
  void canvasReleaseEvent(QgsMapMouseEvent* e) override;
  void canvasMoveEvent(QgsMapMouseEvent* e) override;
  void canvasDoubleClickEvent(QgsMapMouseEvent* e) override;
  void keyPressEvent(QKeyEvent* e) override;
  void activate() override;
  void deactivate() override;

public slots:
  // On-screen buttons for pen and touch: the same as right-click/Enter and Esc.
  void finishSketch();
  void cancelSketch();

signals:
  void geometryCaptured(const QgsGeometry& geom);
  // Finishing did not produce a shape (too few points, unusable geometry).
  void captureCanceled();
  // Esc or 취소 threw away the points drawn so far.
  void sketchCanceled();
  // The number of sketch points changed (0 after finishing, canceling or leaving).
  void sketchChanged(int pointCount);

private:
  void finish();
  void cancel();
  void rebuildRubber(const QgsPointXY* cursorOrNull);
  void destroyRubber();
  void updateSnapMarker(const QgsPointXY& mapPt, bool snapped, bool isIntersection = false);
  void destroySnapMarker();
  bool mapPointFromEvent(QgsMapMouseEvent* e, QgsPointXY* out, bool* snapped = nullptr,
                         bool* onVertex = nullptr);
  bool nearPoint(const QgsPointXY& a, const QgsPointXY& b) const;
  int indexOfSketchVertex(const QgsPointXY& pt) const;
  void announceSketch();
  void startDwell(const QgsPointXY& mapPt);
  void stopDwell();
  void addDwellPoint();

  Mode m_mode = Mode::Polygon;
  QPointer<QgsVectorLayer> m_layer;
  QgsRubberBand* m_rubber = nullptr;
  QgsVertexMarker* m_snapMark = nullptr;
  QVector<QgsPointXY> m_points;
  bool m_finishing = false;
  bool m_snapEnabled = true;
  bool m_easyDraw = false;
  int m_announcedCount = 0;
  QString m_repairNotice;
  // Easy draw adds a vertex snap on hover; an edge snap only after the pointer rests.
  QTimer* m_dwell = nullptr;
  QgsPointXY m_dwellPoint;
  bool m_dwellValid = false;
  Qt::ContextMenuPolicy m_savedMenuPolicy = Qt::DefaultContextMenu;
};
