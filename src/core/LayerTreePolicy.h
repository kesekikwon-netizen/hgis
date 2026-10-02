#pragma once

#include <QString>

class QgsMapLayer;
class QgsProject;

// Layer list rules that the map applies on top of the row order.
//
// The list is flat and additive (PO goal ORIG-3). The persisted role, not a parent
// heading, keeps survey data apart from reference maps; only heritage and survey
// contour imports add their own 「참조 지도」 bundles, and cadastral rows stay at the
// root. Drawing order follows the rows except for the user-requested fixed rules
// below; forcedOrderNote() explains them on the row so a drag that "does nothing"
// is not a mystery.
namespace LayerTreePolicy {

// Korean one-line note for rows whose map position is fixed; empty otherwise.
QString forcedOrderNote(const QgsMapLayer* layer, QgsProject* project);

}  // namespace LayerTreePolicy
