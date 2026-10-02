#pragma once

#include "SurveyPointReader.h"

#include <functional>

struct SurveyContourJob {
  SurveyReadOptions read;
  // Points the dialog already read and arranged; empty means read `read` again.
  QVector<SurveyPoint> points;
  // Optional 3D lines added as TIN nodes so the surface follows them.
  QVector<SurveyPolyline> breaklines;
  QString outputDir;
  QString groupTitle;
  int intervalCm = 50;
  int baseCm = 0;
  int indexEvery = 5;
  double cellSizeM = 0;
  // Optional: triangles with an edge longer than this stay empty. 0 keeps the whole hull.
  double maxEdgeM = 0;
  bool colorBands = true;
  int bandIntervalCm = 0;
  bool excludeSuspicious = false;
  QString clipWkt;
};

struct SurveyContourResult {
  bool ok = false;
  bool canceled = false;
  QString error;
  QString warning;
  int lineCount = 0;
  int linesBeforeClip = 0;
  int bandCount = 0;
  int pointCount = 0;
  int minCm = 0;
  int maxCm = 0;
  int bandIntervalCm = 0;
  double cellSizeM = 0;
  bool cellEnlarged = false;
  bool clipped = false;
  int maskedTriangles = 0;
  int breaklineNodes = 0;
};

class SurveyContourBuilder {
public:
  static constexpr qint64 kMaxCells = 4000000;
  static QString rejection(const QVector<SurveyPoint>& points);
  static double autoCellSize(const QVector<SurveyPoint>& points);
  static int autoBandIntervalCm(int minCm, int maxCm);
  // Smallest cell (whole centimetres once enlarged) whose grid over spanX x spanY fits maxCells.
  static double fitCellSize(double cell, double spanX, double spanY, qint64 maxCells = kMaxCells);
  static SurveyContourResult build(const QVector<SurveyPoint>& points, const SurveyContourJob& job,
                                   const std::function<bool()>& canceled = {},
                                   const std::function<void(double)>& progress = {});
  // Moves contours.gpkg and surface.tif from fromDir into toDir. The previous files are kept
  // aside and put back if any move fails, so a failed swap never loses the last result.
  static bool installFiles(const QString& fromDir, const QString& toDir, QString* error);
};
