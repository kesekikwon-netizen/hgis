// 도형선택 in the cases that used to pick nothing: a shape drawn but not saved yet (negative
// edit-buffer id), a layer lifted onto the above-labels overlay (not in the canvas list), and
// the inner pieces of one polygon (a hole, a part inside another part). Also key A.
#include <QtTest>
#include <QAction>
#include <QApplication>
#include <QFile>
#include <QMainWindow>
#include <QMenu>
#include <QSignalSpy>
#include <QTimer>
#include <QUndoStack>

#include "app/KaAttributeMapTool.h"
#include "app/KaCaptureMapTool.h"
#include "app/KaFeatureSelectTool.h"
#include "app/KaVertexEditTool.h"
#include "core/LayerOps.h"

#include <qgsapplication.h>
#include <qgsmapcanvas.h>
#include <qgsmapmouseevent.h>
#include <qgsmaptopixel.h>
#include <qgspallabeling.h>
#include <qgsproject.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayerlabeling.h>

namespace {
const QgsRectangle kExtent(200000, 450000, 200100, 450075);

QgsPolylineXY ring(double x, double y, double size) {
  return {{x, y}, {x + size, y}, {x + size, y + size}, {x, y + size}, {x, y}};
}
// A 60 m square with a 10 m hole at (30,30).
QgsGeometry holed() { return QgsGeometry::fromPolygonXY({ring(200010, 450010, 60), ring(200030, 450030, 10)}); }
// The same outlines stored as two overlapping parts of one shape.
QgsGeometry nested() {
  return QgsGeometry::fromMultiPolygonXY({{ring(200010, 450010, 60)}, {ring(200030, 450030, 10)}});
}
int ringCount(const QgsGeometry& g) {
  int rings = 0;
  for (const QgsPolygonXY& part : g.asMultiPolygon()) rings += part.size();
  return rings;
}

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

QgsVectorLayer* surveyArea(const QgsGeometry& shape) {
  auto* layer = new QgsVectorLayer(QStringLiteral("MultiPolygon?crs=EPSG:5186"), QStringLiteral("조사구역"),
                                   QStringLiteral("memory"));
  LayerOps::markSurveyLayer(layer, QStringLiteral("survey_area"));
  QgsFeature f(layer->fields());
  QgsGeometry g = shape;
  g.convertToMultiType();
  f.setGeometry(g);
  layer->dataProvider()->addFeature(f);
  layer->updateExtents();
  QgsProject::instance()->addMapLayer(layer);
  return layer;
}
QgsGeometry savedShape(QgsVectorLayer* layer) {
  QgsFeature f;
  layer->dataProvider()->getFeatures(QgsFeatureRequest(1)).nextFeature(f);  // what is on disk
  return f.geometry();
}
void click(QgsMapTool& tool, QgsMapCanvas* canvas, const QPoint& at) {
  QgsMapMouseEvent press(canvas, QEvent::MouseButtonPress, at, Qt::LeftButton, Qt::LeftButton);
  tool.canvasPressEvent(&press);
  QgsMapMouseEvent release(canvas, QEvent::MouseButtonRelease, at, Qt::LeftButton, Qt::NoButton);
  tool.canvasReleaseEvent(&release);
}
// Right-click and return the enabled entries of the menu that opens (closed unchosen).
QStringList rightClick(QgsMapTool& tool, QgsMapCanvas* canvas, const QPoint& at) {
  QStringList seen;  QTimer close;
  QObject::connect(&close, &QTimer::timeout, [&seen] {
    if (auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget())) {
      for (QAction* action : menu->actions()) seen << (action->isEnabled() ? action->text() : QString());
      menu->close();
    }
  });
  close.start(20);
  QgsMapMouseEvent release(canvas, QEvent::MouseButtonRelease, at, Qt::RightButton, Qt::NoButton);
  tool.canvasReleaseEvent(&release);
  return seen;
}
}  // namespace

class TestInnerPiece : public QObject {
  Q_OBJECT
private slots:
  void cleanup() { QgsProject::instance()->removeAllMapLayers(); }

