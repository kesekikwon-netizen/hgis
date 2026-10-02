#include "TrenchLayerEdit.h"

#include "LayerOps.h"

#include <QCryptographicHash>
#include <QList>
#include <QPair>
#include <QStringList>

#include <cmath>

#include <qgsabstractgeometry.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgsgeometry.h>
#include <qgsproject.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorlayer.h>

namespace TrenchLayerEdit {
namespace {

bool fail(QString* errorOut, const QString& message) {
  if (errorOut) *errorOut = message;
  return false;
}

// Undo can bring back an earlier generated grid; that one is not a hand edit either.
constexpr int kPlacedHistory = 8;

QStringList placedSignatures(QgsVectorLayer* layer) {
  return layer ? layer->customProperty(QString::fromLatin1(kPlacedLayoutKey)).toStringList() : QStringList();
}

// Unknown (no record) or different from every recently placed grid.
bool differsFromPlaced(QgsVectorLayer* layer) {
  const QStringList placed = placedSignatures(layer);
  return placed.isEmpty() || !placed.contains(layoutSignature(layer));
}

// Millimetres: float noise from reading back a saved file is not a hand edit.
void appendVertex(QString& row, double x, double y) {
  row += QStringLiteral("|%1,%2").arg(std::llround(x * 1000.0)).arg(std::llround(y * 1000.0));
}

QString hashRows(QStringList rows) {
  rows.sort();
  return QString::fromLatin1(
      QCryptographicHash::hash(rows.join(QLatin1Char('\n')).toUtf8(), QCryptographicHash::Sha1).toHex());
}

// What layoutSignature returns once these cells are the layer's only trenches.
QString cellsSignature(const std::vector<TrenchGridGenerator::Cell>& cells) {
  QStringList rows;
  for (const TrenchGridGenerator::Cell& cell : cells) {
    QString row = cell.name;
    for (const auto& point : cell.ring) appendVertex(row, point.first, point.second);
    rows << row;
  }
  return hashRows(rows);
}

}  // namespace

QString layoutSignature(QgsVectorLayer* layer) {
  if (!layer || !layer->isValid()) return {};
  const int nameIndex = layer->fields().indexOf(QStringLiteral("name"));
  QStringList rows;
  QgsFeature feature;
  QgsFeatureIterator it = layer->getFeatures();
  while (it.nextFeature(feature)) {
    QString row = nameIndex >= 0 ? feature.attribute(nameIndex).toString() : QString();
    const QgsGeometry geometry = feature.geometry();
    if (!geometry.isNull())
      for (auto v = geometry.vertices_begin(); v != geometry.vertices_end(); ++v) appendVertex(row, (*v).x(), (*v).y());
    rows << row;
  }
  return hashRows(rows);
}

void rememberPlacedLayout(QgsVectorLayer* layer) {
  if (!layer) return;
  QStringList placed = placedSignatures(layer);
  const QString current = layoutSignature(layer);
  placed.removeAll(current);
  placed.prepend(current);
  while (placed.size() > kPlacedHistory) placed.removeLast();
  layer->setCustomProperty(QString::fromLatin1(kPlacedLayoutKey), placed);
}

bool hasHandAdjustments(QgsVectorLayer* layer) {
  return layer && layer->isValid() && layer->featureCount() > 0 && differsFromPlaced(layer);
}

bool hasUnsavedHandEdits(QgsVectorLayer* layer) {
  return layer && layer->isValid() && layer->isModified() && differsFromPlaced(layer);
}

bool replaceWithCells(QgsVectorLayer* layer, const std::vector<TrenchGridGenerator::Cell>& cells,
                      const QString& authid, QString* errorOut) {
  if (errorOut) errorOut->clear();
  if (!layer || !layer->isValid())
    return fail(errorOut, QStringLiteral("시굴격자 레이어를 찾지 못했습니다. 조사를 다시 열어 확인하세요."));
  if (!TrenchGridGenerator::cellsValid(cells, errorOut)) return false;
  if (layer->geometryType() != Qgis::GeometryType::Polygon)
    return fail(errorOut, QStringLiteral("기존 시굴격자가 면 레이어가 아니어서 새 격자를 놓지 못했습니다. 기존 격자는 유지됩니다."));
  if (!authid.isEmpty() && layer->crs().authid() != authid)
    return fail(errorOut, QStringLiteral("기존 시굴격자의 좌표계(%1)가 조사구역 좌표계(%2)와 달라 새 격자를 놓지 "
                                         "못했습니다. 기존 격자는 유지됩니다.")
                              .arg(layer->crs().authid(), authid));
  const QgsFields fields = layer->fields();
  const int nameIndex = fields.indexOf(QStringLiteral("name"));
  const int widthIndex = fields.indexOf(QStringLiteral("width"));
  const int lengthIndex = fields.indexOf(QStringLiteral("length"));
  if (nameIndex < 0 || widthIndex < 0 || lengthIndex < 0)
    return fail(errorOut, QStringLiteral("기존 시굴격자의 속성 칸(name·width·length)이 없어 새 격자를 놓지 못했습니다."));

  QgsFeatureList added;
  added.reserve(static_cast<qsizetype>(cells.size()));
  for (const TrenchGridGenerator::Cell& cell : cells) {
    QgsPolylineXY ring;
    for (const auto& point : cell.ring) ring << QgsPointXY(point.first, point.second);
    QgsFeature feature(fields);
    feature.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY{ring}));
    feature.setAttribute(nameIndex, cell.name);
    feature.setAttribute(widthIndex, cell.width);
    feature.setAttribute(lengthIndex, cell.length);
    added << feature;
  }
  const QgsFeatureIds previous = layer->allFeatureIds();
  if (!LayerOps::runEditCommand(layer, QStringLiteral("시굴격자 배치"), [&]() {
        if (!previous.isEmpty() && !layer->deleteFeatures(previous)) return false;
        return layer->addFeatures(added);
      }, errorOut))
    return false;
  rememberPlacedLayout(layer);
  return true;
}

