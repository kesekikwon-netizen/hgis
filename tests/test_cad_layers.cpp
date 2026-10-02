// 도면 변환본(GPKG)을 참조 지도 아래 「<원본 이름> (도면)」 묶음으로 올리는 CadDrawingLayers.
// 변환본은 CadDrawingStore::write 로 만든다. 레이어 이름이 「entities」로 나오던 옛 동작을 막는다.
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <cmath>

#include "core/CadDrawingLayers.h"
#include "core/CadDrawingStore.h"
#include "core/GeorefService.h"
#include "core/HeritageImport.h"
#include "core/LayerRole.h"

#include <qgsapplication.h>
#include <qgslayertree.h>
#include <qgslinesymbollayer.h>
#include <qgspallabeling.h>
#include <qgsproject.h>
#include <qgsproperty.h>
#include <qgssinglesymbolrenderer.h>
#include <qgssymbol.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayerlabeling.h>

namespace {

const QString kId = QStringLiteral("1b2c3d4e-0000-4000-8000-000000000001");
const QString kOtherId = QStringLiteral("1b2c3d4e-0000-4000-8000-000000000002");

CadEntity line() {
  CadEntity e;
  e.kind = CadKind::Line;
  e.geometry = QgsGeometry::fromPolylineXY({QgsPointXY(0, 0), QgsPointXY(10, 0)});
  e.cadLayer = QStringLiteral("JIJUK");
  e.color = QColor(QStringLiteral("#7f0000"));
  return e;
}

CadEntity text() {
  CadEntity e;
  e.kind = CadKind::Text;
  e.geometry = QgsGeometry::fromPointXY(QgsPointXY(5, 5));
  e.cadLayer = QStringLiteral("JIBUN");
  e.color = QColor(QStringLiteral("#3c3c3c"));
  e.text = QStringLiteral("374-1전");
  e.textHeight = 2.5;
  e.textAngle = 30;
  e.textAnchor = 1;
  return e;
}

QString writeGpkg(const QTemporaryDir& dir, const QString& name, const QVector<CadEntity>& entities) {
  CadDrawing drawing;
  drawing.entities = entities;
  const QString path = dir.filePath(name + QStringLiteral(".gpkg"));
  QString error;
  const bool ok = CadDrawingStore::write(drawing, CadStoreInfo{QStringLiteral("C:/x/") + name + QStringLiteral(".dxf"),
                                                               QStringLiteral("ab12"), QString(), QString()},
                                         QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")),
                                         QgsProject::instance()->transformContext(), path, &error);
  return ok ? path : QString();
}

QgsLayerTreeGroup* titleGroup(QgsProject& project, const QString& title) {
  QgsLayerTreeGroup* reference = project.layerTreeRoot()->findGroup(HeritageImport::referenceGroupName());
  return reference ? reference->findGroup(title) : nullptr;
}

int titleGroupCount(QgsProject& project, const QString& title) {
  QgsLayerTreeGroup* reference = project.layerTreeRoot()->findGroup(HeritageImport::referenceGroupName());
  int count = 0;
  if (reference)
    for (QgsLayerTreeNode* node : reference->children()) count += QgsLayerTree::isGroup(node) && node->name() == title;
  return count;
}

QStringList childNames(QgsLayerTreeGroup* group) {
  QStringList names;
  for (QgsLayerTreeNode* node : group->children()) names << node->name();
  return names;
}

QgsVectorLayer* named(const QList<QgsVectorLayer*>& layers, const QString& name) {
  for (QgsVectorLayer* layer : layers)
    if (layer->name() == name) return layer;
  return nullptr;
}

QString drawingProperty() { return QString::fromLatin1(CadDrawingLayers::kPropDrawing); }

}  // namespace

class TestCadLayers : public QObject {
  Q_OBJECT
 private slots:
  void groupTitle_isTheFileNamePlusDrawing() {
    QCOMPARE(CadDrawingLayers::groupTitle(QStringLiteral("C:/a/[동국] 도면(2010v).dwg")),
             QStringLiteral("[동국] 도면(2010v) (도면)"));
  }

