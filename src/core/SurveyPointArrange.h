#pragma once

#include "SurveyPointReader.h"

#include <qgsrectangle.h>

// Turns a file-order read into map order and the work CRS without reading the file again.
struct SurveyArrangeOptions {
  bool swapAxes = false;    // read the first coordinate column (X) as northing
  QString sourceCrsAuthId;  // CRS of the file; empty means it already uses the work CRS
  QString targetCrsAuthId;  // work CRS of the map
};

namespace SurveyPointArrange {

// fileOrder must be a file-order read: SurveyPointReader::read with swapAxes=false and no
// sourceCrsAuthId. Heights are never changed; points that cannot be transformed are dropped.
SurveyReadReport arrange(const SurveyReadReport& fileOrder, const SurveyArrangeOptions& options);

// Gap in map units between the point bounds and reference; 0 when they touch or reference is empty.
double gapToExtent(const QVector<SurveyPoint>& points, const QgsRectangle& reference);

// Plain notes for the contour dialog, most important first. reference is the survey area bounds.
QStringList notes(const SurveyReadReport& arranged, const QgsRectangle& reference, bool otherAxisFits);

}  // namespace SurveyPointArrange
