// Polygon merge, shared-vertex move and the label stack query/apply split
// (evaluation F083, F220, F218).
#include <QtTest>
#include <QUndoStack>
#include <qgsapplication.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgslayertree.h>
#include <qgslinesymbol.h>
#include <qgspallabeling.h>
#include <qgsproject.h>
#include <qgssinglesymbolrenderer.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayerlabeling.h>
#include "core/GeometryEditOps.h"
#include "core/LayerOps.h"

namespace {
QgsFeatureId addRect(QgsVectorLayer* layer, const QgsRectangle& rect, const QString& kind, const QString& number) {
  QgsFeature feature(layer->fields());
  feature.setAttribute(QStringLiteral("kind"), kind);
  feature.setAttribute(QStringLiteral("feature_no"), number);
  feature.setGeometry(QgsGeometry::fromRect(rect));
  QgsFeatureList features{feature};
  if (!layer->dataProvider()->addFeatures(features)) return FID_NULL;
  return features.first().id();
}

std::unique_ptr<QgsVectorLayer> featurePolygons() {
  auto layer = std::make_unique<QgsVectorLayer>(
      QStringLiteral("Polygon?crs=EPSG:5187&field=kind:string&field=feature_no:string"),
      QStringLiteral("유구면"), QStringLiteral("memory"));
  LayerOps::markSurveyLayer(layer.get(), QStringLiteral("feature_poly"));
  return layer;
}
}  // namespace

class LayerEditsTest : public QObject {
  Q_OBJECT
private slots:
  void mergeNeverTakesTheWholeLayer() {
    auto layer = featurePolygons();
    const QgsFeatureId a = addRect(layer.get(), QgsRectangle(0, 0, 10, 10), QStringLiteral("주거지"), QStringLiteral("1"));
    addRect(layer.get(), QgsRectangle(20, 0, 30, 10), QStringLiteral("수혈"), QStringLiteral("2"));
    QString error;
    QVERIFY(!LayerOps::mergePolygonFeatures(layer.get(), QgsFeatureIds(), &error));
    QVERIFY(error.contains(QStringLiteral("2개 이상")));
    QVERIFY(!LayerOps::mergePolygonFeatures(layer.get(), QgsFeatureIds{a}, &error));
    QCOMPARE(layer->featureCount(), 2);
    QVERIFY(!layer->isEditable());
  }

  void mergeWarnsAboutLostAttributesAndCanBeUndone() {
    auto layer = featurePolygons();
    const QgsFeatureId a = addRect(layer.get(), QgsRectangle(0, 0, 10, 10), QStringLiteral("주거지"), QStringLiteral("1"));
    const QgsFeatureId b = addRect(layer.get(), QgsRectangle(10, 0, 20, 10), QStringLiteral("수혈"), QStringLiteral("2"));
    const QgsFeatureId c = addRect(layer.get(), QgsRectangle(40, 0, 50, 10), QStringLiteral("주거지"), QString());
    QCOMPARE(GeometryEditOps::mergeKeeperId({b, a}), a);
    // Unsaved shapes get -1, -2, … as they are drawn; saved ones were drawn before them.
    QCOMPARE(GeometryEditOps::mergeKeeperId({-3, -1, 5}), QgsFeatureId(5));
    QCOMPARE(GeometryEditOps::mergeKeeperId({-3, -1}), QgsFeatureId(-1));
    QVERIFY(GeometryEditOps::mergeConflicts(layer.get(), {a, c}).isEmpty());  // nothing lost
    const auto conflicts = GeometryEditOps::mergeConflicts(layer.get(), {a, b});
    QCOMPARE(conflicts.size(), 2);
    QCOMPARE(conflicts.at(0).field, QStringLiteral("kind"));
    QCOMPARE(conflicts.at(0).values, (QStringList{QStringLiteral("주거지"), QStringLiteral("수혈")}));
    const QString summary = GeometryEditOps::mergeConflictSummary(conflicts);
    QVERIFY(summary.contains(QStringLiteral("먼저 그린")) && summary.contains(QStringLiteral("Ctrl+Z")));

    QString error;
    QVERIFY2(LayerOps::mergePolygonFeatures(layer.get(), {a, b}, &error), qPrintable(error));
    QVERIFY2(layer->isEditable() && layer->isModified(), "the merge stays in the edit buffer (saved with the survey)");
    QCOMPARE(layer->featureCount(), 2);
    QgsFeature merged;
    QgsFeatureIterator it = layer->getFeatures();
    bool keptFirst = false;
    while (it.nextFeature(merged)) {
      if (merged.id() != c) {
        // QStringLiteral, not QLatin1String: the source is UTF-8, so a Latin-1 view of
        // a Korean literal never equals the stored QString.
        keptFirst = merged.attribute(QStringLiteral("kind")).toString() == QStringLiteral("주거지") &&
                    merged.attribute(QStringLiteral("feature_no")).toString() == QLatin1String("1");
        QVERIFY(qAbs(merged.geometry().area() - 200.) < 1e-6);
      }
    }
    QVERIFY2(keptFirst, "the polygon drawn first keeps its record");
    QVERIFY(layer->undoStack()->canUndo());
    layer->undoStack()->undo();
    QCOMPARE(layer->featureCount(), 3);
    QVERIFY(layer->getFeature(b).isValid());
    QVERIFY(layer->rollBack());
  }

