// Trial-trench editing (package B2): moves and deletes are undoable edit-buffer commands
// that write nothing until 저장, re-generation replaces the grid as one Ctrl+Z step and
// knows hand edits, snapping uses one reference, and the dialog preview is debounced,
// computed off the UI thread and cached without changing the 10%/2% result.
#include <QtTest>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QUndoStack>

#include "app/KaTrenchDialog.h"
#include "app/KaTrenchMoveTool.h"
#include "core/LayerOps.h"
#include "core/TrenchGridGenerator.h"
#include "core/TrenchLayerEdit.h"
#include "core/TrenchPlanCache.h"

#include <qgsapplication.h>
#include <qgsgeometry.h>
#include <qgsmapcanvas.h>
#include <qgsmapmouseevent.h>
#include <qgsmaptopixel.h>
#include <qgsproject.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorlayer.h>

namespace {
struct Canvas {
  QgsMapCanvas canvas;
  Canvas() {
    canvas.setRenderFlag(false);
    canvas.setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    canvas.resize(640, 480);
    canvas.mapSettings().setOutputSize(QSize(640, 480));
    canvas.setExtent(QgsRectangle(200000, 450000, 200100, 450075));
  }
};

QgsVectorLayer* trenchLayer(const QList<QgsRectangle>& rects) {
  auto* layer = new QgsVectorLayer(
      QStringLiteral("Polygon?crs=EPSG:5186&field=name:string&field=width:double&field=length:double"),
      QStringLiteral("시굴격자"), QStringLiteral("memory"));
  LayerOps::markSurveyLayer(layer, QStringLiteral("trial_trench"));
  int n = 1;
  for (const QgsRectangle& rect : rects) {
    QgsFeature feature(layer->fields());
    feature.setGeometry(QgsGeometry::fromRect(rect));
    feature.setAttribute(QStringLiteral("name"), QStringLiteral("Tr-%1").arg(n++));
    feature.setAttribute(QStringLiteral("width"), rect.width());
    feature.setAttribute(QStringLiteral("length"), rect.height());
    layer->dataProvider()->addFeature(feature);
  }
  layer->updateExtents();
  QgsProject::instance()->addMapLayer(layer);
  return layer;
}

void mouse(QgsMapTool& tool, QgsMapCanvas* canvas, QEvent::Type type, const QgsPointXY& at,
           Qt::MouseButton button) {
  const QgsPointXY px = canvas->getCoordinateTransform()->transform(at);
  const Qt::MouseButtons held = type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::MouseButtons(button);
  QgsMapMouseEvent event(canvas, type, QPoint(qRound(px.x()), qRound(px.y())),
                         type == QEvent::MouseMove ? Qt::NoButton : button, held);
  event.setMapPoint(at);  // exact map point, not the nearest pixel
  if (type == QEvent::MouseButtonPress) tool.canvasPressEvent(&event);
  else if (type == QEvent::MouseMove) tool.canvasMoveEvent(&event);
  else tool.canvasReleaseEvent(&event);
}

QgsRectangle boxOf(QgsVectorLayer* layer, QgsFeatureId fid) { return layer->getFeature(fid).geometry().boundingBox(); }

QStringList names(QgsVectorLayer* layer) {
  QStringList out;
  QgsFeature feature;
  auto it = layer->getFeatures();
  while (it.nextFeature(feature)) out << feature.attribute(QStringLiteral("name")).toString();
  out.sort();
  return out;
}

QByteArray squareWkb(double size) { return QgsGeometry::fromRect(QgsRectangle(0, 0, size, size)).asWkb(); }
}  // namespace

class TestTrenchEdit : public QObject {
  Q_OBJECT
private slots:
  void cleanup() { QgsProject::instance()->removeAllMapLayers(); }

