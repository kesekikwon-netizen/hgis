#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

// Map coordinates: x is easting, y is northing. Survey tables often store the opposite.
struct SurveyPoint {
  double x = 0;
  double y = 0;
  double z = 0;
  QString name;
  int row = 0;
  bool suspicious = false;
};

struct SurveyReadOptions {
  QString path;
  QString layer;
  int idColumn = -1;
  int xColumn = -1;
  int yColumn = -1;
  int zColumn = -1;
  bool swapAxes = false;
  QString crsAuthId = QStringLiteral("EPSG:5186");
};

struct SurveySourceInfo {
  QString fatal;
  bool dxf = false;
  QStringList layers;
  QStringList fields;
};

struct SurveyReadReport {
  QVector<SurveyPoint> points;
  QStringList issues;
  QStringList fields;
  int skipped = 0;
  int duplicateGroups = 0;
  int rawCount = 0;
  int idColumn = -1;
  int xColumn = -1;
  int yColumn = -1;
  int zColumn = -1;
  bool swapSuggested = false;
  bool outsideKorea = false;
  int minCm = 0;
  int maxCm = 0;
  QString fatal;
};

// Reads DXF POINT Z and headered XLSX/XLS/CSV. The source file is opened read-only.
class SurveyPointReader {
public:
  static SurveySourceInfo inspect(const QString& path);
  static SurveyReadReport read(const SurveyReadOptions& options);
};