  void addToProject_buildsTheGroupUnderReference() {
    QTemporaryDir tmp;
    const QString gpkg = writeGpkg(tmp, QStringLiteral("가수리"), {line(), text()});
    QVERIFY(!gpkg.isEmpty());
    QgsProject project;
    QString error;
    const QList<QgsVectorLayer*> layers =
        CadDrawingLayers::addToProject(&project, gpkg, QStringLiteral("가수리 (도면)"), kId, &error);
    QVERIFY2(layers.size() == 2, qUtf8Printable(error));
    QgsLayerTreeGroup* group = titleGroup(project, QStringLiteral("가수리 (도면)"));
    QVERIFY(group);
    QCOMPARE(childNames(group), QStringList({"글자", "선"}));
    for (QgsVectorLayer* layer : layers) {
      QCOMPARE(LayerRole::stored(layer), LayerRole::Kind::Reference);
      QVERIFY(layer->customProperty(QStringLiteral("ka_hgis/imported_reference")).toBool());
      QCOMPARE(layer->customProperty(drawingProperty()).toString(), kId);
    }
    QVERIFY(project.mapLayersByName(QStringLiteral("entities")).isEmpty());
  }

  void addToProject_stylesFollowTheSpec() {
    QTemporaryDir tmp;
    const QString gpkg = writeGpkg(tmp, QStringLiteral("가수리"), {line(), text()});
    QgsProject project;
    QString error;
    const QList<QgsVectorLayer*> layers =
        CadDrawingLayers::addToProject(&project, gpkg, QStringLiteral("가수리 (도면)"), kId, &error);
    QgsVectorLayer* lines = named(layers, QStringLiteral("선"));
    QVERIFY(lines);
    auto* renderer = dynamic_cast<QgsSingleSymbolRenderer*>(lines->renderer());
    QVERIFY(renderer && renderer->symbol());
    auto* stroke = dynamic_cast<QgsSimpleLineSymbolLayer*>(renderer->symbol()->symbolLayer(0));
    QVERIFY(stroke);
    QCOMPARE(stroke->width(), 0.26);
    QCOMPARE(stroke->widthUnit(), Qgis::RenderUnit::Millimeters);
    QCOMPARE(stroke->dataDefinedProperties().property(QgsSymbolLayer::Property::StrokeColor).asExpression(),
             QStringLiteral("\"color\""));

    QgsVectorLayer* texts = named(layers, QStringLiteral("글자"));
    QVERIFY(texts);
    QCOMPARE(texts->renderer()->type(), QStringLiteral("nullSymbol"));
    QVERIFY(texts->labelsEnabled());
    auto* labeling = dynamic_cast<const QgsVectorLayerSimpleLabeling*>(texts->labeling());
    QVERIFY(labeling);
    const QgsPalLayerSettings settings = labeling->settings();
    QCOMPARE(settings.fieldName, QStringLiteral("text"));
    QCOMPARE(settings.format().sizeUnit(), Qgis::RenderUnit::MapUnits);
    const QgsPropertyCollection& dd = settings.dataDefinedProperties();
    QCOMPARE(dd.property(QgsPalLayerSettings::Property::Size).asExpression(), QStringLiteral("\"text_height\""));
    QCOMPARE(dd.property(QgsPalLayerSettings::Property::LabelRotation).asExpression(),
             QStringLiteral("-\"text_angle\""));
    QCOMPARE(dd.property(QgsPalLayerSettings::Property::Color).asExpression(), QStringLiteral("\"color\""));
  }

  void addToProject_replacesTheSameTitle() {
    QTemporaryDir tmp;
    const QString first = writeGpkg(tmp, QStringLiteral("첫째"), {line(), text()});
    const QString second = writeGpkg(tmp, QStringLiteral("둘째"), {line(), text()});
    QgsProject project;
    QString error;
    QVERIFY(!CadDrawingLayers::addToProject(&project, first, QStringLiteral("가수리 (도면)"), kId, &error).isEmpty());
    QVERIFY(
        !CadDrawingLayers::addToProject(&project, second, QStringLiteral("가수리 (도면)"), kOtherId, &error).isEmpty());
    QCOMPARE(titleGroupCount(project, QStringLiteral("가수리 (도면)")), 1);
    QCOMPARE(titleGroup(project, QStringLiteral("가수리 (도면)"))->children().size(), 2);
    QCOMPARE(project.mapLayers().size(), 2);
    QCOMPARE(CadDrawingLayers::drawingIdOfGroup(&project, QStringLiteral("가수리 (도면)")), kOtherId);
  }

