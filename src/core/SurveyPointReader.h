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
  int row = 0;  // -1 marks a vertex added from a breakline, not a surveyed point
  bool suspicious = false;
};

using SurveyPolyline = QVector<SurveyPoint>;

struct SurveyReadOptions {
  QString path;
  QString layer;
  int idColumn = -1;
  int xColumn = -1;
  int yColumn = -1;
  int zColumn = -1;
  bool swapAxes = false;
  // Work CRS of the map. Points are returned in this CRS.
  QString crsAuthId = QStringLiteral("EPSG:5186");
  // CRS the file was surveyed in. Empty means the file already uses crsAuthId.
  QString sourceCrsAuthId;
};

struct SurveySourceInfo {
  QString fatal;
  bool dxf = false;
  QStringList layers;
  QStringList fields;
};

struct SurveyReadReport {
  QVector<SurveyPoint> points;
  // DXF lines whose vertices carry heights; usable as breaklines.
  QVector<SurveyPolyline> breaklines;
  QStringList issues;
  QStringList fields;
  // DXF point layers left out because none of their points has a height.
  QStringList droppedLayers;
  int skipped = 0;
  int duplicateGroups = 0;
  int rawCount = 0;
  int idColumn = -1;
  int xColumn = -1;
  int yColumn = -1;
  int zColumn = -1;
  bool swapSuggested = false;
  // Why the reader suggests reading X as northing; empty when it does not.
  QString swapReason;
  // Whether points are in map order (x = east). False while still in file order.
  bool swapApplied = false;
  bool outsideKorea = false;
  int outsideCount = 0;
  int suspiciousCount = 0;
  int nonPointCount = 0;
  int flatLineCount = 0;
  int transformFailed = 0;
  int issuesOmitted = 0;
  QString sourceCrsAuthId;
  QString crsAuthId;
  int minCm = 0;
  int maxCm = 0;
  QString fatal;
};

// Reads DXF POINT Z and headered XLSX/XLS/CSV. The source file is opened read-only.
class SurveyPointReader {
public:
  // At most this many per-row messages are kept; the rest are only counted.
  static constexpr int kMaxIssues = 40;
  static SurveySourceInfo inspect(const QString& path);
  static SurveyReadReport read(const SurveyReadOptions& options);
};
