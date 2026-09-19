#include "KaSessionLog.h"
#include "MeasureOps.h"

#include <cmath>
#include <exception>

#include <qgis.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransformcontext.h>
#include <qgsdistancearea.h>
#include <qgsgeometry.h>
#include <qgspointxy.h>

namespace {

QgsDistanceArea makeEngine(const QgsCoordinateReferenceSystem& crs,
                           const QgsCoordinateTransformContext& ctx) {
  QgsDistanceArea da;
  if (crs.isValid())
    da.setSourceCrs(crs, ctx);
  // Site-scale tape on TM work CRS: Cartesian plane (matches 축척자 / 지적).
  da.setEllipsoid(Qgis::geoNone());
  return da;
}

double fallbackLength(const QVector<QgsPointXY>& pts) {
  double s = 0.0;
  for (int i = 1; i < pts.size(); ++i)
    s += pts[i - 1].distance(pts[i]);
  return s;
}

QgsGeometry lineGeom(const QVector<QgsPointXY>& pts) {
  if (pts.size() < 2)
    return QgsGeometry();
  QgsPolylineXY line;
  line.reserve(pts.size());
  for (const QgsPointXY& p : pts)
    line.append(p);
  return QgsGeometry::fromPolylineXY(line);
}

QgsGeometry polygonGeom(const QVector<QgsPointXY>& pts) {
  if (pts.size() < 3)
    return QgsGeometry();
  QgsPolylineXY ring;
  ring.reserve(pts.size() + 1);
  for (const QgsPointXY& p : pts)
    ring.append(p);
  if (ring.first().sqrDist(ring.last()) > 1e-12)
    ring.append(ring.first());
  return QgsGeometry::fromPolygonXY({ring});
}

}  // namespace

namespace MeasureOps {

double lineLengthMeters(const QVector<QgsPointXY>& pts,
                        const QgsCoordinateReferenceSystem& crs,
                        const QgsCoordinateTransformContext& ctx) {
  if (pts.size() < 2)
    return 0.0;
  QgsDistanceArea da = makeEngine(crs, ctx);
  const QgsGeometry g = lineGeom(pts);
  if (g.isNull() || g.isEmpty())
    return fallbackLength(pts);
  try {
    const double v = da.measureLength(g);
    if (std::isfinite(v) && v >= 0.0)
      return v;
  } catch (const std::exception&) {
  } catch (...) {
    KaSessionLog::line(QStringLiteral("[except] core/MeasureOps.cpp:72"));
  }
  return fallbackLength(pts);
}

double polygonAreaSquareMeters(const QVector<QgsPointXY>& pts,
                               const QgsCoordinateReferenceSystem& crs,
                               const QgsCoordinateTransformContext& ctx) {
  if (pts.size() < 3)
    return 0.0;
  QgsDistanceArea da = makeEngine(crs, ctx);
  const QgsGeometry g = polygonGeom(pts);
  if (g.isNull() || g.isEmpty())
    return 0.0;
  try {
    const double v = da.measureArea(g);
    if (std::isfinite(v) && v >= 0.0)
      return v;
  } catch (const std::exception&) {
  } catch (...) {
    KaSessionLog::line(QStringLiteral("[except] core/MeasureOps.cpp:91"));
  }
  return std::abs(g.area());
}

double polygonPerimeterMeters(const QVector<QgsPointXY>& pts,
                              const QgsCoordinateReferenceSystem& crs,
                              const QgsCoordinateTransformContext& ctx) {
  if (pts.size() < 2)
    return lineLengthMeters(pts, crs, ctx);
  QgsDistanceArea da = makeEngine(crs, ctx);
  const QgsGeometry g = polygonGeom(pts);
  if (g.isNull() || g.isEmpty())
    return lineLengthMeters(pts, crs, ctx);
  try {
    const double v = da.measurePerimeter(g);
    if (std::isfinite(v) && v >= 0.0)
      return v;
  } catch (const std::exception&) {
  } catch (...) {
    KaSessionLog::line(QStringLiteral("[except] core/MeasureOps.cpp:110"));
  }
  return lineLengthMeters(pts, crs, ctx);
}

QVector<double> segmentLengthsMeters(const QVector<QgsPointXY>& pts,
                                     const QgsCoordinateReferenceSystem& crs,
                                     const QgsCoordinateTransformContext& ctx) {
  QVector<double> out;
  if (pts.size() < 2)
    return out;
  out.reserve(pts.size() - 1);
  for (int i = 1; i < pts.size(); ++i) {
    QVector<QgsPointXY> pair{pts[i - 1], pts[i]};
    out.append(lineLengthMeters(pair, crs, ctx));
  }
  return out;
}

QString formatLengthM(double meters) {
  if (!std::isfinite(meters) || meters < 0.0)
    return QStringLiteral("—");
  if (meters >= 1000.0)
    return QStringLiteral("%1 km").arg(meters / 1000.0, 0, 'f', 3);
  if (meters >= 10.0)
    return QStringLiteral("%1 m").arg(meters, 0, 'f', 2);
  return QStringLiteral("%1 m").arg(meters, 0, 'f', 3);
}

QString formatAreaM2(double squareMeters) {
  if (!std::isfinite(squareMeters) || squareMeters < 0.0)
    return QStringLiteral("—");
  if (squareMeters >= 10000.0)
    return QStringLiteral("%1 ha (%2 ㎡)")
        .arg(squareMeters / 10000.0, 0, 'f', 3)
        .arg(squareMeters, 0, 'f', 1);
  if (squareMeters >= 100.0)
    return QStringLiteral("%1 ㎡").arg(squareMeters, 0, 'f', 1);
  return QStringLiteral("%1 ㎡").arg(squareMeters, 0, 'f', 2);
}

}  // namespace MeasureOps
