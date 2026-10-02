#include "EditGeometryRepair.h"

#include "MeasureOps.h"

#include <algorithm>
#include <cmath>

#include <qgsabstractgeometry.h>

namespace {

// Thinness 4πA/P² is 1 for a circle and tends to 0 for points on one line.
constexpr double kMinThinness = 1e-3;
constexpr double kMinArea = 1e-4;  // 1 cm² in a metric work CRS
constexpr double kPi = 3.14159265358979323846;

void collectPolygons(const QgsGeometry& geom, QVector<QgsGeometry>* out) {
  if (geom.isNull() || geom.isEmpty()) return;
  if (geom.type() == Qgis::GeometryType::Polygon) {
    if (geom.isMultipart()) {
      const QgsMultiPolygonXY parts = geom.asMultiPolygon();
      for (const QgsPolygonXY& poly : parts) out->append(QgsGeometry::fromPolygonXY(poly));
    } else {
      out->append(geom);
    }
    return;
  }
  if (geom.isMultipart()) {
    const QVector<QgsGeometry> members = geom.asGeometryCollection();
    for (const QgsGeometry& member : members) collectPolygons(member, out);
  }
}

}  // namespace

namespace EditGeometryRepair {

PolygonResult closePolygon(const QVector<QgsPointXY>& sketch, double minSpacing) {
  PolygonResult result;
  const double tol2 = minSpacing * minSpacing;
  QgsPolylineXY ring;
  for (const QgsPointXY& p : sketch) {
    if (ring.isEmpty() || ring.last().sqrDist(p) > tol2) ring.append(p);
  }
  if (ring.size() >= 3 && ring.first() != ring.last()) ring.append(ring.first());
  if (ring.size() < 4) return result;

  QgsGeometry geom = QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring);
  if (geom.isEmpty()) return result;
  if (!geom.isGeosValid()) {
    const QgsGeometry fixed = geom.makeValid();
    if (!fixed.isEmpty()) {
      geom = fixed;
      result.repaired = true;
    }
  }

  QVector<QgsGeometry> parts;
  collectPolygons(geom, &parts);
  int best = -1;
  for (int i = 0; i < parts.size(); ++i) {
    const double area = parts.at(i).area();
    result.partsArea += area;
    if (best < 0 || area > result.keptArea) {
      best = i;
      result.keptArea = area;
    }
  }
  if (best < 0) return result;
  result.geometry = parts.at(best);
  result.droppedParts = static_cast<int>(parts.size()) - 1;

  const double perimeter =
      result.geometry.constGet() ? result.geometry.constGet()->perimeter() : 0.0;
  const double thinness =
      perimeter > 0.0 ? 4.0 * kPi * result.keptArea / (perimeter * perimeter) : 0.0;
  result.degenerate = result.keptArea < kMinArea || thinness < kMinThinness;
  return result;
}

QString notice(const PolygonResult& result) {
  if (result.geometry.isEmpty()) return {};
  if (result.droppedParts > 0) {
    const double dropped = std::max(0.0, result.partsArea - result.keptArea);
    return QStringLiteral("선이 서로 꼬인 면이라 가장 큰 조각만 넣었습니다. 빠진 조각 %1개(%2)를 "
                          "도형선택으로 확인하세요.")
        .arg(result.droppedParts)
        .arg(MeasureOps::formatAreaM2(dropped));
  }
  if (result.degenerate) {
    return QStringLiteral("점이 거의 한 줄이라 면적이 %1뿐입니다. 제출 검수에서 걸리니 다시 그리세요.")
        .arg(MeasureOps::formatAreaM2(result.keptArea));
  }
  return {};
}

}  // namespace EditGeometryRepair
