// Layer role, transparency, fixed-order notes and missing-source retry spacing
// (evaluation F020, F018, F184, F136).
#include <QtTest>
#include <QTemporaryDir>
#include <qgsapplication.h>
#include <qgslayertree.h>
#include <qgsproject.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectorlayer.h>
#include "core/LayerOps.h"
#include "core/LayerRole.h"
#include "core/LayerTreePolicy.h"
#include "core/LayerTreeRecovery.h"

namespace {
QgsVectorLayer* memory(const QString& name, const QString& geometry = QStringLiteral("Polygon")) {
  return new QgsVectorLayer(geometry + QStringLiteral("?crs=EPSG:5187&field=name:string"), name,
                            QStringLiteral("memory"));
}
}  // namespace

class LayerRolesTest : public QObject {
  Q_OBJECT
private slots:
  void importedSurveyDataIsNeverReferenceByTitle() {
    // An imported SHP keeps its "user:" key and survey role; titles are labels only.
    std::unique_ptr<QgsVectorLayer> imported(memory(QStringLiteral("OSM 지적경계_수정")));
    LayerOps::markSurveyLayer(imported.get(), QStringLiteral("user:지적경계_수정"));
    QVERIFY(!LayerOps::isReferenceLayer(imported.get()));
    QVERIFY(!LayerOps::isCadastralLayer(imported.get()));
    QVERIFY(!LayerOps::isReferenceOrBasemapLayer(imported.get()));
    QVERIFY(LayerOps::isSnapSourceLayer(imported.get()));
    QCOMPARE(LayerRole::resolve(imported.get()), LayerRole::Kind::Survey);
    // A key alone (older save without a role) is enough.
    std::unique_ptr<QgsVectorLayer> keyed(memory(QStringLiteral("지형단면_판독")));
    keyed->setCustomProperty(QStringLiteral("ka_hgis/layer_key"), QStringLiteral("user:지형단면"));
    QVERIFY(!LayerOps::isReferenceLayer(keyed.get()));
    QVERIFY(!LayerOps::isReferenceOrBasemapLayer(keyed.get()));
  }

  void storedRoleSurvivesRename() {
    std::unique_ptr<QgsVectorLayer> soil(memory(QStringLiteral("토양도")));
    LayerOps::markReferenceLayer(soil.get());
    soil->setName(QStringLiteral("내 자료"));
    QVERIFY(LayerOps::isReferenceLayer(soil.get()));
    QVERIFY(LayerOps::isReferenceOrBasemapLayer(soil.get()));
    std::unique_ptr<QgsVectorLayer> cad(memory(QStringLiteral("지적도 · 조사 주변 5km")));
    LayerOps::markCadastralLayer(cad.get());
    cad->setName(QStringLiteral("받은 경계"));
    QVERIFY(LayerOps::isCadastralLayer(cad.get()));
    QVERIFY(!LayerOps::isReferenceLayer(cad.get()));
  }

  void legacyTitlesStayAMigrationFallback() {
    std::unique_ptr<QgsVectorLayer> osm(memory(QStringLiteral("OSM")));
    QCOMPARE(LayerRole::stored(osm.get()), LayerRole::Kind::Unknown);
    QVERIFY(LayerOps::isReferenceLayer(osm.get()));
    QgsProject project;
    auto* legacy = memory(QStringLiteral("VWorld 배경"));
    auto* plain = memory(QStringLiteral("가져온 면"));
    project.addMapLayer(legacy);
    project.addMapLayer(plain);
    project.setDirty(false);
    QCOMPARE(LayerRole::persistLegacyRoles(&project), 1);
    QVERIFY2(!project.isDirty(), "opening a survey must not mark it as changed");
    QCOMPARE(LayerRole::stored(legacy), LayerRole::Kind::Reference);
    legacy->setName(QStringLiteral("이름 바꿈"));
    QVERIFY(LayerOps::isReferenceLayer(legacy));
    QCOMPARE(LayerRole::stored(plain), LayerRole::Kind::Unknown);
    QCOMPARE(LayerRole::persistLegacyRoles(&project), 0);
  }

  void surveyPolygonsCanBeMadeSeeThrough() {
    std::unique_ptr<QgsVectorLayer> area(memory(QStringLiteral("유구면")));
    LayerOps::markSurveyLayer(area.get(), QStringLiteral("feature_poly"));
    QVERIFY(!LayerOps::isReferenceOrBasemapLayer(area.get()));
    QVERIFY(LayerOps::canAdjustOpacity(area.get()));
    QVERIFY(LayerOps::opacityUnavailableReason(area.get()).isEmpty());
    QSignalSpy repaint(area.get(), &QgsMapLayer::repaintRequested);
    QVERIFY(LayerOps::applyLayerOpacity(area.get(), 0.6));
    QCOMPARE(area->opacity(), 0.6);
    QVERIFY(repaint.count() > 0);
    QVERIFY(LayerOps::applyLayerOpacity(area.get(), 3.0));
    QCOMPARE(area->opacity(), 1.0);
    std::unique_ptr<QgsVectorLayer> broken(
        new QgsVectorLayer(QStringLiteral("A:/없는/자료.shp"), QStringLiteral("끊긴 자료"), QStringLiteral("ogr")));
    QVERIFY(!broken->isValid());
    QVERIFY(!LayerOps::canAdjustOpacity(broken.get()));
    QVERIFY(!LayerOps::opacityUnavailableReason(broken.get()).isEmpty());
    QVERIFY(!LayerOps::applyLayerOpacity(broken.get(), 0.5));
    QVERIFY(!LayerOps::canAdjustOpacity(nullptr));
  }

