// Ctrl+Z for layer order / check marks, and the merged layer-change refresh
// (evaluation F186, F131).
#include <QtTest>
#include <qgsapplication.h>
#include <qgslayertree.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#include "app/KaLayerRefreshBatch.h"
#include "app/KaLayerTreeUndo.h"
#include "core/LayerOps.h"

namespace {
QgsVectorLayer* addLayer(QgsProject& project, QgsLayerTreeGroup* group, const QString& name) {
  auto* layer = new QgsVectorLayer(QStringLiteral("Point?crs=EPSG:5187"), name, QStringLiteral("memory"));
  project.addMapLayer(layer, false);
  group->addLayer(layer);
  return layer;
}

QStringList names(QgsLayerTreeGroup* group) {
  QStringList out;
  for (QgsLayerTreeNode* child : group->children()) out << child->name();
  return out;
}
}  // namespace

class LayerTreeUndoTest : public QObject {
  Q_OBJECT
private slots:
  void restorePutsOrderAndChecksBack() {
    QgsProject project;
    auto* root = project.layerTreeRoot();
    auto* a = addLayer(project, root, QStringLiteral("A"));
    auto* refs = root->addGroup(QStringLiteral("참조 지도"));
    auto* b = addLayer(project, refs, QStringLiteral("B"));
    addLayer(project, refs, QStringLiteral("C"));
    const auto before = KaLayerTreeUndo::capture(root);
    const int layers = project.mapLayers().size();
    // Drag A into the group, reorder, switch B off.
    QVERIFY(LayerOps::moveLegendLayer(refs->findLayer(b->id()), 1));
    refs->insertChildNode(0, root->findLayer(a->id())->clone());
    root->removeChildNode(root->findLayer(a->id()));
    refs->findLayer(b->id())->setItemVisibilityChecked(false);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(project.mapLayers().size(), layers);
    QVERIFY(*KaLayerTreeUndo::capture(root) != *before);
    QString error;
    QVERIFY2(KaLayerTreeUndo::restoreInto(root, *before, &error), qPrintable(error));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(names(root), (QStringList{QStringLiteral("A"), QStringLiteral("참조 지도")}));
    QCOMPARE(names(refs), (QStringList{QStringLiteral("B"), QStringLiteral("C")}));
    QVERIFY(root->findLayer(b->id())->itemVisibilityChecked());
    QCOMPARE(project.mapLayers().size(), layers);  // no layer lost on the way
    QVERIFY(*KaLayerTreeUndo::capture(root) == *before);
  }

  void onlyTheUsersChangesBecomeUndoSteps() {
    QgsProject project;
    auto* root = project.layerTreeRoot();
    auto* a = addLayer(project, root, QStringLiteral("A"));
    addLayer(project, root, QStringLiteral("B"));
    QList<std::shared_ptr<KaLayerTreeSnapshot>> pushed;
    KaLayerTreeUndo undo(&project, nullptr, [&](std::shared_ptr<KaLayerTreeSnapshot> before) { pushed << before; });
    // Programmatic move (e.g. satellite kept at the bottom): folded into the baseline.
    QVERIFY(LayerOps::moveLegendLayer(root->findLayer(a->id()), 1));
    QTest::qWait(10);
    QVERIFY(pushed.isEmpty());
    // The user's click: one undo step holding the state before it.
    undo.noteUserGesture();
    root->findLayer(a->id())->setItemVisibilityChecked(false);
    QTRY_COMPARE(pushed.size(), 1);
    QVERIFY(pushed.first()->checked.value(QStringLiteral("L:") + a->id()));
    // Adding a layer has its own undo: not recorded here even right after a click.
    undo.noteUserGesture();
    addLayer(project, root, QStringLiteral("C"));
    QTest::qWait(10);
    QCOMPARE(pushed.size(), 1);
    // An empty group removed with the Delete key keeps the same layer set; it is
    // still a removed row (its LayersRemoved step brings it back), not a reorder.
    auto* empty = root->insertGroup(0, QStringLiteral("빈 묶음"));
    QTest::qWait(10);
    undo.noteUserGesture();
    root->removeChildNode(empty);
    QTest::qWait(10);
    QCOMPARE(pushed.size(), 1);
    QVERIFY(undo.restore(*pushed.first()));
    QVERIFY(root->findLayer(a->id())->itemVisibilityChecked());
    QTest::qWait(10);
    QCOMPARE(pushed.size(), 1);  // the undo itself is not a new step
  }

  void layerChangesMergeIntoOneRefresh() {
    QObject owner;
    int runs = 0;
    KaLayerRefreshBatch batch(&owner, [&runs] { ++runs; });
    for (int i = 0; i < 25; ++i) batch.request();
    QVERIFY(batch.pending());
    QTRY_COMPARE(runs, 1);
    QCOMPARE(batch.lastBatchSize(), 25);
    QVERIFY(!batch.pending());
    batch.request();
    QTRY_COMPARE(runs, 2);
    QCOMPARE(batch.lastBatchSize(), 1);
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, true);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  LayerTreeUndoTest test;
  const int rc = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
#include "test_layer_tree_undo.moc"
