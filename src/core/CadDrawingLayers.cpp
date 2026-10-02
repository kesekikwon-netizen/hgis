#include "CadDrawingLayers.h"

#include "CadDrawingReader.h"
#include "CadDrawingStore.h"
#include "HeritageImport.h"
#include "LayerOps.h"

#include <QFileInfo>

#include <algorithm>
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

// 바로 아래 레이어에 도면 id 가 적힌 묶음만 도면 묶음이다. 사용자가 도면 묶음을 옮겨 넣은 바깥 묶음은 아니다.
QString ownDrawingId(const QgsLayerTreeGroup* group) {
  for (QgsLayerTreeNode* node : group->children())
    if (QgsLayerTree::isLayer(node))
      if (const QString id = drawingIdOf(QgsLayerTree::toLayer(node)->layer()); !id.isEmpty()) return id;
  return {};
}

// 참조 지도 아래 어느 깊이에 있든 도면 묶음 모두.
QList<QgsLayerTreeGroup*> drawingGroups(const QgsProject* project) {
  QList<QgsLayerTreeGroup*> groups;
  QgsLayerTreeGroup* reference =
      project ? project->layerTreeRoot()->findGroup(HeritageImport::referenceGroupName()) : nullptr;
  for (QgsLayerTreeGroup* group : reference ? reference->findGroups(true) : QList<QgsLayerTreeGroup*>())
    if (!ownDrawingId(group).isEmpty()) groups << group;
  return groups;
}

}  // namespace

QString groupTitle(const QString& sourcePath) {
  return QFileInfo(sourcePath).completeBaseName() + QStringLiteral(" (도면)");
}

QString titleFor(const QgsProject* project, const QString& sourcePath) {
  const QString wanted = QFileInfo(sourcePath).absoluteFilePath();
  QStringList taken;
  for (QgsLayerTreeGroup* group : drawingGroups(project)) {
    const QList<QgsVectorLayer*> layers = layersOf(project, ownDrawingId(group));
    if (layers.isEmpty()) continue;
    const QString gpkg = QgsProviderRegistry::instance()
                             ->decodeUri(QStringLiteral("ogr"), layers.first()->source())
                             .value(QStringLiteral("path"))
                             .toString();
    const QString source = CadDrawingStore::readInfo(gpkg).sourcePath;
    if (QFileInfo(source).absoluteFilePath().compare(wanted, Qt::CaseInsensitive) == 0) return group->name();
    taken << group->name();
  }
  QString title = groupTitle(sourcePath);
  for (int n = 2; taken.contains(title); ++n)
    title = QStringLiteral("%1 (도면 %2)").arg(QFileInfo(sourcePath).completeBaseName(), QString::number(n));
  return title;
}

QList<QgsVectorLayer*> addToProject(QgsProject* project, const QString& gpkgPath, const QString& title,
                                    const QString& drawingId, QString* error) {
  QList<QgsVectorLayer*> added;
  if (!project) return added;
  // 새 레이어를 먼저 모두 연다. 하나라도 못 열면 같은 제목의 예전 묶음은 건드리지 않는다.
  const QStringList tables = CadDrawingStore::tableNames(gpkgPath);
  for (const Part& part : parts()) {
    if (!tables.contains(part.table)) continue;
    auto* layer = new QgsVectorLayer(gpkgPath + QStringLiteral("|layername=") + part.table, part.title,
                                     QStringLiteral("ogr"));
    added << layer;
    if (!layer->isValid()) break;
    part.style(layer);
    LayerOps::markReferenceLayer(layer);
    layer->setCustomProperty(QStringLiteral("ka_hgis/imported_reference"), true);
    layer->setCustomProperty(QString::fromLatin1(kPropDrawing), drawingId);
  }
  if (added.isEmpty() || !added.last()->isValid()) {
    qDeleteAll(added);
    if (error) *error = QStringLiteral("도면 레이어를 열지 못했습니다.");
    return {};
  }
  const QString previous = drawingIdOfGroup(project, title);
  if (!previous.isEmpty()) removeFromProject(project, previous);
  QgsLayerTreeGroup* reference = referenceGroup(project);
  if (QgsLayerTreeGroup* empty = reference->findGroup(title); empty && empty->children().isEmpty())
    if (auto* parent = qobject_cast<QgsLayerTreeGroup*>(empty->parent())) parent->removeChildNode(empty);
  // 참조 지도 맨 위에 둔다: 배경 지도에 가리지 않는다. 참조 지도가 꺼져 있었으면 켠다.
  QgsLayerTreeGroup* group = reference->insertGroup(0, title);
  group->setItemVisibilityCheckedParentRecursive(true);
  for (QgsVectorLayer* layer : added) {
    project->addMapLayer(layer, false);
    group->addLayer(layer);
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
  for (QgsLayerTreeGroup* group : drawingGroups(project))
    if (group->name() == title) return ownDrawingId(group);
  return {};
}

QgsRectangle viewExtent(QgsVectorLayer* layer) {
  if (drawingIdOf(layer).isEmpty()) return layer ? layer->extent() : QgsRectangle();
  QVector<QgsRectangle> boxes;
  QVector<QgsPointXY> centres;
  QgsFeature feature;
  for (QgsFeatureIterator it = layer->getFeatures(QgsFeatureRequest().setNoAttributes()); it.nextFeature(feature);)
    if (feature.hasGeometry() && !feature.geometry().isEmpty()) {
      boxes << feature.geometry().boundingBox();
      centres << boxes.last().center();
    }
  QgsRectangle near = CadDrawingReader::robustExtent(centres);
  if (near.isNull()) return layer->extent();
  near.grow(std::max({near.width(), near.height(), 5000.0}));  // 5 km 안은 도면(2지점 등), 그 밖 몇 개만 뺀다
  QgsRectangle view;
  view.setNull();
  for (const QgsRectangle& box : boxes)
    if (near.contains(box.center())) view.combineExtentWith(box);
  return view.isNull() ? layer->extent() : view;
}

QString drawingIdOf(const QgsMapLayer* layer) {
  return layer ? layer->customProperty(QString::fromLatin1(kPropDrawing)).toString() : QString();
}

QStringList hideCompanions(QgsProject* project, const QString& drawingId, const QgsMapLayer* keep) {
  QStringList hidden;
  if (!project) return hidden;
  for (QgsVectorLayer* layer : layersOf(project, drawingId)) {
    QgsLayerTreeLayer* node = project->layerTreeRoot()->findLayer(layer->id());
    if (layer == keep || !node || !node->itemVisibilityChecked()) continue;
    node->setItemVisibilityChecked(false);
    hidden << layer->id();
  }
  return hidden;
}

void showLayers(QgsProject* project, const QStringList& layerIds) {
  if (!project) return;
  for (const QString& id : layerIds)
    if (QgsLayerTreeLayer* node = project->layerTreeRoot()->findLayer(id)) node->setItemVisibilityChecked(true);
}

bool saveAlignment(QgsProject* project, const QString& drawingId, const GeorefService::Affine& a, QString* error) {
  const QList<QgsVectorLayer*> layers = layersOf(project, drawingId);
  if (layers.isEmpty()) {
    if (error) *error = QStringLiteral("맞출 도면 레이어가 없습니다.");
    return false;
  }
  if (!CadDrawingStore::applyAffine(layers, a, error)) return false;
  for (QgsVectorLayer* layer : layers) layer->triggerRepaint();
  return true;
}

}  // namespace CadDrawingLayers