  // 떨어진 두 구역은 하나로 묶지 않는다: 한 조각 표(조사구역·유구면)에 여러 조각 도형을 넣으면 저장 때 면적 0인
  // 고장 난 도형이 된다(2026-10-02 조사 R25). 두 구역은 그대로 남아 따로 저장·제출된다.
  void mergeRefusesSeparatePolygonsInASinglePartLayer() {
    auto layer = featurePolygons();
    const QgsFeatureId a = addRect(layer.get(), QgsRectangle(0, 0, 10, 10), QStringLiteral("구역"), QStringLiteral("1"));
    const QgsFeatureId b = addRect(layer.get(), QgsRectangle(20, 0, 30, 10), QStringLiteral("구역"), QStringLiteral("2"));
    QString error;
    QVERIFY(!LayerOps::mergePolygonFeatures(layer.get(), {a, b}, &error));
    QVERIFY2(error.contains(QStringLiteral("떨어진 구역")), qUtf8Printable(error));
    QCOMPARE(layer->featureCount(), 2);
    QVERIFY(!layer->isModified());
  }

  void sharedVertexMoveTouchesOnlyNeighbours() {
    auto layer = featurePolygons();
    const QgsFeatureId left = addRect(layer.get(), QgsRectangle(0, 0, 10, 10), QStringLiteral("a"), QStringLiteral("1"));
    const QgsFeatureId right = addRect(layer.get(), QgsRectangle(10, 0, 20, 10), QStringLiteral("b"), QStringLiteral("2"));
    const QgsFeatureId far = addRect(layer.get(), QgsRectangle(500, 500, 510, 510), QStringLiteral("c"), QStringLiteral("3"));
    QVERIFY(layer->startEditing());
    // Vertex 2 of the left rectangle is (10, 10), shared with the right one.
    const QgsPoint shared = layer->getFeature(left).geometry().vertexAt(2);
    QCOMPARE(QgsPointXY(shared.x(), shared.y()), QgsPointXY(10, 10));
    QString error;
    QVERIFY2(LayerOps::applyVertexMove(layer.get(), left, 2, 11, 12, true, &error), qPrintable(error));
    const auto hasVertex = [&](QgsFeatureId id, const QgsPointXY& p) {
      const QgsGeometry g = layer->getFeature(id).geometry();
      for (int i = 0; i < int(g.constGet()->nCoordinates()); ++i)
        if (QgsPointXY(g.vertexAt(i).x(), g.vertexAt(i).y()) == p) return true;
      return false;
    };
    QVERIFY(hasVertex(left, QgsPointXY(11, 12)));
    QVERIFY2(hasVertex(right, QgsPointXY(11, 12)), "the neighbour inside the 1 mm box moves along");
    QVERIFY(hasVertex(far, QgsPointXY(500, 500)));
    QVERIFY(layer->rollBack());
  }

  void labelStackQueryDoesNotWrite() {
    QgsProject project;
    auto* cad = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186&field=jibun:string"),
                                   QStringLiteral("지번"), QStringLiteral("memory"));
    QVERIFY(LayerOps::applyNameAttributeLabels(cad, QStringLiteral("jibun"), 8.0, false));
    auto* line = new QgsVectorLayer(QStringLiteral("LineString?crs=EPSG:5186"), QStringLiteral("조사선"),
                                    QStringLiteral("memory"));
    line->setRenderer(new QgsSingleSymbolRenderer(QgsLineSymbol::createSimple(
        {{QStringLiteral("color"), QStringLiteral("255,0,0,255")}}).release()));
    project.addMapLayers({cad, line});
    auto* root = project.layerTreeRoot();
    root->removeAllChildren();
    root->addLayer(line);
    root->addLayer(cad);
    QVERIFY(LayerOps::layersDrawnAboveLabels(&project).contains(line));
    QVERIFY2(!line->customProperty(QStringLiteral("rendering/renderAboveLabels")).isValid(),
             "a query must not change layers");
    LayerOps::applyLayerOrderToLabels(&project, nullptr);
    QVERIFY(line->customProperty(QStringLiteral("rendering/renderAboveLabels")).toBool());
    QCOMPARE(cad->labeling()->settings().zIndex, 1.0);
    // Unchanged state: another apply touches nothing (no repaint, no label rewrite).
    // This is the per-refresh path; verifying every label setting here would copy
    // QgsPalLayerSettings per layer on each map refresh.
    QSignalSpy repaints(cad, &QgsMapLayer::repaintRequested);
    LayerOps::applyLayerOrderToLabels(&project, nullptr);
    QCOMPARE(repaints.count(), 0);
    // setLabeling() emits nothing; every app path repaints right after it, and that
    // repaint is what invalidates the stack so the next apply restores the order.
    QgsPalLayerSettings fresh = cad->labeling()->settings();
    fresh.zIndex = 0;
    cad->setLabeling(new QgsVectorLayerSimpleLabeling(fresh));
    cad->triggerRepaint();
    LayerOps::applyLayerOrderToLabels(&project, nullptr);
    QCOMPARE(cad->labeling()->settings().zIndex, 1.0);
    // Hiding the labelled layer changes the answer without any explicit apply.
    root->findLayer(cad->id())->setItemVisibilityChecked(false);
    QVERIFY(!LayerOps::layersDrawnAboveLabels(&project).contains(line));
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  LayerEditsTest test;
  const int rc = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
#include "test_layer_edits.moc"
