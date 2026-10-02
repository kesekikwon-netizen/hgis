// Drawing, select and vertex tools (package B1): no writes while drawing, a new shape may
// start on an existing corner, overlapping shapes can be picked in turn, Delete on a
// picked vertex removes only that vertex.
#include <QtTest>
#include <QFile>
#include <algorithm>
#include <memory>
#include <QSignalSpy>
#include <QUndoStack>

#include "app/KaAttributeMapTool.h"
#include "app/KaCaptureMapTool.h"
#include "app/KaEditTolerance.h"
#include "app/KaFeatureSelectTool.h"
#include "app/KaVertexEditTool.h"
#include "core/LayerOps.h"

#include <qgsapplication.h>
#include <qgsmapcanvas.h>
#include <qgsmapmouseevent.h>
#include <qgsmaptopixel.h>
#include <qgsproject.h>
#include <qgssnappingconfig.h>
#include <qgssnappingutils.h>
#include <qgsvectorlayer.h>

namespace {
const QgsRectangle kExtent(200000, 450000, 200100, 450075);

struct Canvas {
  QgsMapCanvas canvas;
  Canvas() {
    canvas.setRenderFlag(false);
    canvas.setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    canvas.resize(640, 480);
    canvas.mapSettings().setOutputSize(QSize(640, 480));
    canvas.setExtent(kExtent);
  }
  QPoint px(double x, double y) {
    const QgsPointXY p = canvas.getCoordinateTransform()->transform(x, y);
    return QPoint(qRound(p.x()), qRound(p.y()));
  }
};

QgsVectorLayer* surveyPolygons(const QList<QgsGeometry>& shapes) {
  auto* layer = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186&field=note:string"),
                                   QStringLiteral("유구면"), QStringLiteral("memory"));
  LayerOps::markSurveyLayer(layer, QStringLiteral("feature_poly"));
  for (const QgsGeometry& g : shapes) {
    QgsFeature f(layer->fields());
    f.setGeometry(g);
    layer->dataProvider()->addFeature(f);
  }
  layer->updateExtents();
  QgsProject::instance()->addMapLayer(layer);
  return layer;
}
QgsGeometry square(double x, double y, double size) {
  return QgsGeometry::fromRect(QgsRectangle(x, y, x + size, y + size));
}

void click(QgsMapTool& tool, QgsMapCanvas* canvas, const QPoint& at, Qt::MouseButton button = Qt::LeftButton) {
  QgsMapMouseEvent press(canvas, QEvent::MouseButtonPress, at, button, button);
  tool.canvasPressEvent(&press);
  QgsMapMouseEvent release(canvas, QEvent::MouseButtonRelease, at, button, Qt::NoButton);
  tool.canvasReleaseEvent(&release);
}

void hover(QgsMapTool& tool, QgsMapCanvas* canvas, const QPoint& at) {
  QgsMapMouseEvent move(canvas, QEvent::MouseMove, at, Qt::NoButton, Qt::NoButton);
  tool.canvasMoveEvent(&move);
}

QList<QgsFeatureId> selectedIds(QgsVectorLayer* layer) {
  QList<QgsFeatureId> ids = layer->selectedFeatureIds().values();
  std::sort(ids.begin(), ids.end());
  return ids;
}
}  // namespace

class TestEditingTools : public QObject {
  Q_OBJECT
private slots:
  void cleanup() { QgsProject::instance()->removeAllMapLayers(); }
  void captureStartsNewShapeOnSavedCornerWithoutWriting();
  void captureReportsBowTieAfterFinishingAndCancelsCleanly();
  void easyDrawTakesCornersAtOnceAndEdgesAfterRest();
  void selectPrefersSmallShapeAndCyclesOverlaps();
  void clickedHandleIsPickedAndDeleteRemovesOnlyThatVertex();
  void attributeEditingSkipsReferenceAndCadastral();
};