  void layersOf_andAlignLayerOf() {
    QTemporaryDir tmp;
    const QString both = writeGpkg(tmp, QStringLiteral("가수리"), {line(), text()});
    const QString textOnly = writeGpkg(tmp, QStringLiteral("글자만"), {text()});
    QgsProject project;
    QString error;
    CadDrawingLayers::addToProject(&project, both, QStringLiteral("가수리 (도면)"), kId, &error);
    CadDrawingLayers::addToProject(&project, textOnly, QStringLiteral("글자만 (도면)"), kOtherId, &error);
    QCOMPARE(CadDrawingLayers::layersOf(&project, kId).size(), 2);
    QgsVectorLayer* align = CadDrawingLayers::alignLayerOf(&project, kId);
    QVERIFY(align);
    QCOMPARE(align->name(), QStringLiteral("선"));
    QgsVectorLayer* onlyText = CadDrawingLayers::alignLayerOf(&project, kOtherId);
    QVERIFY(onlyText);
    QCOMPARE(onlyText->name(), QStringLiteral("글자"));
  }

  void hideCompanions_keepsTheAlignLayer() {
    QTemporaryDir tmp;
    const QString gpkg = writeGpkg(tmp, QStringLiteral("가수리"), {line(), text()});
    QgsProject project;
    QString error;
    const QList<QgsVectorLayer*> layers =
        CadDrawingLayers::addToProject(&project, gpkg, QStringLiteral("가수리 (도면)"), kId, &error);
    QgsVectorLayer* lines = named(layers, QStringLiteral("선"));
    QgsVectorLayer* texts = named(layers, QStringLiteral("글자"));
    QVERIFY(lines && texts);
    QCOMPARE(CadDrawingLayers::drawingIdOf(lines), kId);
    const QStringList hidden = CadDrawingLayers::hideCompanions(&project, kId, lines);
    QCOMPARE(hidden, QStringList{texts->id()});
    QVERIFY(!project.layerTreeRoot()->findLayer(texts->id())->itemVisibilityChecked());
    QVERIFY(project.layerTreeRoot()->findLayer(lines->id())->itemVisibilityChecked());
    CadDrawingLayers::showLayers(&project, hidden);
    QVERIFY(project.layerTreeRoot()->findLayer(texts->id())->itemVisibilityChecked());
    QVERIFY(CadDrawingLayers::hideCompanions(&project, QString(), lines).isEmpty());
  }

  // 90° 회전 · 2배 · (10, 20) 이동을 도면의 선·글자 표 모두에 같이 적용한다.
  void saveAlignment_movesEveryLayerOfTheDrawing() {
    QTemporaryDir tmp;
    const QString gpkg = writeGpkg(tmp, QStringLiteral("가수리"), {line(), text()});
    QgsProject project;
    QString error;
    CadDrawingLayers::addToProject(&project, gpkg, QStringLiteral("가수리 (도면)"), kId, &error);
    GeorefService::Affine a;
    a.a = 0;
    a.b = -2;
    a.c = 10;
    a.d = 2;
    a.e = 0;
    a.f = 20;
    a.valid = true;
    QVERIFY2(CadDrawingLayers::saveAlignment(&project, kId, a, &error), qUtf8Printable(error));
    QgsVectorLayer lines(gpkg + QStringLiteral("|layername=lines"), QStringLiteral("l"), QStringLiteral("ogr"));
    QgsFeature f;
    QVERIFY(lines.getFeatures().nextFeature(f));
    const QgsPointXY start(f.geometry().vertexAt(0));  // (0, 0) → (10, 20)
    QVERIFY2(start.distance(QgsPointXY(10, 20)) < 1e-6, qUtf8Printable(start.toString(6)));
    QgsVectorLayer texts(gpkg + QStringLiteral("|layername=texts"), QStringLiteral("t"), QStringLiteral("ogr"));
    QgsFeature t;
    QVERIFY(texts.getFeatures().nextFeature(t));
    QVERIFY2(t.geometry().asPoint().distance(QgsPointXY(0, 30)) < 1e-6, qUtf8Printable(t.geometry().asWkt()));  // (5, 5)
    QVERIFY(std::abs(t.attribute(QStringLiteral("text_angle")).toDouble() - 120.0) < 1e-9);
    QVERIFY(std::abs(t.attribute(QStringLiteral("text_height")).toDouble() - 5.0) < 1e-9);
  }

