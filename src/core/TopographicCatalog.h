#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <qgsrectangle.h>
#include <qgscoordinatetransformcontext.h>
#include <functional>

class QgsFeedback;

// No layer/project/QObject ownership: these values can cross a QgsTask boundary.
namespace TopographicCatalog {
enum class Category { Unknown, Contour, Road, Building, Water, Vegetation, Boundary, Facility,
                      ElevationPoint, PlaceName };
struct Record {
  QString source;
  QString layerName;
  // OGR DXF exposes one entities layer; these identify a CAD layer/geometry subset.
  QString cadLayer;
  QString geometryType;
  QString displayName;
  QString sourceSheet;
  QString crsWkt;
  QgsRectangle extent;
  bool hasExtent = false;
  Category category = Category::Unknown;
  QString signature;
};
struct ScanResult {
  QList<Record> records;
  QStringList warnings;
  QString error;
  bool canceled = false;
  int filesRead = 0;
  int cacheHits = 0;
};
struct QueryResult {
  QList<Record> matches;
  QList<Record> unknownCrs;
  QStringList warnings;
  QString error;
  bool canceled = false;
};

// Recursively reads local vector files. An empty cacheDirectory uses the app cache.
// Sources are never edited. Cancellation returns no partial records and does not
// replace the cache; callers must retain their previous catalog on error/cancel.
ScanResult scan(const QString& folder, QgsFeedback* feedback = nullptr,
                const QString& cacheDirectory = QString(),
                const std::function<bool()>& isCanceled = {});
QueryResult query(const QList<Record>& records, const QgsRectangle& bounds,
                  const QString& boundsCrs, const QgsCoordinateTransformContext& context,
                  QgsFeedback* feedback = nullptr,
                  const std::function<bool()>& isCanceled = {},
                  double scale = 0);
// 1:25,000 sheet scale. Coverage still uses this to recognize a survey view.
// Category visibility is the archaeology location-map set, not this number.
constexpr double kDetailMaxScale = 25000.;
constexpr double kCoverageRadiusMeters = 5000.;
bool visibleAtScale(Category category, double scale);
// Work-CRS meters. Grows a tight survey view to the same 5 km load radius
// so received neighbor sheets stay on the map; a larger canvas wins as-is.
QgsRectangle coverageBounds(const QgsRectangle& view, double radiusMeters = kCoverageRadiusMeters);
QString categoryName(Category category);
}
