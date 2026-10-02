#pragma once

#include "SurveyContourBuilder.h"

class QgsProject;

class SurveyContourStyle {
public:
  // Removes a previous run of the same group, then adds reference layers under 참조 지도.
  static bool apply(QgsProject* project, const QString& gpkgPath, const QString& groupTitle,
                    const SurveyContourResult& built, QString* error);
  static void removeGroup(QgsProject* project, const QString& groupTitle);
  // Reads what apply() needs back from a finished contours.gpkg. False when there is none.
  static bool readResult(const QString& gpkgPath, SurveyContourResult* out);
  // Shows a result already on disk again, e.g. after a replacement failed and was rolled back.
  static bool reapply(QgsProject* project, const QString& gpkgPath, const QString& groupTitle);
};