  // F003: right click deletes at once, Ctrl+Z restores, the file is untouched.
  void rightClickDeleteIsUndoableAndNotSaved() {
    Canvas c;
    auto* layer = trenchLayer({QgsRectangle(200010, 450010, 200012, 450030),
                               QgsRectangle(200020, 450010, 200022, 450030)});
    KaTrenchMoveTool tool(&c.canvas);
    tool.setLayer(layer);
    tool.setMode(KaTrenchMoveTool::Mode::Whole);  // both modes delete on right click
    QSignalSpy edited(&tool, &KaTrenchMoveTool::trenchesEdited);
    mouse(tool, &c.canvas, QEvent::MouseButtonPress, QgsPointXY(200011, 450020), Qt::RightButton);
    QCOMPARE(layer->featureCount(), 1);
    QCOMPARE(layer->dataProvider()->featureCount(), 2);  // nothing written until 저장
    QVERIFY(layer->isModified());
    QCOMPARE(edited.count(), 1);
    QVERIFY(layer->undoStack()->canUndo());
    layer->undoStack()->undo();
    QCOMPARE(layer->featureCount(), 2);
  }

  // F003 + F046: a single move is one undo step and moves whole grid steps from the
  // snapped click, not from the raw click (the old 0.4 m jump).
  void singleMoveUsesSnappedClickAndUndoes() {
    Canvas c;
    auto* layer = trenchLayer({QgsRectangle(200010, 450010, 200012, 450030)});
    const QgsFeatureId fid = *layer->allFeatureIds().constBegin();
    KaTrenchMoveTool tool(&c.canvas);
    tool.setLayer(layer);
    tool.setSnapMeters(1.0);
    tool.setMode(KaTrenchMoveTool::Mode::Single);
    mouse(tool, &c.canvas, QEvent::MouseButtonPress, QgsPointXY(200011.4, 450020.3), Qt::LeftButton);
    QCOMPARE(tool.selectedTrench(), fid);
    mouse(tool, &c.canvas, QEvent::MouseMove, QgsPointXY(200014.4, 450020.3), Qt::LeftButton);
    mouse(tool, &c.canvas, QEvent::MouseButtonRelease, QgsPointXY(200014.4, 450020.3), Qt::LeftButton);
    const QgsRectangle moved = boxOf(layer, fid);
    QVERIFY2(qAbs(moved.xMinimum() - 200013.0) < 1e-9, qPrintable(QString::number(moved.xMinimum(), 'f', 3)));
    QVERIFY(qAbs(moved.yMinimum() - 450010.0) < 1e-9);
    QCOMPARE(layer->dataProvider()->featureCount(), 1);
    QCOMPARE(layer->undoStack()->count(), 1);
    layer->undoStack()->undo();
    QVERIFY(qAbs(boxOf(layer, fid).xMinimum() - 200010.0) < 1e-9);
  }

  void wholeMoveIsOneUndoStep() {
    Canvas c;
    auto* layer = trenchLayer({QgsRectangle(200010, 450010, 200012, 450030),
                               QgsRectangle(200020, 450010, 200022, 450030)});
    KaTrenchMoveTool tool(&c.canvas);
    tool.setLayer(layer);
    tool.setMode(KaTrenchMoveTool::Mode::Whole);
    mouse(tool, &c.canvas, QEvent::MouseButtonPress, QgsPointXY(200050, 450050), Qt::LeftButton);
    mouse(tool, &c.canvas, QEvent::MouseButtonRelease, QgsPointXY(200055, 450047), Qt::LeftButton);
    QCOMPARE(layer->undoStack()->count(), 1);
    QgsFeature feature;
    auto it = layer->getFeatures();
    while (it.nextFeature(feature)) {
      const QgsRectangle box = feature.geometry().boundingBox();
      QVERIFY(qAbs(box.yMinimum() - 450007.0) < 1e-9);
      QVERIFY(qAbs(box.xMinimum() - 200015.0) < 1e-9 || qAbs(box.xMinimum() - 200025.0) < 1e-9);
    }
    layer->undoStack()->undo();
    QCOMPARE(names(layer), QStringList({QStringLiteral("Tr-1"), QStringLiteral("Tr-2")}));
    auto back = layer->getFeatures();
    while (back.nextFeature(feature))
      QVERIFY(qAbs(feature.geometry().boundingBox().yMinimum() - 450010.0) < 1e-9);
  }

