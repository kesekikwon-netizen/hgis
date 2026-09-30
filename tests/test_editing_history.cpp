// Edits on several layers and the closing of a sketched polygon (package B1).
#include <QtTest>
#include <QFile>
#include <QUndoStack>

#include "core/EditGeometryRepair.h"
#include "core/EditHistory.h"

#include <qgsapplication.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgsvectorlayer.h>

namespace {
std::unique_ptr<QgsVectorLayer> memoryPolygons(const QString& name) {
  return std::make_unique<QgsVectorLayer>(
      QStringLiteral("Polygon?crs=EPSG:5186&field=note:string"), name, QStringLiteral("memory"));
}

bool addSquare(QgsVectorLayer* layer, double x) {
  layer->beginEditCommand(QStringLiteral("도형 그리기"));
  QgsFeature feature(layer->fields());
  feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(x, 0, x + 10, 10)));
  const bool ok = layer->addFeature(feature);
  layer->endEditCommand();
  return ok;
}
}  // namespace

class TestEditingHistory : public QObject {
  Q_OBJECT
private slots:
  void stampsOrderEditsAcrossLayers();
  void commandStateFollowsUndoCommitAndDiscard();
  void saveThatDropsTheBufferKeepsWrittenCommandsCommitted();
  void redoPrefersTheLastUndone();
  void bowTieKeepsLargestPartAndSaysSo();
  void nearlyStraightRingIsFlagged();
  void plainSquareHasNoNotice();
};

void TestEditingHistory::stampsOrderEditsAcrossLayers() {
  auto a = memoryPolygons(QStringLiteral("A"));
  auto b = memoryPolygons(QStringLiteral("B"));
  QVERIFY(a->isValid() && b->isValid());
  EditHistory history;
  history.watch(a.get());
  history.watch(b.get());
  history.watch(a.get());  // idempotent
  QVERIFY(a->startEditing() && b->startEditing());

  QVERIFY(addSquare(a.get(), 0));
  const EditHistory::Stamp first = history.undoTop(a.get());
  QVERIFY(first.id > 0 && first.recent > 0);
  QVERIFY(addSquare(b.get(), 20));
  const EditHistory::Stamp second = history.undoTop(b.get());
  QVERIFY2(second.recent > first.recent, "a later edit on another layer is newer");
  const quint64 removal = history.next();  // e.g. a layer removed from the tree
  QVERIFY(removal > second.recent);
  QVERIFY(addSquare(a.get(), 40));
  QVERIFY(history.undoTop(a.get()).recent > removal);
  QVERIFY(history.redoTop(a.get()).command == nullptr);
  QVERIFY(a->rollBack() && b->rollBack());
}

void TestEditingHistory::commandStateFollowsUndoCommitAndDiscard() {
  auto layer = memoryPolygons(QStringLiteral("유구면"));
  EditHistory history;
  history.watch(layer.get());
  QVERIFY(layer->startEditing());
  QVERIFY(addSquare(layer.get(), 0));
  const quint64 kept = history.undoTop(layer.get()).id;
  QVERIFY(addSquare(layer.get(), 20));
  const quint64 undone = history.undoTop(layer.get()).id;
  QCOMPARE(history.state(kept), EditHistory::State::Applied);

  layer->undoStack()->undo();
  QCOMPARE(history.state(undone), EditHistory::State::Undone);
  QCOMPARE(history.undoTop(layer.get()).id, kept);
  QCOMPARE(history.redoTop(layer.get()).id, undone);

  // A new edit throws the undone one away: its fallback must never be applied.
  QVERIFY(addSquare(layer.get(), 40));
  QCOMPARE(history.state(undone), EditHistory::State::Discarded);
  const quint64 last = history.undoTop(layer.get()).id;

  // 저장 writes what is applied: those commands leave the stack as committed.
  QVERIFY(layer->commitChanges(false));
  QCOMPARE(history.state(kept), EditHistory::State::Committed);
  QCOMPARE(history.state(last), EditHistory::State::Committed);
  QVERIFY(history.lastRecent(last) > history.lastRecent(kept));
  QVERIFY(history.undoTop(layer.get()).command == nullptr);
  QCOMPARE(history.state(0), EditHistory::State::Unknown);
  QVERIFY(layer->rollBack());
}

void TestEditingHistory::saveThatDropsTheBufferKeepsWrittenCommandsCommitted() {
  // 저장 writes the buffer into the next survey generation, then drops it with
  // rollBack (every command undone) and clears the stack. The written commands must
  // still count as Committed so Ctrl+Z after the save can use their fallbacks; the
  // one undone before the save was not written and stays Discarded.
  auto layer = memoryPolygons(QStringLiteral("유구면"));
  EditHistory history;
  history.watch(layer.get());
  QVERIFY(layer->startEditing());
  QVERIFY(addSquare(layer.get(), 0));
  const quint64 written = history.undoTop(layer.get()).id;
  QVERIFY(addSquare(layer.get(), 20));
  const quint64 undoneBefore = history.undoTop(layer.get()).id;
  layer->undoStack()->undo();
  QCOMPARE(history.state(undoneBefore), EditHistory::State::Undone);

  history.commitApplied(layer.get());  // what beforeRollBack / beforeCommitChanges report
  QVERIFY(layer->rollBack(false));
  layer->undoStack()->clear();
  QCOMPARE(history.state(written), EditHistory::State::Committed);
  QCOMPARE(history.state(undoneBefore), EditHistory::State::Discarded);
  QVERIFY(history.lastRecent(written) > 0);

  // A plain rollBack (a real discard) marks nothing as written.
  QVERIFY(layer->isEditable());
  QVERIFY(addSquare(layer.get(), 40));
  const quint64 discarded = history.undoTop(layer.get()).id;
  QVERIFY(layer->rollBack(false));
  layer->undoStack()->clear();
  QCOMPARE(history.state(discarded), EditHistory::State::Discarded);
  QVERIFY(layer->rollBack());
}

