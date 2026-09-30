#include "PolygonErase.h"

#include "KaSessionLog.h"
#include "LayerOps.h"
#include "MeasureOps.h"

#include <algorithm>

#include <qgscoordinatetransform.h>
#include <qgscoordinatetransformcontext.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgsgeometry.h>
#include <qgslayertree.h>
#include <qgsproject.h>
#include <qgsvectordataprovider.h>
#include <qgswkbtypes.h>

namespace {

QgsGeometry cleaned(const QgsGeometry& geometry) {
  if (geometry.isNull() || geometry.isEmpty()) return QgsGeometry();
  return geometry.isGeosValid() ? geometry : geometry.makeValid();
}

QgsGeometry geometryOf(const PolygonErase::Shape& shape) {
  if (!shape.layer || FID_IS_NULL(shape.fid)) return QgsGeometry();
  const QgsFeature feature = shape.layer->getFeature(shape.fid);
  if (!feature.isValid() || !feature.hasGeometry()) return QgsGeometry();
  return cleaned(feature.geometry());
}

// from 레이어 좌표의 도형을 to 레이어 좌표로 옮긴다. 옮기지 못하면 빈 도형.
QgsGeometry moved(QgsGeometry geometry, const QgsVectorLayer* from, const QgsVectorLayer* to,
                  const QgsProject* project) {
  if (!from || !to || from == to || !from->crs().isValid() || !to->crs().isValid() ||
      from->crs() == to->crs())
    return geometry;
  QgsCoordinateTransform transform(from->crs(), to->crs(),
                                   project ? project->transformContext() : QgsCoordinateTransformContext());
  transform.setBallparkTransformsAreAppropriate(true);
  try {
    if (geometry.transform(transform) != Qgis::GeometryOperationResult::Success) return QgsGeometry();
  } catch (...) {
    KaSessionLog::line(QStringLiteral("[except] core/PolygonErase.cpp:transform"));
    return QgsGeometry();
  }
  return geometry;
}

// 테두리만 닿은 것은 겹친 것이 아니다.
bool overlapWithArea(const QgsGeometry& a, const QgsGeometry& b) {
  return !a.isNull() && !b.isNull() && a.intersects(b) && !a.touches(b);
}

bool shownInLegend(const QgsVectorLayer* layer, const QgsProject* project) {
  const QgsLayerTree* root = project ? project->layerTreeRoot() : nullptr;
  if (!root) return true;
  const QgsLayerTreeLayer* node = root->findLayer(layer->id());
  return !node || node->isVisible();
}

struct Under {
  PolygonErase::Shape shape;
  bool remains = false;  // 자르고 나서 남는 면이 있는가
};

// 자르는 면(레이어 좌표) 아래에 넓이를 갖고 겹친 면들.
QList<Under> shapesUnder(QgsVectorLayer* layer, const QgsGeometry& cutter, QgsFeatureId skip) {
  QList<Under> found;
  QgsFeatureRequest request;
  request.setFilterRect(cutter.boundingBox());
  QgsFeatureIterator it = layer->getFeatures(request);
  QgsFeature feature;
  while (it.nextFeature(feature)) {
    if (feature.id() == skip || !feature.hasGeometry()) continue;
    const QgsGeometry geometry = cleaned(feature.geometry());
    if (!overlapWithArea(geometry, cutter)) continue;
    Under under;
    under.shape.layer = layer;
    under.shape.fid = feature.id();
    under.remains = !PolygonErase::erased(geometry, cutter).isNull();
    found.append(under);
  }
  return found;
}

// 한 면만 담는 레이어(POLYGON)는 여러 조각을 한 도형으로 간직하지 못한다. 저장하면서 조각들이
// 한 면의 고리로 합쳐져 넓이가 틀어진다. 그런 레이어에서는 조각마다 도형 하나로 나눈다.
// 큰 조각이 먼저 온다.
QVector<QgsGeometry> partsFor(const QgsVectorLayer* layer, QgsGeometry geometry) {
  QVector<QgsGeometry> parts;
  if (QgsWkbTypes::isMultiType(layer->wkbType())) {
    geometry.convertToMultiType();
    parts.append(geometry);
    return parts;
  }
  if (!geometry.isMultipart()) {
    parts.append(geometry);
    return parts;
  }
  const QVector<QgsGeometry> pieces = geometry.asGeometryCollection();
  for (const QgsGeometry& piece : pieces) {
    if (!piece.isEmpty() && piece.type() == Qgis::GeometryType::Polygon) parts.append(piece);
  }
  std::sort(parts.begin(), parts.end(),
            [](const QgsGeometry& a, const QgsGeometry& b) { return a.area() > b.area(); });
  return parts;
}

struct LayerEdit {
  QPointer<QgsVectorLayer> layer;
  QList<QPair<QgsFeatureId, QgsGeometry>> changes;
  QgsFeatureList additions;
  QgsFeatureIds deletions;
};

int editIndexFor(QList<LayerEdit>& edits, QgsVectorLayer* layer) {
  for (int i = 0; i < edits.size(); ++i) {
    if (edits.at(i).layer == layer) return i;
  }
  LayerEdit edit;
  edit.layer = layer;
  edits.append(edit);
  return edits.size() - 1;
}

}  // namespace

