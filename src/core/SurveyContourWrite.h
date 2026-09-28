#pragma once

#include "SurveyContourBuilder.h"

SurveyContourResult writeSurveyContourFiles(const QVector<SurveyPoint>& points, const SurveyContourJob& job,
                                            int minCm, int maxCm, double minX, double maxX, double minY,
                                            double maxY, double cell, const std::function<bool()>& canceled,
                                            const std::function<void(double)>& progress);