  // 새 변환본을 열지 못하면 같은 제목의 예전 묶음을 지우지 않는다.
  void addToProject_failureKeepsThePreviousGroup() {
    QTemporaryDir tmp;
    const QString gpkg = writeGpkg(tmp, QStringLiteral("가수리"), {line(), text()});
    QgsProject project;
    QString error;
    QCOMPARE(CadDrawingLayers::addToProject(&project, gpkg, QStringLiteral("가수리 (도면)"), kId, &error).size(), 2);
    QVERIFY(CadDrawingLayers::addToProject(&project, tmp.filePath(QStringLiteral("없음.gpkg")), QStringLiteral("가수리 (도면)"),
                                           kOtherId, &error)
                .isEmpty());
    QCOMPARE(CadDrawingLayers::drawingIdOfGroup(&project, QStringLiteral("가수리 (도면)")), kId);
    QCOMPARE(CadDrawingLayers::layersOf(&project, kId).size(), 2);
  }

  // 도면 묶음을 사용자가 다른 묶음 안으로 옮겨도 같은 원본만 그 묶음을 이어 쓰고, 바깥 묶음은 도면으로 보지 않는다.
  void titleFor_seesDrawingGroupsMovedIntoAFolder() {
    QTemporaryDir tmp;
    QgsProject project;
    QString error;
    QVERIFY(!CadDrawingLayers::addToProject(&project, writeGpkg(tmp, QStringLiteral("가수리"), {line()}),
                                            QStringLiteral("가수리 (도면)"), kId, &error).isEmpty());
    QgsLayerTreeGroup* reference = project.layerTreeRoot()->findGroup(HeritageImport::referenceGroupName());
    QgsLayerTreeGroup* drawing = reference->findGroup(QStringLiteral("가수리 (도면)"));
    reference->addGroup(QStringLiteral("묶음"))->addChildNode(drawing->clone());
    reference->removeChildNode(drawing);
    QCOMPARE(CadDrawingLayers::titleFor(&project, QStringLiteral("C:/x/가수리.dxf")), QStringLiteral("가수리 (도면)"));
    QCOMPARE(CadDrawingLayers::titleFor(&project, QStringLiteral("D:/y/가수리.dxf")), QStringLiteral("가수리 (도면 2)"));
    QVERIFY(CadDrawingLayers::drawingIdOfGroup(&project, QStringLiteral("묶음")).isEmpty());
    QCOMPARE(CadDrawingLayers::drawingIdOfGroup(&project, QStringLiteral("가수리 (도면)")), kId);
  }

  void removeFromProject_dropsLayersAndGroup() {
    QTemporaryDir tmp;
    const QString gpkg = writeGpkg(tmp, QStringLiteral("가수리"), {line(), text()});
    QgsProject project;
    QString error;
    QCOMPARE(CadDrawingLayers::addToProject(&project, gpkg, QStringLiteral("가수리 (도면)"), kId, &error).size(), 2);
    CadDrawingLayers::removeFromProject(&project, kId);
    QVERIFY(CadDrawingLayers::layersOf(&project, kId).isEmpty());
    QVERIFY(!titleGroup(project, QStringLiteral("가수리 (도면)")));
    QVERIFY(project.mapLayers().isEmpty());
  }
};

int main(int argc, char** argv) {
  QStandardPaths::setTestModeEnabled(true);
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  int result = 0;
  {
    TestCadLayers tests;
    result = QTest::qExec(&tests, argc, argv);
  }
  QgsApplication::exitQgis();
  return result;
}

#include "test_cad_layers.moc"