  void unsavedShapeIsPickedAndEdited() {
    // Drawing adds to the edit buffer; until 저장 the shape has a negative id.
    Canvas view;
    QgsVectorLayer* layer = surveyArea(holed());
    view.canvas.setLayers({layer});
    QVERIFY(layer->startEditing());
    QgsFeature drawn(layer->fields());
    drawn.setGeometry(QgsGeometry::fromMultiPolygonXY({{ring(200080, 450010, 10)}}));
    QVERIFY(layer->addFeature(drawn));
    QVERIFY2(drawn.id() < 0, "fixture: an edit-buffer id");
    KaFeatureSelectTool tool(&view.canvas);

    click(tool, &view.canvas, view.px(200085, 450015));
    QCOMPARE(layer->selectedFeatureIds(), QgsFeatureIds{drawn.id()});
    auto* vertex = tool.findChild<KaVertexEditTool*>();
    QVERIFY2(vertex && vertex->hasTarget(), "its handles are up");
    QVERIFY(vertex->moveVertexTo(0, QgsPointXY(200079, 450009)));
    QVERIFY(vertex->deleteVertexAt(1));
    QCOMPARE(layer->getFeature(drawn.id()).geometry().constGet()->nCoordinates(), 4);
    vertex->clearTarget();
    QVERIFY(!vertex->hasTarget());
  }

  void shapeLiftedAboveLabelsIsStillPicked() {
    // A survey layer above a labelled layer is painted by the above-labels overlay and taken
    // out of the canvas list (LayerOps refresh). The click must still reach it.
    auto* labelled = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186&field=jibun:string"),
                                        QStringLiteral("지번"), QStringLiteral("memory"));
    QgsPalLayerSettings text;
    text.fieldName = QStringLiteral("jibun");
    labelled->setLabeling(new QgsVectorLayerSimpleLabeling(text));
    labelled->setLabelsEnabled(true);
    QgsProject::instance()->addMapLayer(labelled);
    QgsVectorLayer* layer = surveyArea(nested());  // added after it: above it in the layer tree
    QVERIFY2(LayerOps::layersDrawnAboveLabels(QgsProject::instance()).contains(layer),
             "fixture: the survey area is lifted above the labels");
    Canvas view;
    view.canvas.setLayers({labelled});  // what the app leaves on the canvas
    // 「이 도형 기록 입력」 and double-click pick with the same rules, the inner part standing for its shape.
    KaAttributeMapTool record(&view.canvas);
    QgsVectorLayer* picked = nullptr;  QgsFeature feature;
    QVERIFY(record.pickAtScreen(view.px(200035, 450035), &picked, &feature));
    QVERIFY(picked == layer && feature.id() == 1);
    KaFeatureSelectTool tool(&view.canvas);

