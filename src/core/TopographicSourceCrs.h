#pragma once
#include "TopographicCatalog.h"
#include <qgscoordinatetransformcontext.h>

namespace TopographicSourceCrs {
inline constexpr int kSolverVersion = 4;
struct Result {
  QString crsWkt;
  QString authId;
  QString evidence;
  QString warning;
  QString error;
  QList<int> unverifiedRecordIndexes;
};
// Call only with a verified official one-sheet download-to-metadata association.
// File names are not provenance. Bounds matching is not imagery alignment proof.
Result resolve(const QString& sheetNo, const QString& projection,
               const QList<TopographicCatalog::Record>& records,
               const QgsCoordinateTransformContext& context);
// Prefer verified official inventory bounds to the nominal sheet-number grid.
// Invalid explicit bounds fail; they never silently fall back to the grid.
Result resolve(const QString& sheetNo, const QString& projection,
               const QList<TopographicCatalog::Record>& records,
               const QgsCoordinateTransformContext& context, const QgsRectangle& officialBounds5179);
}
