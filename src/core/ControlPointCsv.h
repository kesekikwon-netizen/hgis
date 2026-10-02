#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <qgspointxy.h>

// Reading of control-point CSV files (기준점 CSV) for LayerOps::previewControlPointsCsv
// and LayerOps::importControlPointsCsv. Korean and English headers, quoted cells and an
// elevation (Z/표고) column are read. Axis swap is never automatic: the preview only
// suggests it and the user picks.
namespace ControlPointCsv {

struct Table {
  QString encoding;
  QChar delimiter = QLatin1Char(',');
  bool headerFound = false;       // false: point_id, x, y, [z when numeric], datum, ...
  bool headerGeographic = false;  // lon/lat (경도/위도) header
  QStringList headers;            // normalized: point_id, x, y, z, datum, ellipsoid, ...
  QList<QStringList> rows;        // data rows, cells unquoted and trimmed
  int shortRows = 0;              // lines with fewer than two cells (never imported)
  int idColumn = -1;
  int xColumn = -1;
  int yColumn = -1;
  int zColumn = -1;
};

// "측점" -> point_id, "X(m)" -> x, "표고" -> z, "경도" -> x, "위도" -> y ...
QString normalizeHeader(const QString& raw);
// The most frequent of ',', ';', tab outside quotes; ' ' for space-separated lines.
QChar detectDelimiter(const QString& line);
// Splits one line; "a, b" in double quotes stays one cell, "" is a literal quote.
QStringList splitLine(const QString& line, QChar delimiter);
bool parseText(const QString& text, Table* table, QString* errorOut = nullptr);
bool load(const QString& path, Table* table, QString* errorOut = nullptr);
QString cell(const QStringList& row, int index);

// Points already in the layer by name, to skip a re-imported point (same name, same
// position within toleranceM) and to report a name used at another position.
class DuplicateIndex {
public:
  explicit DuplicateIndex(double toleranceM = 0.01) : m_tolerance(toleranceM) {}
  void add(const QString& id, const QgsPointXY& point);
  bool isSamePoint(const QString& id, const QgsPointXY& point) const;
  bool hasName(const QString& id) const { return !id.isEmpty() && m_points.contains(id); }

private:
  double m_tolerance;
  QHash<QString, QList<QgsPointXY>> m_points;
};

}  // namespace ControlPointCsv