void TestEditingHistory::redoPrefersTheLastUndone() {
  auto a = memoryPolygons(QStringLiteral("A"));
  auto b = memoryPolygons(QStringLiteral("B"));
  EditHistory history;
  history.watch(a.get());
  history.watch(b.get());
  QVERIFY(a->startEditing() && b->startEditing());
  QVERIFY(addSquare(a.get(), 0));
  QVERIFY(addSquare(b.get(), 20));
  b->undoStack()->undo();
  a->undoStack()->undo();
  QVERIFY2(history.redoTop(a.get()).undone > history.redoTop(b.get()).undone,
           "the edit undone last is redone first");
  a->undoStack()->redo();
  QCOMPARE(history.state(history.undoTop(a.get()).id), EditHistory::State::Applied);
  QVERIFY2(history.undoTop(a.get()).recent > history.redoTop(b.get()).undone,
           "a redo is newer than the undo before it");
  QVERIFY(a->rollBack() && b->rollBack());
}

void TestEditingHistory::bowTieKeepsLargestPartAndSaysSo() {
  // (0,0)-(10,4) crosses (10,0)-(0,10): a large triangle on the left (about 35.7 m²)
  // and a small one on the right (about 5.7 m²).
  const QVector<QgsPointXY> ring{QgsPointXY(0, 0), QgsPointXY(10, 4), QgsPointXY(10, 0),
                                 QgsPointXY(0, 10)};
  const EditGeometryRepair::PolygonResult result = EditGeometryRepair::closePolygon(ring, 0.01);
  QVERIFY(!result.geometry.isEmpty());
  QCOMPARE(result.geometry.type(), Qgis::GeometryType::Polygon);
  QVERIFY(!result.geometry.isMultipart());
  QVERIFY(result.repaired);
  QCOMPARE(result.droppedParts, 1);
  QVERIFY2(std::abs(result.keptArea - 250.0 / 7.0) < 1e-6, qPrintable(QString::number(result.keptArea)));
  QVERIFY2(std::abs(result.partsArea - result.keptArea - 40.0 / 7.0) < 1e-6,
           qPrintable(QString::number(result.partsArea)));
  QVERIFY(std::abs(result.geometry.area() - result.keptArea) < 1e-9);
  const QString notice = EditGeometryRepair::notice(result);
  QVERIFY2(notice.contains(QStringLiteral("1개")), qPrintable(notice));
}

void TestEditingHistory::nearlyStraightRingIsFlagged() {
  const QVector<QgsPointXY> ring{QgsPointXY(0, 0), QgsPointXY(50, 0.0001), QgsPointXY(100, 0)};
  const EditGeometryRepair::PolygonResult result = EditGeometryRepair::closePolygon(ring, 0.01);
  QVERIFY(!result.geometry.isEmpty());
  QVERIFY(result.degenerate);
  QVERIFY(!EditGeometryRepair::notice(result).isEmpty());
}

void TestEditingHistory::plainSquareHasNoNotice() {
  const QVector<QgsPointXY> ring{QgsPointXY(0, 0), QgsPointXY(0, 10), QgsPointXY(10, 10),
                                 QgsPointXY(10, 10.001), QgsPointXY(10, 0)};
  const EditGeometryRepair::PolygonResult result = EditGeometryRepair::closePolygon(ring, 0.01);
  QVERIFY(!result.repaired);
  QCOMPARE(result.droppedParts, 0);
  QVERIFY(!result.degenerate);
  QVERIFY(std::abs(result.keptArea - 100.0) < 1e-6);
  QVERIFY(EditGeometryRepair::notice(result).isEmpty());
  QVERIFY(EditGeometryRepair::closePolygon({QgsPointXY(0, 0), QgsPointXY(1, 1)}, 0.01)
              .geometry.isEmpty());
}

#include "test_editing_history.moc"

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  const QString prefix = qEnvironmentVariable(
      "QGIS_PREFIX_PATH", QFile::exists(QStringLiteral("A:/OSGeo4W/apps/qgis-dev"))
                              ? QStringLiteral("A:/OSGeo4W/apps/qgis-dev")
                              : QStringLiteral("C:/OSGeo4W/apps/qgis-dev"));
  QgsApplication::setPrefixPath(prefix, true);
  QgsApplication::setPluginPath(prefix + QStringLiteral("/plugins"));
  QgsApplication::initQgis();
  TestEditingHistory tc;
  const int rc = QTest::qExec(&tc, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