  void fixedDrawingOrderIsExplained() {
    QgsProject project;
    auto* satellite = memory(QStringLiteral("위성"));
    auto* cad = memory(QStringLiteral("지적도"));
    LayerOps::markCadastralLayer(cad);
    auto* plain = memory(QStringLiteral("참고 면"));
    LayerOps::markReferenceLayer(plain);
    project.addMapLayers({satellite, cad, plain});
    QVERIFY(LayerTreePolicy::forcedOrderNote(satellite, &project).contains(QStringLiteral("맨 아래")));
    QVERIFY(LayerTreePolicy::forcedOrderNote(cad, &project).contains(QStringLiteral("지적")));
    QVERIFY(LayerTreePolicy::forcedOrderNote(plain, &project).isEmpty());
    QVERIFY(LayerTreePolicy::forcedOrderNote(nullptr, &project).isEmpty());
  }

  void flatListStaysFlat() {
    // PO goal ORIG-3: the list is flat; the role, not a heading, separates data.
    QgsProject project;
    auto* soil = memory(QStringLiteral("토양도"));
    LayerOps::markReferenceLayer(soil);
    project.addMapLayer(soil);
    LayerOps::placeInLegendGroup(&project, soil, QString::fromUtf8(LayerOps::kGroupReference));
    auto* node = project.layerTreeRoot()->findLayer(soil->id());
    QVERIFY(node);
    QCOMPARE(node->parent(), project.layerTreeRoot());
    QVERIFY(node->itemVisibilityChecked());
    QVERIFY(!project.layerTreeRoot()->findGroup(QString::fromUtf8(LayerOps::kGroupReference)));
    QVERIFY(!project.layerTreeRoot()->findGroup(QString::fromUtf8(LayerOps::kGroupSurveyData)));
  }

  void missingSourcesAreRetriedWithGrowingSpacing() {
    QCOMPARE(LayerTreeRecovery::backoffSeconds(0), 0);
    QCOMPARE(LayerTreeRecovery::backoffSeconds(1), 15);
    QCOMPARE(LayerTreeRecovery::backoffSeconds(2), 30);
    QCOMPARE(LayerTreeRecovery::backoffSeconds(3), 60);
    QCOMPARE(LayerTreeRecovery::backoffSeconds(10), 300);
    qint64 clock = 1000;
    LayerTreeRecovery::resetForTests();
    LayerTreeRecovery::setClockForTests([&clock] { return clock; });
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("gone.gpkg"));
    QgsVectorLayer source(QStringLiteral("Polygon?crs=EPSG:5187"), QStringLiteral("src"), QStringLiteral("memory"));
    QgsVectorFileWriter::SaveVectorOptions options;
    options.driverName = QStringLiteral("GPKG");
    QCOMPARE(QgsVectorFileWriter::writeAsVectorFormatV3(&source, path, QgsCoordinateTransformContext(), options),
             QgsVectorFileWriter::NoError);
    QgsProject project;
    auto* layer = new QgsVectorLayer(path, QStringLiteral("USB 자료"), QStringLiteral("ogr"));
    QVERIFY(layer->isValid());
    project.addMapLayer(layer);
    layer->setDataSource(dir.filePath(QStringLiteral("missing.gpkg")), layer->name(), QStringLiteral("ogr"));
    QVERIFY(!layer->isValid());
    QStringList broken;
    QCOMPARE(LayerOps::reviveInvalidLayers(&project, nullptr, &broken), 0);
    QCOMPARE(broken, QStringList{QStringLiteral("USB 자료")});
    QVERIFY2(!LayerTreeRecovery::shouldProbe(layer), "a confirmed-missing file waits before the next probe");
    broken.clear();
    QCOMPARE(LayerOps::reviveInvalidLayers(&project, nullptr, &broken), 0);
    QCOMPARE(broken, QStringList{QStringLiteral("USB 자료")});  // still listed for the 30 s log
    clock += 15 * 1000;
    QVERIFY(LayerTreeRecovery::shouldProbe(layer));
    layer->setDataSource(path, layer->name(), QStringLiteral("ogr"));  // drive is back
    QVERIFY(layer->isValid());
    LayerOps::reviveInvalidLayers(&project);
    QVERIFY(LayerTreeRecovery::shouldProbe(layer));
    LayerTreeRecovery::setClockForTests({});
    LayerTreeRecovery::resetForTests();
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  LayerRolesTest test;
  const int rc = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
#include "test_layer_roles.moc"