  // Unsaved trenches carry negative ids; they must still be picked and deleted.
  void newUnsavedTrenchesCanBePickedAndDeleted() {
    Canvas c;
    auto* layer = trenchLayer({});
    TrenchGridGenerator::Spec spec;
    spec.originX = 200010;
    spec.originY = 450010;
    spec.cols = 2;
    spec.trenchLength = 10;
    spec.balkWidth = 3;
    QString error;
    QVERIFY2(TrenchLayerEdit::replaceWithCells(layer, TrenchGridGenerator::build(spec),
                                               QStringLiteral("EPSG:5186"), &error), qPrintable(error));
    QCOMPARE(layer->featureCount(), 2);
    KaTrenchMoveTool tool(&c.canvas);
    tool.setLayer(layer);
    tool.setMode(KaTrenchMoveTool::Mode::Single);
    mouse(tool, &c.canvas, QEvent::MouseButtonPress, QgsPointXY(200011, 450015), Qt::LeftButton);
    mouse(tool, &c.canvas, QEvent::MouseButtonRelease, QgsPointXY(200011, 450015), Qt::LeftButton);
    QVERIFY(!FID_IS_NULL(tool.selectedTrench()));
    mouse(tool, &c.canvas, QEvent::MouseButtonPress, QgsPointXY(200011, 450015), Qt::RightButton);
    QCOMPARE(layer->featureCount(), 1);
    QVERIFY(FID_IS_NULL(tool.selectedTrench()));
  }

  // F156: re-generation replaces the grid as one command; hand edits are told apart.
  void replaceIsOneUndoStepAndTracksHandEdits() {
    auto* layer = trenchLayer({QgsRectangle(200010, 450010, 200012, 450030),
                               QgsRectangle(200020, 450010, 200022, 450030)});
    QVERIFY(TrenchLayerEdit::hasHandAdjustments(layer));  // no record of a placed grid
    QVERIFY(!TrenchLayerEdit::hasUnsavedHandEdits(layer));
    TrenchGridGenerator::Spec spec;
    spec.originX = 200040;
    spec.originY = 450010;
    spec.cols = 3;
    QString error;
    QVERIFY2(TrenchLayerEdit::replaceWithCells(layer, TrenchGridGenerator::build(spec),
                                               QStringLiteral("EPSG:5186"), &error), qPrintable(error));
    QCOMPARE(layer->featureCount(), 3);
    QCOMPARE(layer->dataProvider()->featureCount(), 2);  // nothing written until 저장
    QCOMPARE(layer->undoStack()->count(), 1);
    QVERIFY(!TrenchLayerEdit::hasHandAdjustments(layer));
    QVERIFY(!TrenchLayerEdit::hasUnsavedHandEdits(layer));
    const QgsFeatureId one = *layer->allFeatureIds().constBegin();
    QVERIFY(TrenchLayerEdit::translate(layer, {one}, 1.0, 0.0, QStringLiteral("트렌치 이동"), &error));
    QVERIFY(TrenchLayerEdit::hasHandAdjustments(layer));
    QVERIFY(TrenchLayerEdit::hasUnsavedHandEdits(layer));
    layer->undoStack()->undo();
    QVERIFY(!TrenchLayerEdit::hasHandAdjustments(layer));
    layer->undoStack()->undo();  // the grid before re-generation comes back
    QCOMPARE(layer->featureCount(), 2);
    QCOMPARE(names(layer), QStringList({QStringLiteral("Tr-1"), QStringLiteral("Tr-2")}));
  }

  void replaceRefusesOtherCrsAndInvalidCells() {
    auto* layer = trenchLayer({QgsRectangle(200010, 450010, 200012, 450030)});
    TrenchGridGenerator::Spec spec;
    spec.originX = 200040;
    spec.originY = 450010;
    const auto cells = TrenchGridGenerator::build(spec);
    QString error;
    QVERIFY(!TrenchLayerEdit::replaceWithCells(layer, cells, QStringLiteral("EPSG:5187"), &error));
    QVERIFY(error.contains(QStringLiteral("좌표계")));
    auto wide = cells;
    wide[0].width = 3.0;  // over the 2 m limit
    QVERIFY(!TrenchLayerEdit::replaceWithCells(layer, wide, QStringLiteral("EPSG:5186"), &error));
    QCOMPARE(layer->featureCount(), 1);
    QVERIFY(!layer->isModified());
  }

