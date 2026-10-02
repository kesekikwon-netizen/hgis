#include "CadDrawingLayers.h"

#include "CadDrawingStore.h"
#include "HeritageImport.h"
#include "LayerOps.h"

#include <QFileInfo>

#include <memory>

#include <qgsfillsymbol.h>
#include <qgslayertree.h>
#include <qgslinesymbol.h>
#include <qgsmarkersymbol.h>
#include <qgsnullsymbolrenderer.h>
#include <qgspallabeling.h>
#include <qgsproject.h>
#include <qgsproperty.h>
#include <qgsproviderregistry.h>
#include <qgssinglesymbolrenderer.h>
#include <qgssymbollayer.h>
#include <qgstextformat.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayerlabeling.h>

namespace CadDrawingLayers {
namespace {

QgsProperty colorField() { return QgsProperty::fromField(QStringLiteral("color")); }

void styleLines(QgsVectorLayer* layer) {
  std::unique_ptr<QgsLineSymbol> symbol = QgsLineSymbol::createSimple(
      {{QStringLiteral("line_width"), QStringLiteral("0.26")}, {QStringLiteral("line_width_unit"), QStringLiteral("MM")}});
  symbol->symbolLayer(0)->setDataDefinedProperty(QgsSymbolLayer::Property::StrokeColor, colorField());
  layer->setRenderer(new QgsSingleSymbolRenderer(symbol.release()));
}

void styleFills(QgsVectorLayer* layer) {
  std::unique_ptr<QgsFillSymbol> symbol = QgsFillSymbol::createSimple(
      {{QStringLiteral("outline_width"), QStringLiteral("0.26")},
       {QStringLiteral("outline_width_unit"), QStringLiteral("MM")}});
  QgsSymbolLayer* fill = symbol->symbolLayer(0);
  fill->setDataDefinedProperty(QgsSymbolLayer::Property::FillColor,
                               QgsProperty::fromExpression(QStringLiteral("set_color_part(\"color\", 'alpha', 77)")));
  fill->setDataDefinedProperty(QgsSymbolLayer::Property::StrokeColor, colorField());
  layer->setRenderer(new QgsSingleSymbolRenderer(symbol.release()));
}

void stylePoints(QgsVectorLayer* layer) {
  std::unique_ptr<QgsMarkerSymbol> symbol =
      QgsMarkerSymbol::createSimple({{QStringLiteral("name"), QStringLiteral("circle")},
                                     {QStringLiteral("size"), QStringLiteral("1.5")},
                                     {QStringLiteral("size_unit"), QStringLiteral("MM")}});
  QgsSymbolLayer* marker = symbol->symbolLayer(0);
  marker->setDataDefinedProperty(QgsSymbolLayer::Property::FillColor, colorField());
  marker->setDataDefinedProperty(QgsSymbolLayer::Property::StrokeColor, colorField());
  layer->setRenderer(new QgsSingleSymbolRenderer(symbol.release()));
}

// 점 기호 없이 글자만 CAD 크기(지도 단위 m)·각도·색으로 놓는다. 겹쳐도 지우지 않는다(CAD 도면 그대로).
void styleTexts(QgsVectorLayer* layer) {
  layer->setRenderer(new QgsNullSymbolRenderer());
  QgsPalLayerSettings settings;
  settings.fieldName = QStringLiteral("text");
  settings.placement = Qgis::LabelPlacement::OverPoint;
  settings.placementSettings().setOverlapHandling(Qgis::LabelOverlapHandling::AllowOverlapAtNoCost);
  QgsTextFormat format;
  QFont font = format.font();
  font.setFamily(QStringLiteral("Malgun Gothic"));
  format.setFont(font);
  format.setSizeUnit(Qgis::RenderUnit::MapUnits);
  settings.setFormat(format);
  QgsPropertyCollection& properties = settings.dataDefinedProperties();
  properties.setProperty(QgsPalLayerSettings::Property::Size, QgsProperty::fromField(QStringLiteral("text_height")));
  // QGIS 라벨 회전은 시계 방향, CAD 각도는 반시계 방향이다.
  properties.setProperty(QgsPalLayerSettings::Property::LabelRotation,
                         QgsProperty::fromExpression(QStringLiteral("-\"text_angle\"")));
  properties.setProperty(QgsPalLayerSettings::Property::Color, colorField());
  // CAD 기준점(GDAL LABEL p:, 1~3 기준선·4~6 가운데·7~9 위·10~12 아래) → QGIS 사분면(0 왼위 … 8 오른아래).
  properties.setProperty(
      QgsPalLayerSettings::Property::OffsetQuad,
      QgsProperty::fromExpression(QStringLiteral(
          "CASE \"text_anchor\" WHEN 1 THEN 2 WHEN 10 THEN 2 WHEN 2 THEN 1 WHEN 11 THEN 1 WHEN 3 THEN 0 "
          "WHEN 12 THEN 0 WHEN 4 THEN 5 WHEN 5 THEN 4 WHEN 6 THEN 3 WHEN 7 THEN 8 WHEN 8 THEN 7 WHEN 9 THEN 6 "
          "ELSE 2 END")));
  layer->setLabeling(new QgsVectorLayerSimpleLabeling(settings));
  layer->setLabelsEnabled(true);
}

struct Part {
  QString table;
  QString title;
  void (*style)(QgsVectorLayer*);
};

// 묶음 안 순서(위에서부터).
const QList<Part>& parts() {
  static const QList<Part> list = {{QStringLiteral("texts"), QStringLiteral("글자"), styleTexts},
                                   {QStringLiteral("points"), QStringLiteral("점"), stylePoints},
                                   {QStringLiteral("lines"), QStringLiteral("선"), styleLines},
                                   {QStringLiteral("fills"), QStringLiteral("면"), styleFills}};
  return list;
}

QString tableOf(const QgsMapLayer* layer) {
  return QgsProviderRegistry::instance()
      ->decodeUri(QStringLiteral("ogr"), layer->source())
      .value(QStringLiteral("layerName"))
      .toString();
}

QgsLayerTreeGroup* referenceGroup(const QgsProject* project) {
  QgsLayerTree* root = project->layerTreeRoot();
  QgsLayerTreeGroup* reference = root->findGroup(HeritageImport::referenceGroupName());
  return reference ? reference : root->addGroup(HeritageImport::referenceGroupName());
}

}  // namespace

QString groupTitle(const QString& sourcePath) {
  return QFileInfo(sourcePath).completeBaseName() + QStringLiteral(" (도면)");
}

QList<QgsVectorLayer*> addToProject(QgsProject* project, const QString& gpkgPath, const QString& title,
                                    const QString& drawingId, QString* error) {
  QList<QgsVectorLayer*> added;
  if (!project) return added;
  const QString previous = drawingIdOfGroup(project, title);
  if (!previous.isEmpty()) removeFromProject(project, previous);
  QgsLayerTreeGroup* reference = referenceGroup(project);
  if (QgsLayerTreeGroup* empty = reference->findGroup(title)) reference->removeChildNode(empty);
  // 참조 지도 맨 위에 둔다: 배경 지도에 가리지 않는다. 참조 지도가 꺼져 있었으면 켠다.
  QgsLayerTreeGroup* group = reference->insertGroup(0, title);
  group->setItemVisibilityCheckedParentRecursive(true);
  const QStringList tables = CadDrawingStore::tableNames(gpkgPath);
  for (const Part& part : parts()) {
    if (!tables.contains(part.table)) continue;
    auto* layer = new QgsVectorLayer(gpkgPath + QStringLiteral("|layername=") + part.table, part.title,
                                     QStringLiteral("ogr"));
    if (!layer->isValid()) {
      delete layer;
      removeFromProject(project, drawingId);
      reference->removeChildNode(group);
      if (error) *error = QStringLiteral("도면 레이어를 열지 못했습니다.");
      return {};
    }
    part.style(layer);
    LayerOps::markReferenceLayer(layer);
    layer->setCustomProperty(QStringLiteral("ka_hgis/imported_reference"), true);
    layer->setCustomProperty(QString::fromLatin1(kPropDrawing), drawingId);
    project->addMapLayer(layer, false);
    group->addLayer(layer);
    added << layer;
  }
  if (added.isEmpty()) {
    reference->removeChildNode(group);
    if (error) *error = QStringLiteral("도면 레이어를 열지 못했습니다.");
  }
  return added;
}

QList<QgsVectorLayer*> layersOf(const QgsProject* project, const QString& drawingId) {
  QList<QgsVectorLayer*> layers;
  if (!project || drawingId.isEmpty()) return layers;
  for (QgsMapLayer* layer : project->mapLayers()) {
    auto* vector = qobject_cast<QgsVectorLayer*>(layer);
    if (vector && vector->customProperty(QString::fromLatin1(kPropDrawing)).toString() == drawingId) layers << vector;
  }
  return layers;
}

QgsVectorLayer* alignLayerOf(const QgsProject* project, const QString& drawingId) {
  const QList<QgsVectorLayer*> layers = layersOf(project, drawingId);
  for (const QString& table : {QStringLiteral("lines"), QStringLiteral("fills"), QStringLiteral("points"),
                               QStringLiteral("texts")})
    for (QgsVectorLayer* layer : layers)
      if (tableOf(layer) == table) return layer;
  return nullptr;
}

void removeFromProject(QgsProject* project, const QString& drawingId) {
  if (!project) return;
  QStringList ids;
  QList<QgsLayerTreeGroup*> groups;
  for (QgsVectorLayer* layer : layersOf(project, drawingId)) {
    ids << layer->id();
    QgsLayerTreeLayer* node = project->layerTreeRoot()->findLayer(layer->id());
    auto* parent = node ? qobject_cast<QgsLayerTreeGroup*>(node->parent()) : nullptr;
    if (parent && !groups.contains(parent)) groups << parent;
  }
  project->removeMapLayers(ids);
  for (QgsLayerTreeGroup* group : groups) {
    auto* parent = qobject_cast<QgsLayerTreeGroup*>(group->parent());
    if (parent && group->children().isEmpty() && group->name() != HeritageImport::referenceGroupName())
      parent->removeChildNode(group);
  }
}

QString drawingIdOfGroup(const QgsProject* project, const QString& title) {
  if (!project) return {};
  QgsLayerTreeGroup* reference = project->layerTreeRoot()->findGroup(HeritageImport::referenceGroupName());
  QgsLayerTreeGroup* group = reference ? reference->findGroup(title) : nullptr;
  if (!group) return {};
  for (QgsLayerTreeLayer* node : group->findLayers()) {
    const QString id = node->layer() ? node->layer()->customProperty(QString::fromLatin1(kPropDrawing)).toString()
                                     : QString();
    if (!id.isEmpty()) return id;
  }
  return {};
}

}  // namespace CadDrawingLayers