bool translate(QgsVectorLayer* layer, const QgsFeatureIds& fids, double dx, double dy,
               const QString& commandTitle, QString* errorOut) {
  if (errorOut) errorOut->clear();
  if (!layer || !layer->isValid())
    return fail(errorOut, QStringLiteral("시굴격자 레이어를 찾지 못했습니다."));
  if (!std::isfinite(dx) || !std::isfinite(dy))
    return fail(errorOut, QStringLiteral("옮길 거리를 계산하지 못했습니다. 다시 끌어 보세요."));
  QgsFeatureRequest request;
  if (!fids.isEmpty()) request.setFilterFids(fids);
  request.setNoAttributes();
  // Collect first: geometries change only inside the edit command, not under an open iterator.
  QList<QPair<QgsFeatureId, QgsGeometry>> moved;
  QgsFeature feature;
  QgsFeatureIterator it = layer->getFeatures(request);
  while (it.nextFeature(feature)) {
    QgsGeometry geometry = feature.geometry();
    if (geometry.isNull()) continue;
    if (geometry.translate(dx, dy) != Qgis::GeometryOperationResult::Success)
      return fail(errorOut, QStringLiteral("트렌치 도형을 옮기지 못했습니다. 기존 배치는 유지됩니다."));
    moved.append({feature.id(), geometry});
  }
  if (moved.isEmpty())
    return fail(errorOut, QStringLiteral("옮길 트렌치를 찾지 못했습니다."));
  return LayerOps::runEditCommand(layer, commandTitle, [&]() {
    for (auto& entry : moved)
      if (!layer->changeGeometry(entry.first, entry.second)) return false;
    return true;
  }, errorOut);
}

bool deleteTrench(QgsVectorLayer* layer, QgsFeatureId fid, QString* errorOut) {
  if (errorOut) errorOut->clear();
  if (!layer || !layer->isValid() || FID_IS_NULL(fid) || !layer->getFeature(fid).isValid())
    return fail(errorOut, QStringLiteral("지울 트렌치를 찾지 못했습니다."));
  return LayerOps::runEditCommand(layer, QStringLiteral("트렌치 삭제"),
                                  [&]() { return layer->deleteFeature(fid); }, errorOut);
}

PlaceResult placeGrid(QgsProject* project, const QString& surveyPath,
                      const std::vector<TrenchGridGenerator::Cell>& cells, const QString& authid,
                      const LoadLayer& loadLayer, const ConfirmReplace& confirm) {
  PlaceResult result;
  const auto failed = [&result](const QString& message, const QString& detail = QString()) {
    result.outcome = PlaceOutcome::Failed;
    result.message = message;
    result.detail = detail;
    return result;
  };
  QString error;
  if (QgsVectorLayer* layer = project ? LayerOps::findByLayerKey(project, QStringLiteral("trial_trench")) : nullptr) {
    result.layer = layer;
    // Reopening the dialog plans the same grid again: no edit, no unsaved mark, no undo step.
    if (layoutSignature(layer) == cellsSignature(cells)) {
      result.outcome = PlaceOutcome::Kept;
      result.message = QStringLiteral("같은 시굴격자가 이미 깔려 있습니다. 옮기거나 지우려면 「개별 편집」이나 「전체 이동」을 누르세요.");
      return result;
    }
    // User rule: a re-generation never throws away unsaved hand edits.
    if (hasUnsavedHandEdits(layer))
      return failed(QStringLiteral("시굴격자에 저장하지 않은 편집이 있습니다. 먼저 저장한 뒤 다시 만드세요. "
                                   "현재 편집은 그대로 유지됩니다."));
    if (hasHandAdjustments(layer) && !(confirm && confirm(layer->featureCount()))) {
      result.outcome = PlaceOutcome::Kept;
      result.message = QStringLiteral("기존 시굴격자를 그대로 두었습니다.");
      return result;
    }
    if (!replaceWithCells(layer, cells, authid, &error))
      return failed(error.isEmpty() ? QStringLiteral("새 시굴격자를 놓지 못했습니다. 기존 격자는 유지됩니다.") : error);
    result.outcome = PlaceOutcome::Placed;
    return result;
  }
  // First grid of the survey: no previous grid on the map to restore with Ctrl+Z.
  if (!TrenchGridGenerator::writeGpkg(surveyPath, QStringLiteral("trial_trench"), cells, authid, &error))
    return failed(QStringLiteral("격자를 조사 파일에 저장하지 못했습니다. 저장 위치와 파일 사용 상태를 확인하고 "
                                 "다시 적용하세요."),
                  error);
  QgsVectorLayer* layer = loadLayer ? loadLayer() : nullptr;
  if (!layer)
    return failed(QStringLiteral("격자는 파일에 저장했지만 지도에 불러오지 못했습니다. 조사를 다시 열어 확인하세요."));
  if (QgsVectorDataProvider* provider = layer->dataProvider()) provider->reloadData();
  layer->updateExtents();
  layer->triggerRepaint();
  rememberPlacedLayout(layer);
  result.layer = layer;
  result.outcome = PlaceOutcome::Placed;
  return result;
}

}  // namespace TrenchLayerEdit
