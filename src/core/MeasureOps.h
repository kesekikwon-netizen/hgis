#pragma once

#include <QString>
#include <QVector>

class QgsCoordinateReferenceSystem;
class QgsCoordinateTransformContext;
class QgsGeometry;
class QgsPointXY;
class QgsProject;

// Field tape math for the map canvas (work CRS 5186/5187).
// Planimetric meters — same plane as the scale bar. Export EPSG:5179 is not used.
// Labels (area($geometry)), tape, select, and layer area summary all use this plane;
// labels follow the project ellipsoid, so pinPlanarEllipsoid keeps it at NONE.
namespace MeasureOps {

// Sets the project ellipsoid to NONE (planar) so area($geometry) labels match the tape.
// Keeps the project's unsaved-changes flag as it was. Returns true when it changed.
bool pinPlanarEllipsoid(QgsProject* project);

double lineLengthMeters(const QVector<QgsPointXY>& pts,
                        const QgsCoordinateReferenceSystem& crs,
                        const QgsCoordinateTransformContext& ctx);

double polygonAreaSquareMeters(const QVector<QgsPointXY>& pts,
                               const QgsCoordinateReferenceSystem& crs,
                               const QgsCoordinateTransformContext& ctx);

// Same planimetric area as polygonAreaSquareMeters / area($geometry).
double geometryAreaSquareMeters(const QgsGeometry& geometry,
                                const QgsCoordinateReferenceSystem& crs,
                                const QgsCoordinateTransformContext& ctx);

double polygonPerimeterMeters(const QVector<QgsPointXY>& pts,
                              const QgsCoordinateReferenceSystem& crs,
                              const QgsCoordinateTransformContext& ctx);

QVector<double> segmentLengthsMeters(const QVector<QgsPointXY>& pts,
                                     const QgsCoordinateReferenceSystem& crs,
                                     const QgsCoordinateTransformContext& ctx);

// Grid bearing (도북 방위각) from `from` to `to`: degrees clockwise from grid north,
// 0 <= value < 360. NaN when the points coincide.
double gridBearingDegrees(const QgsPointXY& from, const QgsPointXY& to);

QString formatLengthM(double meters);
QString formatAreaM2(double squareMeters);
// 도·분·초, e.g. 123°27′15″. "—" for NaN.
QString formatBearing(double degrees);

}  // namespace MeasureOps
