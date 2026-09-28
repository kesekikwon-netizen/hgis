#pragma once

#include "SurveyContourBuilder.h"

class QgsProject;

class SurveyContourStyle {
public:
  // Removes a previous run of the same group, then adds reference layers under 참조 지도.
  static bool apply(QgsProject* project, const QString& gpkgPath, const QString& groupTitle,
                    const SurveyContourResult& built, QString* error);
  static void removeGroup(QgsProject* project, const QString& groupTitle);
};