    click(tool, &view.canvas, view.px(200015, 450015));
    QCOMPARE(layer->selectedFeatureCount(), 1);
    click(tool, &view.canvas, view.px(200035, 450035));
    QVERIFY2(tool.hasActivePiece(), "and so is its inner piece");
  }

  void clickInsideHolePicksItAndDeleteFillsIt() {
    Canvas view;
    QgsVectorLayer* layer = surveyArea(holed());
    view.canvas.setLayers({layer});
    KaFeatureSelectTool tool(&view.canvas);
    QSignalSpy edited(&tool, &KaFeatureSelectTool::featureGeometryEdited);

    click(tool, &view.canvas, view.px(200035, 450035));
    QCOMPARE(layer->selectedFeatureCount(), 1);
    QVERIFY2(tool.hasActivePiece(), "a click inside the hole picks the hole, not nothing");
    QVERIFY(tool.deleteActivePick());
    QVERIFY(!tool.hasActivePiece());
    QCOMPARE(ringCount(layer->getFeature(1).geometry()), 1);
    QVERIFY(qFuzzyCompare(layer->getFeature(1).geometry().area(), 3600.0));
    QCOMPARE(layer->featureCount(), 1LL);
    QCOMPARE(edited.count(), 1);
    QVERIFY2(layer->isEditable() && layer->undoStack()->count() == 1, "one undo step in the edit buffer");
    QCOMPARE(ringCount(savedShape(layer)), 2);  // nothing is written before 저장
    layer->undoStack()->undo();
    QCOMPARE(ringCount(layer->getFeature(1).geometry()), 2);
    QVERIFY2(!tool.deleteActivePick(), "nothing picked: Delete falls through to the whole shape");
  }

  void clickOnOuterAreaKeepsWholeShapeBehaviour() {
    Canvas view;
    QgsVectorLayer* layer = surveyArea(holed());
    view.canvas.setLayers({layer});
    KaFeatureSelectTool tool(&view.canvas);
    click(tool, &view.canvas, view.px(200015, 450015));
    QCOMPARE(layer->selectedFeatureCount(), 1);
    QVERIFY(!tool.hasActivePiece());
    QVERIFY(!tool.deleteActivePick());
    QCOMPARE(ringCount(layer->getFeature(1).geometry()), 2);
  }

  void innerPartCyclesToWholeShapeAndCutsOut() {
    Canvas view;
    QgsVectorLayer* layer = surveyArea(nested());
    view.canvas.setLayers({layer});
    KaFeatureSelectTool tool(&view.canvas);
    const QPoint inner = view.px(200035, 450035);

    click(tool, &view.canvas, inner);
    QVERIFY(tool.hasActivePiece());
    click(tool, &view.canvas, inner);  // same place again: the whole shape
    QCOMPARE(layer->selectedFeatureCount(), 1);
    QVERIFY(!tool.hasActivePiece());
    QVERIFY(!tool.cutOutActivePiece());
    click(tool, &view.canvas, inner);
    QVERIFY(tool.hasActivePiece());

    QVERIFY(tool.cutOutActivePiece());
    QVERIFY(qFuzzyCompare(layer->getFeature(1).geometry().area(), 3500.0));
    QCOMPARE(ringCount(layer->getFeature(1).geometry()), 2);
    // The hole left behind is itself a piece: one more click and Delete fills it again.
    click(tool, &view.canvas, inner);
    QVERIFY(tool.hasActivePiece());
    QVERIFY(tool.deleteActivePick());
    QVERIFY(qFuzzyCompare(layer->getFeature(1).geometry().area(), 3600.0));
    QCOMPARE(layer->undoStack()->count(), 2);
  }

  void anySurveyLayerGetsTheMenuAndARightClickStartsNoCycle() {
    Canvas view;
    QgsVectorLayer* layer = surveyArea(nested());
    LayerOps::markSurveyLayer(layer, QStringLiteral("user_poly_1"));  // not one of the domain layers
    view.canvas.setLayers({layer});
    KaFeatureSelectTool tool(&view.canvas);
    const QPoint inner = view.px(200035, 450035);
    QVERIFY(rightClick(tool, &view.canvas, inner).contains(QStringLiteral("안쪽 도형 지우기")));
    click(tool, &view.canvas, inner);
    QVERIFY2(tool.hasActivePiece(), "after a right-click the next click picks the best shape again");
    QVERIFY(rightClick(tool, &view.canvas, view.px(200010, 450010)).contains(QStringLiteral("점삭제")));
    // Handles exist only while 도형선택 is the map tool (an undo while drawing must not show them).
    auto* vertex = tool.findChild<KaVertexEditTool*>();
    QVERIFY(vertex && vertex->hasTarget());  tool.refreshSelectedGeometry();
    QVERIFY(!vertex->hasTarget());
    view.canvas.setMapTool(&tool);
    QVERIFY(vertex->hasTarget());
  }

  void keyASwitchesToSelectWithoutTogglingOrDroppingASketch() {
    QMainWindow window;
    auto* canvas = new QgsMapCanvas(&window);
    canvas->setObjectName(QStringLiteral("mapCanvas"));
    canvas->setRenderFlag(false);
    canvas->setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    canvas->setExtent(kExtent);
    window.setCentralWidget(canvas);
    QAction select(QStringLiteral("선택"));
    KaFeatureSelectTool::installKeyShortcut(&window, &select);
    auto* key = window.findChild<QAction*>(QStringLiteral("actSelectShapeKey"));
    QVERIFY(key);
    QCOMPARE(key->shortcut(), QKeySequence(Qt::Key_A));
    QVERIFY(select.toolTip().contains(QLatin1Char('A')));
    QSignalSpy started(&select, &QAction::triggered);

    key->trigger();
    QCOMPARE(started.count(), 1);

    KaFeatureSelectTool tool(canvas);
    canvas->setMapTool(&tool);
    key->trigger();
    QCOMPARE(started.count(), 1);  // already on: never toggled off

    QgsVectorLayer* layer = surveyArea(holed());
    KaCaptureMapTool capture(canvas);
    capture.setMode(KaCaptureMapTool::Mode::Polygon);
    capture.setTargetLayer(layer);
    capture.setSnapEnabled(false);
    canvas->setMapTool(&capture);
    key->trigger();
    QCOMPARE(started.count(), 2);  // nothing drawn yet: switching is fine
    click(capture, canvas, QPoint(40, 40));
    QVERIFY(capture.hasSketch());
    key->trigger();
    QCOMPARE(started.count(), 2);  // a half-drawn shape is kept

    select.setEnabled(false);
    canvas->unsetMapTool(&capture);
    key->trigger();
    QCOMPARE(started.count(), 2);  // no survey open: the action is disabled with its reason
  }
};

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
  TestInnerPiece tc;
  const int rc = QTest::qExec(&tc, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}

#include "test_inner_piece.moc"
