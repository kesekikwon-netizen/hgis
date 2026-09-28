#pragma once

#include "SurveyPointReader.h"

#include <functional>

struct SurveyContourJob {
  SurveyReadOptions read;
  QString outputDir;
  QString groupTitle;
  int intervalCm = 50;
  int baseCm = 0;
  int indexEvery = 5;
  double cellSizeM = 0;
  bool colorBands = true;
  int bandIntervalCm = 0;
  bool idw = false;
  bool excludeSuspicious = false;
  QString clipWkt;
};

struct SurveyContourResult {
  bool ok = false;
  bool canceled = false;
  QString error;
  QString warning;
  int lineCount = 0;
  int bandCount = 0;
  int pointCount = 0;
  int minCm = 0;
  int maxCm = 0;
  int bandIntervalCm = 0;
  double cellSizeM = 0;
};

class SurveyContourBuilder {
public:
  static QString rejection(const QVector<SurveyPoint>& points);
  static double autoCellSize(const QVector<SurveyPoint>& points);
  static int autoBandIntervalCm(int minCm, int maxCm);
  static SurveyContourResult build(const QVector<SurveyPoint>& points, const SurveyContourJob& job,
                                   const std::function<bool()>& canceled = {},
                                   const std::function<void(double)>& progress = {});
  static bool installFiles(const QString& fromDir, const QString& toDir, QString* error);
};