void TestEditingTools::captureStartsNewShapeOnSavedCornerWithoutWriting() {
  Canvas view;
  QgsVectorLayer* layer = surveyPolygons({square(200020, 450020, 20)});
  KaCaptureMapTool tool(&view.canvas);
  tool.setMode(KaCaptureMapTool::Mode::Polygon);
  tool.setTargetLayer(layer);
  tool.setSnapEnabled(false);
  QSignalSpy captured(&tool, &KaCaptureMapTool::geometryCaptured);
  QSignalSpy sketch(&tool, &KaCaptureMapTool::sketchChanged);

  // The first click lands exactly on a saved corner: a neighbour sharing that corner.
  click(tool, &view.canvas, view.px(200040, 450020));
  QCOMPARE(tool.pointCount(), 1);
  QVERIFY(tool.hasSketch());
  QVERIFY2(!layer->isEditable() && !layer->isModified(), "drawing must not touch saved shapes");
  click(tool, &view.canvas, view.px(200060, 450020));
  click(tool, &view.canvas, view.px(200060, 450040));
  QCOMPARE(sketch.count(), 3);
  QCOMPARE(sketch.last().at(0).toInt(), 3);
  click(tool, &view.canvas, view.px(200050, 450030), Qt::RightButton);

  QCOMPARE(captured.count(), 1);
  QCOMPARE(tool.pointCount(), 0);
  QCOMPARE(sketch.last().at(0).toInt(), 0);
  const QgsGeometry shape = captured.at(0).at(0).value<QgsGeometry>();
  QCOMPARE(shape.type(), Qgis::GeometryType::Polygon);
  QVERIFY(tool.lastRepairNotice().isEmpty());
  const double mupp = view.canvas.mapUnitsPerPixel();
  QVERIFY(QgsPointXY(shape.vertexAt(0)).distance(QgsPointXY(200040, 450020)) <= mupp);
  QVERIFY2(!layer->isEditable() && !layer->isModified() && layer->undoStack()->count() == 0,
           "the tool hands the shape over; it never commits or edits the layer itself");
}

void TestEditingTools::captureReportsBowTieAfterFinishingAndCancelsCleanly() {
  Canvas view;
  KaCaptureMapTool tool(&view.canvas);
  tool.setMode(KaCaptureMapTool::Mode::Polygon);
  tool.setSnapEnabled(false);
  QSignalSpy captured(&tool, &KaCaptureMapTool::geometryCaptured);
  QSignalSpy failed(&tool, &KaCaptureMapTool::captureCanceled);
  QSignalSpy canceled(&tool, &KaCaptureMapTool::sketchCanceled);

  click(tool, &view.canvas, view.px(200010, 450010));
  click(tool, &view.canvas, view.px(200030, 450010));
  tool.finishSketch();  // on-screen 완료 with too few points keeps the sketch
  QCOMPARE(failed.count(), 1);
  QCOMPARE(tool.pointCount(), 2);
  QVERIFY(tool.undoLastVertex());
  QCOMPARE(tool.pointCount(), 1);
  tool.cancelSketch();
  QCOMPARE(canceled.count(), 1);
  QCOMPARE(tool.pointCount(), 0);
  tool.cancelSketch();
  QCOMPARE(canceled.count(), 1);

  // Crossing edges: only the larger part is stored, and the tool says so afterwards.
  click(tool, &view.canvas, view.px(200010, 450010));
  click(tool, &view.canvas, view.px(200050, 450026));
  click(tool, &view.canvas, view.px(200050, 450010));
  click(tool, &view.canvas, view.px(200010, 450050));
  tool.finishSketch();
  QCOMPARE(captured.count(), 1);
  QVERIFY(!captured.at(0).at(0).value<QgsGeometry>().isMultipart());
  QVERIFY2(!tool.lastRepairNotice().isEmpty(), "a dropped part must be reported");
}

