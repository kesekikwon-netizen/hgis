#pragma once

#include <QString>
#include <functional>
#include <vector>

#include <qgsfeatureid.h>

#include "TrenchGridGenerator.h"

class QgsProject;
class QgsVectorLayer;

// Changes to the trial_trench map layer. Each change is ONE undoable edit command in the
// layer's edit buffer (Ctrl+Z restores it); nothing is written to the survey file until
// the user saves (Ctrl+S / close prompt).
namespace TrenchLayerEdit {

// Custom layer property (saved with the project) holding fingerprints of the last grids the
// generator placed, newest first.
inline constexpr const char* kPlacedLayoutKey = "ka_hgis/trench_placed_layout";

// Fingerprint of the current layout: trench names and corners to the millimetre,
// unsaved edits included. Empty for a missing layer.
QString layoutSignature(QgsVectorLayer* layer);

// Records the current layout as "placed by the generator".
void rememberPlacedLayout(QgsVectorLayer* layer);

// Trenches differ from every recently placed grid (moved, deleted or renamed by hand).
// Trenches without any record of a placed grid (older surveys) count as adjusted.
bool hasHandAdjustments(QgsVectorLayer* layer);

// Unsaved edits other than the generator's own placements, i.e. hand edits that a
// re-generation would throw away.
bool hasUnsavedHandEdits(QgsVectorLayer* layer);

// Replaces every trench with cells as one command, so Ctrl+Z brings the previous grid
// back. authid is the CRS the cells were computed in; a layer in another CRS is refused
// and left unchanged.
bool replaceWithCells(QgsVectorLayer* layer, const std::vector<TrenchGridGenerator::Cell>& cells,
                      const QString& authid, QString* errorOut = nullptr);

// Moves the given trenches (every trench when fids is empty) by (dx, dy) as one command.
bool translate(QgsVectorLayer* layer, const QgsFeatureIds& fids, double dx, double dy,
               const QString& commandTitle, QString* errorOut = nullptr);

// Deletes one trench as one command.
bool deleteTrench(QgsVectorLayer* layer, QgsFeatureId fid, QString* errorOut = nullptr);

// Result of placing a generated grid (「구역에 깔기」, 조사구역 우클릭 시굴/표본, 맵에 찍기).
enum class PlaceOutcome { Placed, Kept, Failed };
struct PlaceResult {
  PlaceOutcome outcome = PlaceOutcome::Failed;
  QgsVectorLayer* layer = nullptr;  // the trench layer (Placed, and Kept when one exists)
  QString message;                  // user-facing reason when Kept or Failed
  QString detail;                   // technical detail for Failed; may be empty
};
using ConfirmReplace = std::function<bool(qint64 trenchCount)>;  // true = replace
using LoadLayer = std::function<QgsVectorLayer*()>;              // adds the written layer to the map

// Puts a generated grid on the map, keeping the "re-generation replaces the grid" rule.
// A trench layer already on the map is replaced by replaceWithCells (one Ctrl+Z step,
// written on 저장). A layer that already holds exactly these cells is Kept untouched (no
// edit, no unsaved mark). Unsaved hand edits block the replacement (save first, edits kept);
// a grid moved, deleted or renamed by hand is replaced only when confirm() agrees. Without a
// trench layer on the map the first grid is written to surveyPath (as before) and loadLayer()
// adds it.
PlaceResult placeGrid(QgsProject* project, const QString& surveyPath,
                      const std::vector<TrenchGridGenerator::Cell>& cells, const QString& authid,
                      const LoadLayer& loadLayer, const ConfirmReplace& confirm);

}  // namespace TrenchLayerEdit
