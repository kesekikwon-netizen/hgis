#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <qgsfeatureid.h>

class QgsVectorLayer;

// Geometry edits that change several features at once (split out of LayerOps).
// LayerOps::mergePolygonFeatures / applyVertexMove keep their public names and
// are defined in GeometryEditMerge.cpp / GeometryEditVertex.cpp.
namespace GeometryEditOps {

// One attribute whose values differ among the polygons about to be merged.
struct MergeConflict {
  QString field;        // alias when set, else the field name
  QStringList values;   // distinct non-empty values, in drawing order
};

// The feature whose attributes the merged polygon keeps: the polygon drawn first
// (saved features by id, then unsaved ones in the order they were added).
// FID_NULL when ids is empty.
QgsFeatureId mergeKeeperId(const QgsFeatureIds& ids);

// Attributes that differ between the given features (primary keys excluded).
// Empty when every feature carries the same values, so no question is needed.
QList<MergeConflict> mergeConflicts(const QgsVectorLayer* layer, const QgsFeatureIds& ids);

// Korean confirmation text for the conflicts, listing at most maxFields fields.
QString mergeConflictSummary(const QList<MergeConflict>& conflicts, int maxFields = 6);

}  // namespace GeometryEditOps