  // F022: ratio plans name trenches by kind, so the prefix row is hidden there.
  void dialogHidesPrefixInRatioMode() {
    KaTrenchDialog dialog;
    dialog.setArea(squareWkb(100), 10000.0);
    auto* form = qobject_cast<QFormLayout*>(dialog.layout());
    QLineEdit* prefix = nullptr;  // spin boxes carry their own line edits
    for (auto* edit : dialog.findChildren<QLineEdit*>())
      if (edit->text() == QStringLiteral("Tr-")) prefix = edit;
    QVERIFY(form && prefix);
    QVERIFY(dialog.autoFill() && dialog.ratioMode());
    QVERIFY(!form->isRowVisible(prefix));
    QComboBox* kind = nullptr;
    for (auto* combo : dialog.findChildren<QComboBox*>())
      if (combo->count() == 3) kind = combo;
    QVERIFY(kind);
    kind->setCurrentIndex(2);  // 직접 지정
    QVERIFY(!dialog.ratioMode());
    QVERIFY(form->isRowVisible(prefix));
    QTRY_VERIFY_WITH_TIMEOUT(!dialog.planPending(), 20000);
  }

  // F097: five quick azimuth steps search once; the apply path reuses the same plan.
  void dialogPreviewRunsOnceAfterInputSettles() {
    TrenchPlanCache::clear();
    KaTrenchDialog dialog;
    dialog.setArea(squareWkb(100), 10000.0);
    QDoubleSpinBox* azimuth = nullptr;
    for (auto* spin : dialog.findChildren<QDoubleSpinBox*>())
      if (spin->suffix().contains(QStringLiteral("°"))) azimuth = spin;
    QVERIFY(azimuth);
    for (int step = 1; step <= 5; ++step) azimuth->setValue(step);
    QVERIFY(dialog.planPending());
    QTRY_VERIFY_WITH_TIMEOUT(!dialog.planPending(), 20000);
    QCOMPARE(TrenchPlanCache::computeCount(), 1);
    auto* summary = dialog.findChild<QLabel*>(QStringLiteral("trenchSummary"));
    QVERIFY(summary);
    QVERIFY2(summary->text().contains(QStringLiteral("목표 10%")), qPrintable(summary->text()));
    QVERIFY2(summary->text().contains(QStringLiteral("방위 5°")), qPrintable(summary->text()));
    const auto cached = TrenchPlanCache::ratioPlan(squareWkb(100), 10.0, 2.0, 5.0);
    QCOMPARE(TrenchPlanCache::computeCount(), 1);
    const auto direct = TrenchGridGenerator::buildForTargetRatio(squareWkb(100), 10.0, 2.0, 5.0);
    QCOMPARE(cached.cells.size(), direct.cells.size());
    QCOMPARE(TrenchGridGenerator::totalArea(cached.cells), TrenchGridGenerator::totalArea(direct.cells));
  }

  // F047: an invalid boundary says why in the preview instead of 「트렌치 0개」.
  void dialogShowsWhyInvalidAreaHasNoGrid() {
    KaTrenchDialog dialog;
    const QByteArray bowtie =
        QgsGeometry::fromWkt(QStringLiteral("POLYGON((0 0,100 100,100 0,0 100,0 0))")).asWkb();
    dialog.setArea(bowtie, 5000.0);
    QTRY_VERIFY_WITH_TIMEOUT(!dialog.planPending(), 20000);
    auto* summary = dialog.findChild<QLabel*>(QStringLiteral("trenchSummary"));
    QVERIFY(summary);
    QVERIFY2(summary->text().contains(QStringLiteral("유효하지 않은")), qPrintable(summary->text()));
  }
};

#include "test_trench_edit.moc"

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  const QString prefix = qEnvironmentVariable(
      "QGIS_PREFIX_PATH", QFile::exists(QStringLiteral("A:/OSGeo4W/apps/qgis-dev"))
                              ? QStringLiteral("A:/OSGeo4W/apps/qgis-dev")
                              : QStringLiteral("C:/OSGeo4W/apps/qgis-dev"));
  QgsApplication::setPrefixPath(prefix, true);
  QgsApplication::setPluginPath(prefix + QStringLiteral("/plugins"));
  QgsApplication::initQgis();
  TestTrenchEdit tc;
  const int rc = QTest::qExec(&tc, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