void TestEditingTools::easyDrawTakesCornersAtOnceAndEdgesAfterRest() {
  Canvas view;
  auto* line = new QgsVectorLayer(QStringLiteral("LineString?crs=EPSG:5186"), QStringLiteral("경계선"),
                                  QStringLiteral("memory"));
  QgsFeature f(line->fields());
  f.setGeometry(QgsGeometry::fromPolylineXY({QgsPointXY(200010, 450030), QgsPointXY(200090, 450030)}));
  line->dataProvider()->addFeature(f);
  line->updateExtents();
  QgsProject::instance()->addMapLayer(line);
  view.canvas.setLayers({line});
  QgsSnappingConfig config(QgsProject::instance());
  config.setEnabled(true);
  config.setMode(Qgis::SnappingMode::AllLayers);
  config.setTypeFlag(Qgis::SnappingType::Vertex | Qgis::SnappingType::Segment);
  config.setTolerance(12);
  config.setUnits(Qgis::MapToolUnit::Pixels);
  QgsSnappingUtils* snapping = view.canvas.snappingUtils();
  snapping->setConfig(config);
  snapping->setIndexingStrategy(QgsSnappingUtils::IndexAlwaysFull);
  snapping->setMapSettings(view.canvas.mapSettings());
  if (!snapping->snapToMap(view.px(200090, 450030)).isValid())
    QSKIP("snapping index is not available in this environment");

  KaCaptureMapTool tool(&view.canvas);
  tool.setMode(KaCaptureMapTool::Mode::Polygon);
  tool.setEasyDraw(true);
  click(tool, &view.canvas, view.px(200050, 450060));
  QCOMPARE(tool.pointCount(), 1);
  // Sweeping along an edge must not drop points on the way.
  hover(tool, &view.canvas, view.px(200040, 450030));
  hover(tool, &view.canvas, view.px(200050, 450030));
  QCOMPARE(tool.pointCount(), 1);
  // Resting on the edge takes that point.
  QTest::qWait(KaEditTolerance::kEasyDrawDwellMs + 300);
  QCOMPARE(tool.pointCount(), 2);
  // A corner of another shape is taken as soon as the pointer reaches it.
  hover(tool, &view.canvas, view.px(200090, 450030));
  QCOMPARE(tool.pointCount(), 3);
}

void TestEditingTools::selectPrefersSmallShapeAndCyclesOverlaps() {
  Canvas view;
  auto* reference = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"), QStringLiteral("참고 면"),
                                       QStringLiteral("memory"));
  LayerOps::markReferenceLayer(reference);
  QgsFeature cover(reference->fields());
  cover.setGeometry(QgsGeometry::fromRect(kExtent));
  reference->dataProvider()->addFeature(cover);
  QgsProject::instance()->addMapLayer(reference);
  QgsVectorLayer* layer = surveyPolygons({square(200010, 450010, 60), square(200030, 450030, 10)});
  view.canvas.setLayers({reference, layer});  // the reference map is on top
  QgsFeatureId house = -1, pit = -1;
  QgsFeature feature;
  for (auto it = layer->getFeatures(); it.nextFeature(feature);)
    (feature.geometry().area() > 1000 ? house : pit) = feature.id();

  KaFeatureSelectTool tool(&view.canvas);
  const QPoint pitCenter = view.px(200035, 450035);
  click(tool, &view.canvas, pitCenter);
  QCOMPARE(selectedIds(layer), QList<QgsFeatureId>{pit});
  QVERIFY2(reference->selectedFeatureIds().isEmpty(), "reference maps are read-only, never picked");
  click(tool, &view.canvas, pitCenter);
  QCOMPARE(selectedIds(layer), QList<QgsFeatureId>{house});
  click(tool, &view.canvas, pitCenter);
  QCOMPARE(selectedIds(layer), QList<QgsFeatureId>{pit});
  QVERIFY(!KaFeatureSelectTool::isPickableLayer(reference));
  QVERIFY(KaFeatureSelectTool::isPickableLayer(layer));
}

