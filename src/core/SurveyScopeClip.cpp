#include "SurveyScopeClip.h"

#include "LayerOps.h"

#include <qgsproject.h>
#include <qgsexception.h>
#include <qgsfeature.h>
#include <qgsfeaturerequest.h>
#include <qgsgeometryengine.h>
#include <qgscoordinatetransform.h>
#include <qgsvectorlayer.h>

#include <cmath>
#include <memory>

namespace SurveyScopeClip {
namespace {

QgsGeometry scopeOf(QgsProject* project, const QgsCoordinateReferenceSystem& workCrs,
                    const QgsCoordinateTransformContext& context, QString* error) {
  const QgsGeometry survey = surveyUnion(project, workCrs, context, error);
  if (survey.isEmpty()) return {};
  const QgsGeometry scope = bufferMeters(survey);
  if (scope.isEmpty() || !scope.isGeosValid()) {
    if (error) *error = QStringLiteral("조사 주변 5km 범위를 만들지 못했습니다.");
    return {};
  }
  return scope;
}

bool intersectsScope(const QgsGeometry& geometry, QgsGeometryEngine* inside) {
  if (!inside || geometry.isEmpty()) return false;
  return inside->intersects(geometry.constGet());
}

}  // namespace

QgsGeometry surveyUnion(QgsProject* project, const QgsCoordinateReferenceSystem& workCrs,
                        const QgsCoordinateTransformContext& context, QString* error) {
  if (error) error->clear();
  if (!project || !workCrs.isValid()) return {};
  QVector<QgsGeometry> parts;
  try {
    for (QgsVectorLayer* layer : LayerOps::surveyAreaLayers(project)) {
      if (!layer || !layer->isValid()) continue;
      QgsFeatureRequest filter;
      filter.setNoAttributes();
      auto features = layer->getFeatures(filter);
      QgsFeature feature;
      const QgsCoordinateTransform transform(layer->crs(), workCrs, context);
      while (features.nextFeature(feature)) {
        QgsGeometry geometry = feature.geometry();
        if (geometry.isEmpty()) continue;
        if (layer->crs() != workCrs &&
            geometry.transform(transform) != Qgis::GeometryOperationResult::Success) {
          if (error) *error = QStringLiteral("조사구역 좌표계를 확인하지 못했습니다.");
          return {};
        }
        parts.append(geometry);
      }
    }
  } catch (const QgsCsException&) {
    if (error) *error = QStringLiteral("조사구역 좌표계를 확인하지 못했습니다.");
    return {};
  }
  if (parts.isEmpty()) return {};
  return QgsGeometry::unaryUnion(parts);
}

QgsGeometry bufferMeters(const QgsGeometry& survey, double meters) {
  if (survey.isEmpty() || !std::isfinite(meters) || meters <= 0.) return {};
  return survey.buffer(meters, 24);
}

bool keepIntersectingIfSurvey(QgsProject* project, QgsVectorLayer* layer, QString* error) {
  if (error) error->clear();
  if (!layer || !layer->isValid()) {
    if (error) *error = QStringLiteral("자를 레이어가 없습니다.");
    return false;
  }
  const QgsCoordinateReferenceSystem workCrs =
      (project && project->crs().isValid()) ? project->crs() : layer->crs();
  const QgsCoordinateTransformContext context =
      project ? project->transformContext() : QgsCoordinateTransformContext();
  const QgsGeometry scope = scopeOf(project, workCrs, context, error);
  if (scope.isEmpty()) return error ? error->isEmpty() : true;

  try {
    const bool sameCrs = layer->crs() == workCrs;
    const QgsCoordinateTransform toWork(layer->crs(), workCrs, context);
    std::unique_ptr<QgsGeometryEngine> inside(QgsGeometry::createGeometryEngine(scope.constGet()));
    inside->prepareGeometry();
    QgsFeatureRequest request;
    request.setNoAttributes();
    if (sameCrs) request.setFilterRect(scope.boundingBox());
    else {
      const QgsCoordinateTransform toSource(workCrs, layer->crs(), context);
      request.setFilterRect(toSource.transformBoundingBox(scope.boundingBox()));
    }
    QgsFeatureIds keep;
    QgsFeature feature;
    auto features = layer->getFeatures(request);
    while (features.nextFeature(feature)) {
      QgsGeometry geometry = feature.geometry();
      if (geometry.isEmpty()) continue;
      if (!sameCrs && geometry.transform(toWork) != Qgis::GeometryOperationResult::Success) {
        if (error) *error = QStringLiteral("유적 좌표를 조사 좌표계로 변환하지 못했습니다.");
        return false;
      }
      if (intersectsScope(geometry, inside.get())) keep.insert(feature.id());
    }
    QgsFeatureIds drop;
    auto all = layer->getFeatures(QgsFeatureRequest().setNoAttributes());
    while (all.nextFeature(feature)) {
      if (!keep.contains(feature.id())) drop.insert(feature.id());
    }
    if (drop.isEmpty()) return true;
    if (!layer->startEditing()) {
      if (error) *error = QStringLiteral("조사 주변 5km로 자를 수 없습니다.");
      return false;
    }
    if (!layer->deleteFeatures(drop) || !layer->commitChanges()) {
      layer->rollBack();
      if (error) *error = QStringLiteral("조사 주변 5km 밖 도형을 빼지 못했습니다.");
      return false;
    }
    return true;
  } catch (const QgsCsException&) {
    if (error) *error = QStringLiteral("조사 범위와 자료 좌표계를 변환하지 못했습니다.");
    return false;
  }
}

QgsVectorLayer* intersectingCopyIfSurvey(QgsProject* project, QgsVectorLayer* source,
                                         const QString& name, QString* error) {
  if (error) error->clear();
  if (!source || !source->isValid()) {
    if (error) *error = QStringLiteral("자를 레이어가 없습니다.");
    return nullptr;
  }
  const QgsCoordinateReferenceSystem workCrs =
      (project && project->crs().isValid()) ? project->crs() : source->crs();
  const QgsCoordinateTransformContext context =
      project ? project->transformContext() : QgsCoordinateTransformContext();
  const QgsGeometry scope = scopeOf(project, workCrs, context, error);
  if (scope.isEmpty()) return error && !error->isEmpty() ? nullptr : source;

  const QString type = source->geometryType() == Qgis::GeometryType::Polygon
                           ? QStringLiteral("Polygon")
                           : source->geometryType() == Qgis::GeometryType::Point
                                 ? QStringLiteral("Point")
                                 : QStringLiteral("LineString");
  const QString auth = workCrs.authid().isEmpty() ? QStringLiteral("EPSG:5186") : workCrs.authid();
  auto* mem = new QgsVectorLayer(QStringLiteral("%1?crs=%2").arg(type, auth), name, QStringLiteral("memory"));
  if (!mem->isValid()) {
    delete mem;
    if (error) *error = QStringLiteral("조사 주변 5km 표시 레이어를 만들지 못했습니다.");
    return nullptr;
  }
  mem->dataProvider()->addAttributes(source->fields().toList());
  mem->updateFields();
  try {
    const bool sameCrs = source->crs() == workCrs;
    const QgsCoordinateTransform toWork(source->crs(), workCrs, context);
    std::unique_ptr<QgsGeometryEngine> inside(QgsGeometry::createGeometryEngine(scope.constGet()));
    inside->prepareGeometry();
    QgsFeatureRequest request;
    if (sameCrs) request.setFilterRect(scope.boundingBox());
    else {
      const QgsCoordinateTransform toSource(workCrs, source->crs(), context);
      request.setFilterRect(toSource.transformBoundingBox(scope.boundingBox()));
    }
    QgsFeatureList outs;
    QgsFeature feature;
    auto features = source->getFeatures(request);
    while (features.nextFeature(feature)) {
      QgsGeometry geometry = feature.geometry();
      if (geometry.isEmpty()) continue;
      if (!sameCrs && geometry.transform(toWork) != Qgis::GeometryOperationResult::Success) {
        delete mem;
        if (error) *error = QStringLiteral("선 좌표를 조사 좌표계로 변환하지 못했습니다.");
        return nullptr;
      }
      if (!intersectsScope(geometry, inside.get())) continue;
      QgsFeature output(mem->fields());
      output.setGeometry(geometry);
      output.setAttributes(feature.attributes());
      outs.append(output);
    }
    if (!outs.isEmpty()) mem->dataProvider()->addFeatures(outs);
    mem->updateExtents();
    if (mem->dataProvider()) mem->dataProvider()->createSpatialIndex();
    mem->setCrs(workCrs);
    return mem;
  } catch (const QgsCsException&) {
    delete mem;
    if (error) *error = QStringLiteral("조사 범위와 자료 좌표계를 변환하지 못했습니다.");
    return nullptr;
  }
}

}  // namespace SurveyScopeClip
