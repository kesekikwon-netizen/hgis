#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QUrl>

#include <qgsgeometry.h>

#include "HeritageFetchPlan.h"

class QgsCoordinateReferenceSystem;
class QgsProject;
class QgsRectangle;

// Which other 시/군 the survey's 5 km scope touches (evaluation F120).
//
// The intranet request unit stays one 시/군. This only lists neighbours so the one confirm
// dialog can offer them; the first 시/군 is still decided by a point inside the survey area.
// The cadastral path already finds every 시·군·구 of the same 5 km scope through
// VWorld LT_C_ADSIGG_INFO; this uses the same layer.
namespace HeritageNearbyRegions {

constexpr double kRadiusMeters = 5000.;

// Survey area buffered by 5 km, returned in EPSG:4326. Empty when the CRS is invalid or
// not metric (a degree buffer would not be 5 km) or the transform fails.
QgsGeometry scopeWgs84(const QgsGeometry& surveyArea, const QgsCoordinateReferenceSystem& crs,
                       double meters = kRadiusMeters);

// VWorld data API request for 시·군·구 boundaries inside the scope's bounding box.
QUrl districtsUrl(const QString& apiKey, const QgsRectangle& wgsBox);

// Parses the LT_C_ADSIGG_INFO GeoJSON answer (EPSG:4326). Keeps districts whose boundary
// touches the scope, folded to the request unit (경기도 수원시 장안구 → 경기도 수원시).
// ok=false when the answer is not a usable OK response. more=true when the server has
// further pages that were not read.
QList<HeritageCity> parseDistricts(const QByteArray& body, const QgsGeometry& scopeWgs,
                                   bool* ok = nullptr, bool* more = nullptr);

// Offline fallback: named polygons of loaded boundary layers that touch the scope.
QList<HeritageCity> fromBoundaryLayers(QgsProject* project, const QgsGeometry& scopeWgs);

// Adds a city once, keeping order.
void appendUnique(QList<HeritageCity>* list, const HeritageCity& city);

}  // namespace HeritageNearbyRegions