namespace PolygonErase {

bool editable(const QgsVectorLayer* layer) {
  if (!layer || !layer->isValid() || layer->readOnly()) return false;
  if (layer->geometryType() != Qgis::GeometryType::Polygon) return false;
  if (LayerOps::isCadastralLayer(layer) || LayerOps::isReferenceLayer(layer) ||
      LayerOps::isReferenceOrBasemapLayer(layer))
    return false;
  const QgsVectorDataProvider* provider = layer->dataProvider();
  const Qgis::VectorProviderCapabilities need =
      Qgis::VectorProviderCapability::ChangeGeometries | Qgis::VectorProviderCapability::DeleteFeatures;
  return provider && (provider->capabilities() & need) == need;
}

QgsGeometry erased(const QgsGeometry& target, const QgsGeometry& cutter) {
  const QgsGeometry from = cleaned(target);
  const QgsGeometry with = cleaned(cutter);
  if (from.isNull()) return QgsGeometry();
  if (with.isNull()) return from;
  QgsGeometry rest = from.difference(with);
  if (rest.isNull() || rest.isEmpty()) return QgsGeometry();
  // 빼고 나면 선·점 부스러기가 섞여 나올 수 있다. 면만 남긴다.
  if (QgsWkbTypes::flatType(rest.wkbType()) == Qgis::WkbType::GeometryCollection &&
      !rest.convertGeometryCollectionToSubclass(Qgis::GeometryType::Polygon))
    return QgsGeometry();
  if (rest.type() != Qgis::GeometryType::Polygon || rest.isEmpty()) return QgsGeometry();
  // 좌표 오차로 남은 실오라기는 남은 면으로 치지 않는다.
  if (rest.area() <= from.area() * 1e-9) return QgsGeometry();
  return rest;
}

Plan plan(const QList<Shape>& selected, QgsProject* project) {
  Plan out;
  struct Picked {
    Shape shape;
    QgsGeometry geometry;
  };
  QList<Picked> picked;
  for (const Shape& shape : selected) {
    if (!editable(shape.layer)) continue;
    const QgsGeometry geometry = geometryOf(shape);
    if (geometry.isNull() || geometry.type() != Qgis::GeometryType::Polygon) continue;
    Picked one;
    one.shape = shape;
    one.geometry = geometry;
    picked.append(one);
  }
  if (picked.isEmpty()) {
    out.hint = selected.isEmpty()
                   ? QStringLiteral("위에 그린 도형을 [도형선택]으로 고른 뒤 누르세요.")
                   : QStringLiteral("고른 도형으로는 지울 수 없습니다. 조사 데이터의 면 도형을 고르세요.");
    return out;
  }

  if (picked.size() == 1) {
    const Picked& cutter = picked.first();
    QList<Shape> remaining;
    bool covered = false;
    const auto collect = [&](const QList<Under>& under) {
      for (const Under& one : under) {
        if (one.remains) remaining.append(one.shape);
        else covered = true;
      }
    };
    collect(shapesUnder(cutter.shape.layer, cutter.geometry, cutter.shape.fid));
    // 같은 레이어에 겹친 면이 없을 때만 다른 레이어를 본다. 여러 개가 겹쳐 있으면
    // 어느 것을 지울지 알 수 없으므로 고르게 한다.
    if (remaining.isEmpty() && !covered && project) {
      const QMap<QString, QgsMapLayer*> layers = project->mapLayers();
      for (QgsMapLayer* mapLayer : layers) {
        auto* layer = qobject_cast<QgsVectorLayer*>(mapLayer);
        if (!layer || layer == cutter.shape.layer || !editable(layer) || !shownInLegend(layer, project))
          continue;
        const QgsGeometry inLayer = moved(cutter.geometry, cutter.shape.layer, layer, project);
        if (inLayer.isNull()) continue;
        collect(shapesUnder(layer, inLayer, FID_NULL));
      }
      if (remaining.size() > 1) {
        out.hint = QStringLiteral("아래에 겹친 도형이 %1개입니다. Shift를 누른 채 지울 도형도 함께 고른 뒤 "
                                  "다시 누르세요.")
                       .arg(remaining.size());
        return out;
      }
    }
    if (remaining.isEmpty()) {
      out.hint = covered ? QStringLiteral("고른 도형이 아래 도형을 모두 덮고 있어 남는 부분이 없습니다. "
                                          "위에 그린 작은 도형을 고르세요.")
                         : QStringLiteral("고른 도형 아래에 겹친 면이 없습니다.");
      return out;
    }
    out.cutters.append(cutter.shape);
    out.targets = remaining;
    return out;
  }

  // 둘 이상 골랐다: 가장 큰 면에서 나머지 면의 자리를 지운다.
  const QgsCoordinateTransformContext context =
      project ? project->transformContext() : QgsCoordinateTransformContext();
  int big = 0;
  double bigArea = -1.0;
  for (int i = 0; i < picked.size(); ++i) {
    const double area =
        MeasureOps::geometryAreaSquareMeters(picked.at(i).geometry, picked.at(i).shape.layer->crs(), context);
    if (area > bigArea) {
      bigArea = area;
      big = i;
    }
  }
  const Picked& target = picked.at(big);
  QgsGeometry rest = target.geometry;
  for (int i = 0; i < picked.size() && !rest.isNull(); ++i) {
    if (i == big) continue;
    const QgsGeometry inTarget =
        moved(picked.at(i).geometry, picked.at(i).shape.layer, target.shape.layer, project);
    if (!overlapWithArea(target.geometry, inTarget)) continue;
    out.cutters.append(picked.at(i).shape);
    rest = erased(rest, inTarget);
  }
  if (out.cutters.isEmpty()) {
    out.hint = QStringLiteral("고른 도형들이 서로 겹치지 않습니다.");
    return out;
  }
  if (rest.isNull()) {
    out.cutters.clear();
    out.hint = QStringLiteral("고른 작은 도형들이 큰 도형을 모두 덮어 남는 부분이 없습니다.");
    return out;
  }
  out.targets.append(target.shape);
  return out;
}

Outcome apply(const Plan& plan, QgsProject* project) {
  Outcome out;
  const auto fail = [&out](const QString& text) {
    out.erased = 0;
    out.added = 0;
    out.error = text;
    return out;
  };
  if (!plan.ready())
    return fail(plan.hint.isEmpty() ? QStringLiteral("지울 도형을 정하지 못했습니다.") : plan.hint);

  // 먼저 모두 계산한다. 하나라도 안 되면 아무것도 고치지 않는다.
  QList<LayerEdit> edits;
  for (const Shape& target : plan.targets) {
    QgsVectorLayer* layer = target.layer;
    if (!editable(layer)) return fail(QStringLiteral("고칠 수 없는 레이어의 도형입니다."));
    const QgsFeature feature = layer->getFeature(target.fid);
    QgsGeometry rest = feature.isValid() && feature.hasGeometry() ? cleaned(feature.geometry()) : QgsGeometry();
    if (rest.isNull()) return fail(QStringLiteral("지울 도형을 찾지 못했습니다. 도형을 다시 고르세요."));
    for (const Shape& cutter : plan.cutters) {
      if (cutter.layer == target.layer && cutter.fid == target.fid) continue;
      const QgsGeometry with = moved(geometryOf(cutter), cutter.layer, layer, project);
      if (with.isNull()) return fail(QStringLiteral("위에 그린 도형을 찾지 못했습니다. 도형을 다시 고르세요."));
      rest = erased(rest, with);
      if (rest.isNull()) return fail(QStringLiteral("남는 부분이 없어 지우지 않았습니다."));
    }
    const QVector<QgsGeometry> parts = partsFor(layer, rest);
    if (parts.isEmpty()) return fail(QStringLiteral("남는 부분이 없어 지우지 않았습니다."));
    const int at = editIndexFor(edits, layer);
    edits[at].changes.append(qMakePair(target.fid, parts.first()));
    for (int i = 1; i < parts.size(); ++i) {
      QgsAttributes attributes = feature.attributes();
      // 파일이 매기는 번호와, 도형마다 달라야 하는 uid 는 새 조각에 물려주지 않는다.
      const QgsAttributeList keys = layer->dataProvider()->pkAttributeIndexes();
      for (int key : keys) {
        if (key >= 0 && key < attributes.size()) attributes[key] = QVariant();
      }
      const int uid = layer->fields().lookupField(QStringLiteral("uid"));
      if (uid >= 0 && uid < attributes.size()) attributes[uid] = QVariant();
      QgsFeature extra(layer->fields());
      extra.setAttributes(attributes);
      extra.setGeometry(parts.at(i));
      edits[at].additions.append(extra);
    }
    ++out.erased;
    out.added += parts.size() - 1;
  }
  for (const Shape& cutter : plan.cutters) {
    if (!editable(cutter.layer)) return fail(QStringLiteral("고칠 수 없는 레이어의 도형입니다."));
    edits[editIndexFor(edits, cutter.layer)].deletions.insert(cutter.fid);
  }
  for (const LayerEdit& edit : edits) {
    if (edit.layer->isEditCommandActive())
      return fail(QStringLiteral("진행 중인 도형 편집을 마친 뒤 다시 실행하세요."));
  }

  QList<QgsVectorLayer*> done;
  for (const LayerEdit& edit : edits) {
    QgsVectorLayer* layer = edit.layer;
    QString error;
    const bool ok = LayerOps::runEditCommand(layer, QStringLiteral("겹친 곳 지우기"), [&]() {
      for (const auto& change : edit.changes) {
        QgsGeometry geometry = change.second;
        if (!layer->changeGeometry(change.first, geometry)) return false;
      }
      if (!edit.additions.isEmpty()) {
        QgsFeatureList additions = edit.additions;
        if (!layer->addFeatures(additions)) return false;
      }
      for (QgsFeatureId fid : edit.deletions) {
        if (!layer->deleteFeature(fid)) return false;
      }
      return true;
    }, &error);
    if (!ok) {
      // 앞서 고친 레이어를 되돌려 반쯤 지워진 상태를 남기지 않는다.
      for (auto it = done.crbegin(); it != done.crend(); ++it) LayerOps::undoLayerEdits(*it);
      return fail(QStringLiteral("%1: %2").arg(layer->name(), error));
    }
    done.append(layer);
  }
  out.ok = true;
  for (QgsVectorLayer* layer : done) out.layers.append(layer);
  return out;
}

}  // namespace PolygonErase
