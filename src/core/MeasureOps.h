#pragma once

#include <QString>
#include <QVector>

class QgsCoordinateReferenceSystem;
class QgsCoordinateTransformContext;
class QgsGeometry;
class QgsPointXY;

// Field tape math for the map canvas (work CRS 5186/5187).
// Planimetric meters — same plane as the scale bar. Export EPSG:5179 is not used.
// Labels (area($geometry)), tape, select, and layer area summary all use this plane.
namespace MeasureOps {

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

QString formatLengthM(double meters);
QString formatAreaM2(double squareMeters);

}  // namespace MeasureOps
