#include "KaSessionLog.h"
#include "KaLogExcept.h"
#include "MeasureOps.h"

#include <cmath>
#include <exception>
#include <limits>

#include <qgis.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransformcontext.h>
#include <qgsdistancearea.h>
#include <qgsgeometry.h>
#include <qgspointxy.h>
#include <qgsproject.h>

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

bool pinPlanarEllipsoid(QgsProject* project) {
  if (!project) return false;
  const QString stored =
      project->readEntry(QStringLiteral("Measure"), QStringLiteral("/Ellipsoid"), Qgis::geoNone());
  if (stored == Qgis::geoNone() && project->ellipsoid() == Qgis::geoNone()) return false;
  const bool wasDirty = project->isDirty();
  project->setEllipsoid(Qgis::geoNone());
  if (!wasDirty) project->setDirty(false);
  return true;
}

double gridBearingDegrees(const QgsPointXY& from, const QgsPointXY& to) {
  const double dx = to.x() - from.x();
  const double dy = to.y() - from.y();
  if (std::abs(dx) < 1e-12 && std::abs(dy) < 1e-12)
    return std::numeric_limits<double>::quiet_NaN();
  double deg = std::atan2(dx, dy) * 180.0 / 3.14159265358979323846;
  if (deg < 0.0) deg += 360.0;
  if (deg >= 360.0) deg -= 360.0;
  return deg;
}

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
    KA_LOG_EXCEPT();
  }
  return fallbackLength(pts);
}

double geometryAreaSquareMeters(const QgsGeometry& geometry,
                                const QgsCoordinateReferenceSystem& crs,
                                const QgsCoordinateTransformContext& ctx) {
  if (geometry.isNull() || geometry.isEmpty())
    return 0.0;
  QgsDistanceArea da = makeEngine(crs, ctx);
  try {
    const double v = da.measureArea(geometry);
    if (std::isfinite(v) && v >= 0.0)
      return v;
  } catch (const std::exception&) {
  } catch (...) {
    KaSessionLog::line(QStringLiteral("[except] core/MeasureOps.cpp:geometryArea"));
  }
  return std::abs(geometry.area());
}

double polygonAreaSquareMeters(const QVector<QgsPointXY>& pts,
                               const QgsCoordinateReferenceSystem& crs,
                               const QgsCoordinateTransformContext& ctx) {
  if (pts.size() < 3)
    return 0.0;
  return geometryAreaSquareMeters(polygonGeom(pts), crs, ctx);
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
    KA_LOG_EXCEPT();
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

QString formatBearing(double degrees) {
  if (!std::isfinite(degrees))
    return QStringLiteral("—");
  long long total = std::llround(degrees * 3600.0);
  total %= 360LL * 3600LL;
  if (total < 0) total += 360LL * 3600LL;
  const long long d = total / 3600;
  const long long m = (total % 3600) / 60;
  const long long sec = total % 60;
  return QStringLiteral("%1°%2′%3″")
      .arg(d)
      .arg(m, 2, 10, QLatin1Char('0'))
      .arg(sec, 2, 10, QLatin1Char('0'));
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
