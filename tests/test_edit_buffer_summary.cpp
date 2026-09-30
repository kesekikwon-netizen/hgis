// P5 shell-chrome core: EditBufferSummary counts unique edited feature ids over the
// project's edit buffers for the status bar's 「저장 안 됨 n건」. Memory layers only.
#include <QtTest>

#include <QElapsedTimer>
#include <QVariant>

#include <qgsapplication.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgspointxy.h>
#include <qgsproject.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorlayer.h>

#include "core/EditBufferSummary.h"

namespace {
// Owned by the project that adds it (takeOwnership).
QgsVectorLayer* pointLayer(int count, const QString& name = QStringLiteral("유구")) {
  auto* layer = new QgsVectorLayer(QStringLiteral("Point?crs=EPSG:5186&field=note:string"), name, QStringLiteral("memory"));
  QgsFeatureList list;
  list.reserve(count);
  for (int i = 0; i < count; ++i) {
    QgsFeature f(layer->fields());
    f.setAttribute(0, QString::number(i));
    f.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(200000 + i, 450000)));
    list.append(f);
  }
  layer->dataProvider()->addFeatures(list);
  layer->updateExtents();
  return layer;
}

QList<QgsFeatureId> idsOf(QgsVectorLayer* layer) {
  QList<QgsFeatureId> ids;
  QgsFeature f;
  QgsFeatureIterator it = layer->getFeatures();
  while (it.nextFeature(f)) ids.append(f.id());
  return ids;
}
}  // namespace

class TestEditBufferSummary : public QObject {
  Q_OBJECT
 private slots:
  void countsUniqueFeatureIds() {
    QgsProject project;
    QgsVectorLayer* layer = pointLayer(3);
    project.addMapLayer(layer);
    project.setDirty(false);
    QVERIFY(layer->startEditing());
    const QgsFeatureId id = idsOf(layer).first();
    QVERIFY(layer->changeAttributeValue(id, 0, QStringLiteral("바뀜")));
    QgsGeometry moved = QgsGeometry::fromPointXY(QgsPointXY(1, 1));
    QVERIFY(layer->changeGeometry(id, moved));
    QCOMPARE(EditBufferSummary::uniqueEditedIds(layer), 1);  // attribute + geometry on one feature
    const EditBufferSummary::Summary summary = EditBufferSummary::summarize(&project);
    QCOMPARE(summary.features, 1);
    QCOMPARE(summary.layers, 1);
    QVERIFY(summary.unsaved());
    QVERIFY(layer->rollBack());
    QCOMPARE(EditBufferSummary::summarize(&project).features, 0);
  }