void TestEditingTools::clickedHandleIsPickedAndDeleteRemovesOnlyThatVertex() {
  Canvas view;
  const QgsPolylineXY ring{QgsPointXY(200020, 450020), QgsPointXY(200060, 450020),
                           QgsPointXY(200070, 450045), QgsPointXY(200040, 450065),
                           QgsPointXY(200015, 450045), QgsPointXY(200020, 450020)};
  QgsVectorLayer* layer = surveyPolygons({QgsGeometry::fromPolygonXY({ring})});
  view.canvas.setLayers({layer});
  const QgsFeatureId fid = *layer->allFeatureIds().constBegin();
  KaFeatureSelectTool tool(&view.canvas);
  click(tool, &view.canvas, view.px(200040, 450040));
  QCOMPARE(selectedIds(layer), QList<QgsFeatureId>{fid});

  // A click on a handle only picks it: no edit, nothing to undo.
  click(tool, &view.canvas, view.px(200060, 450020));
  QVERIFY(!layer->isModified());
  QVERIFY(!layer->undoStack() || layer->undoStack()->count() == 0);
  QVERIFY(tool.deleteActiveVertex());
  QCOMPARE(layer->getFeature(fid).geometry().constGet()->nCoordinates(), 5);
  QVERIFY2(layer->isModified() && layer->undoStack()->count() == 1,
           "the vertex removal is one undoable step in the edit buffer");
  QVERIFY2(!tool.deleteActiveVertex(), "the picked vertex is gone; Delete falls back to shapes");

  // Dragging without Ctrl puts the vertex under the cursor.
  const QPoint from = view.px(200070, 450045);
  const QPoint to = from + QPoint(30, 12);
  QgsMapMouseEvent press(&view.canvas, QEvent::MouseButtonPress, from, Qt::LeftButton, Qt::LeftButton);
  tool.canvasPressEvent(&press);
  QgsMapMouseEvent move(&view.canvas, QEvent::MouseMove, to, Qt::NoButton, Qt::LeftButton);
  tool.canvasMoveEvent(&move);
  QgsMapMouseEvent release(&view.canvas, QEvent::MouseButtonRelease, to, Qt::LeftButton, Qt::NoButton);
  tool.canvasReleaseEvent(&release);
  const QgsPointXY expected = view.canvas.getCoordinateTransform()->toMapCoordinates(to);
  bool found = false;
  const QgsGeometry moved = layer->getFeature(fid).geometry();
  for (auto it = moved.vertices_begin(); it != moved.vertices_end(); ++it)
    found |= QgsPointXY((*it).x(), (*it).y()).distance(expected) < 1e-6;
  QVERIFY(found);
  QCOMPARE(layer->undoStack()->count(), 2);
  QVERIFY(layer->rollBack());
}

void TestEditingTools::attributeEditingSkipsReferenceAndCadastral() {
  QgsVectorLayer* survey = surveyPolygons({square(200010, 450010, 10)});
  auto reference = std::make_unique<QgsVectorLayer>(QStringLiteral("Polygon?crs=EPSG:5186"),
                                                    QStringLiteral("주변유적"), QStringLiteral("memory"));
  LayerOps::markReferenceLayer(reference.get());
  auto cadastral = std::make_unique<QgsVectorLayer>(QStringLiteral("Polygon?crs=EPSG:5186"),
                                                    QStringLiteral("지적도 · 조사 주변 5km"),
                                                    QStringLiteral("memory"));
  LayerOps::markCadastralLayer(cadastral.get());
  QVERIFY(KaAttributeMapTool::isEditableLayer(survey));
  QVERIFY(!KaAttributeMapTool::isEditableLayer(reference.get()));
  QVERIFY(!KaAttributeMapTool::isEditableLayer(cadastral.get()));
  QVERIFY(!KaAttributeMapTool::isEditableLayer(nullptr));
}
#include "test_editing_tools.moc"

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  const QString prefix = qEnvironmentVariable(
      "QGIS_PREFIX_PATH", QFile::exists(QStringLiteral("A:/OSGeo4W/apps/qgis-dev"))
                              ? QStringLiteral("A:/OSGeo4W/apps/qgis-dev")
                              : QStringLiteral("C:/OSGeo4W/apps/qgis-dev"));
  QgsApplication::setPrefixPath(prefix, true);
  QgsApplication::setPluginPath(prefix + QStringLiteral("/plugins"));
  QgsApplication::initQgis();
  qRegisterMetaType<QgsGeometry>("QgsGeometry");
  TestEditingTools tc;
  const int rc = QTest::qExec(&tc, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
