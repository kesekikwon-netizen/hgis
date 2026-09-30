#pragma once

class QgsProject;
class QgsVectorLayer;

// What the edit buffers hold, for the status bar's 「저장 안 됨 n건」. Counted per feature
// id, so an attribute edit plus a geometry edit on the same feature is one, never two.
// The project-level dirty flag rides along for changes with no feature behind them (a
// layer added, a style changed). Read only: nothing here starts or stops editing.
namespace EditBufferSummary {

struct Summary {
  int features = 0;           // unique edited feature ids over every editing vector layer
  int layers = 0;             // vector layers with at least one buffered edit
  bool projectDirty = false;  // QgsProject::isDirty()
  bool unsaved() const { return features > 0 || projectDirty; }
};

// Unique ids in one layer's buffer: added, geometry changed, attributes changed, deleted.
// 0 for a null layer or one that is not editing.
int uniqueEditedIds(const QgsVectorLayer* layer);

// Sums uniqueEditedIds over the project's vector layers; a null project gives zeros.
Summary summarize(const QgsProject* project);

}  // namespace EditBufferSummary