  void addedChangedDeleted() {
    QgsProject project;
    QgsVectorLayer* layer = pointLayer(3);
    project.addMapLayer(layer);
    QVERIFY(layer->startEditing());
    const QList<QgsFeatureId> ids = idsOf(layer);
    QgsFeature added(layer->fields());
    added.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(5, 5)));
    QVERIFY(layer->addFeature(added));
    QCOMPARE(EditBufferSummary::uniqueEditedIds(layer), 1);
    QVERIFY(layer->changeAttributeValue(ids.at(0), 0, QStringLiteral("a")));
    QCOMPARE(EditBufferSummary::uniqueEditedIds(layer), 2);
    QVERIFY(layer->deleteFeature(ids.at(1)));
    QCOMPARE(EditBufferSummary::uniqueEditedIds(layer), 3);
    QgsGeometry moved = QgsGeometry::fromPointXY(QgsPointXY(2, 2));
    QVERIFY(layer->changeGeometry(ids.at(0), moved));
    QCOMPARE(EditBufferSummary::uniqueEditedIds(layer), 3);  // same feature again
    QCOMPARE(EditBufferSummary::summarize(&project).features, 3);
    QVERIFY(layer->rollBack());
  }

  void ignoresLayersWithoutEditBuffer() {
    QgsProject project;
    QgsVectorLayer* quiet = pointLayer(2, QStringLiteral("조용"));
    QgsVectorLayer* busy = pointLayer(2, QStringLiteral("편집"));
    project.addMapLayer(quiet);
    project.addMapLayer(busy);
    project.setDirty(false);
    QCOMPARE(EditBufferSummary::uniqueEditedIds(quiet), 0);
    QCOMPARE(EditBufferSummary::uniqueEditedIds(nullptr), 0);
    QVERIFY(busy->startEditing());
    QCOMPARE(EditBufferSummary::summarize(&project).layers, 0);  // editing but nothing changed yet
    QVERIFY(busy->changeAttributeValue(idsOf(busy).first(), 0, QStringLiteral("x")));
    const EditBufferSummary::Summary summary = EditBufferSummary::summarize(&project);
    QCOMPARE(summary.features, 1);
    QCOMPARE(summary.layers, 1);
    QVERIFY(busy->rollBack());
    const EditBufferSummary::Summary none = EditBufferSummary::summarize(nullptr);
    QCOMPARE(none.features, 0);
    QVERIFY(!none.projectDirty && !none.unsaved());
  }

  void projectDirtyFlagPassesThrough() {
    QgsProject project;
    project.setDirty(true);
    EditBufferSummary::Summary summary = EditBufferSummary::summarize(&project);
    QVERIFY(summary.projectDirty && summary.unsaved());
    QCOMPARE(summary.features, 0);
    project.setDirty(false);
    summary = EditBufferSummary::summarize(&project);
    QVERIFY(!summary.projectDirty && !summary.unsaved());
  }

  void tenThousandEditsUnderTenMs() {
    QgsProject project;
    QgsVectorLayer* layer = pointLayer(200);
    project.addMapLayer(layer);
    QVERIFY(layer->startEditing());
    const QList<QgsFeatureId> ids = idsOf(layer);
    QCOMPARE(ids.size(), 200);
    // 10 200 buffered edits on 10 000 features: an attribute and a geometry change on each
    // of the 200 provider features (each costs QGIS an old-value fetch, which is not what is
    // measured, so they go first while the buffer is small), then 9 800 added in one call.
    for (const QgsFeatureId id : ids) {
      QVERIFY(layer->changeAttributeValue(id, 0, QStringLiteral("n")));
      QgsGeometry moved = QgsGeometry::fromPointXY(QgsPointXY(1, static_cast<double>(id)));
      QVERIFY(layer->changeGeometry(id, moved));
    }
    QgsFeatureList fresh;
    fresh.reserve(9800);
    for (int i = 0; i < 9800; ++i) {
      QgsFeature f(layer->fields());
      f.setAttribute(0, QStringLiteral("new"));
      f.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(300000 + i, 450000)));
      fresh.append(f);
    }
    QVERIFY(layer->addFeatures(fresh));
    QCOMPARE(EditBufferSummary::uniqueEditedIds(layer), 10000);
    qint64 best = -1;
    for (int run = 0; run < 20; ++run) {
      QElapsedTimer timer;
      timer.start();
      const EditBufferSummary::Summary summary = EditBufferSummary::summarize(&project);
      const qint64 ns = timer.nsecsElapsed();
      QCOMPARE(summary.features, 10000);
      if (best < 0 || ns < best) best = ns;
    }
    qInfo().noquote() << "summarize(10000 features, 10200 edits) best of 20 runs:" << best / 1e6 << "ms";
    QVERIFY2(best < 10'000'000, qPrintable(QStringLiteral("best of 20 runs: %1 ms").arg(best / 1e6, 0, 'f', 2)));
    QVERIFY(layer->rollBack());
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, true);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH", "A:/OSGeo4W/apps/qgis-dev"), true);
  QgsApplication::initQgis();
  TestEditBufferSummary test;
  const int result = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis();
  return result;
}

#include "test_edit_buffer_summary.moc"
