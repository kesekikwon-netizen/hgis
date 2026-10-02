#include "PolygonPieces.h"

#include <cmath>
#include <limits>

#include <QVector>

namespace PolygonPieces {
namespace {

// Rings of every part as plain XY. Curved or non-polygon geometry yields nothing, so
// callers fall back to whole-shape behaviour.
QgsMultiPolygonXY partsOf(const QgsGeometry& geometry) {
  if (geometry.isNull() || geometry.type() != Qgis::GeometryType::Polygon) return {};
  if (geometry.isMultipart()) return geometry.asMultiPolygon();
  const QgsPolygonXY single = geometry.asPolygon();
  return single.isEmpty() ? QgsMultiPolygonXY() : QgsMultiPolygonXY{single};
}

QgsGeometry filled(const QgsPolylineXY& ring) {
  return QgsGeometry::fromPolygonXY(QgsPolygonXY{ring});
}

bool hasPiece(const QgsMultiPolygonXY& parts, const Piece& piece) {
  return piece.isValid() && piece.part < parts.size() && piece.ring < parts.at(piece.part).size() &&
         parts.at(piece.part).at(piece.ring).size() >= 4;
}

// A part counts as inner when a larger part's outline surrounds it (holes ignored), which
// covers both a part drawn on top of another and an island standing in a hole.
bool liesInAnotherPart(const QgsMultiPolygonXY& parts, int index) {
  if (parts.at(index).isEmpty()) return false;
  const QgsGeometry shell = filled(parts.at(index).at(0));
  const QgsGeometry probe = shell.pointOnSurface();
  if (probe.isNull()) return false;
  const double area = std::abs(shell.area());
  for (int other = 0; other < parts.size(); ++other) {
    if (other == index || parts.at(other).isEmpty()) continue;
    const QgsGeometry around = filled(parts.at(other).at(0));
    if (std::abs(around.area()) > area && around.contains(probe)) return true;
  }
  return false;
}

}  // namespace

std::optional<Piece> innerPieceAt(const QgsGeometry& geometry, const QgsPointXY& layerPoint, double tolerance) {
  const QgsMultiPolygonXY parts = partsOf(geometry);
  const QgsGeometry probe = QgsGeometry::fromPointXY(layerPoint);
  std::optional<Piece> best;
  double bestArea = std::numeric_limits<double>::max();
  const auto consider = [&](const QgsPolylineXY& ring, int part, int ringIndex) {
    if (ring.size() < 4) return;
    const QgsGeometry shape = filled(ring);
    const double distance = shape.distance(probe);  // 0 inside; negative when GEOS gives up
    if (distance < 0 || distance > tolerance) return;
    const double area = std::abs(shape.area());
    if (area < bestArea) {
      bestArea = area;
      best = Piece{part, ringIndex};
    }
  };
  for (int part = 0; part < parts.size(); ++part) {
    const QgsPolygonXY& rings = parts.at(part);
    for (int ring = 1; ring < rings.size(); ++ring) consider(rings.at(ring), part, ring);
    if (parts.size() > 1 && liesInAnotherPart(parts, part)) consider(rings.at(0), part, 0);
  }
  return best;
}

QgsGeometry outline(const QgsGeometry& geometry, const Piece& piece) {
  const QgsMultiPolygonXY parts = partsOf(geometry);
  if (!hasPiece(parts, piece)) return {};
  return filled(parts.at(piece.part).at(piece.ring));
}

QgsGeometry withoutPiece(const QgsGeometry& geometry, const Piece& piece) {
  const QgsMultiPolygonXY parts = partsOf(geometry);
  if (!hasPiece(parts, piece)) return {};
  QgsGeometry out = geometry;
  if (!piece.isHole()) {
    if (parts.size() < 2) return {};  // the only part is the shape itself, not a piece of it
    return out.deletePart(piece.part) ? out : QgsGeometry();
  }
  // Islands standing in the hole go with it: left behind they would lie on top of the
  // filled area as hidden duplicate parts.
  const QgsGeometry hole = filled(parts.at(piece.part).at(piece.ring));
  const double holeArea = std::abs(hole.area());
  QVector<int> islands;
  for (int part = 0; part < parts.size(); ++part) {
    if (part == piece.part || parts.at(part).isEmpty()) continue;
    const QgsGeometry shell = filled(parts.at(part).at(0));
    const QgsGeometry probe = shell.pointOnSurface();
    if (!probe.isNull() && std::abs(shell.area()) < holeArea && hole.contains(probe)) islands.append(part);
  }
  if (!out.deleteRing(piece.ring, piece.part)) return {};
  for (auto it = islands.crbegin(); it != islands.crend(); ++it)  // highest index first
    if (!out.deletePart(*it)) return {};
  return out;
}

QgsGeometry withPieceCutOut(const QgsGeometry& geometry, const Piece& piece) {
  if (piece.isHole()) return {};
  const QgsGeometry cutter = outline(geometry, piece);
  const QgsGeometry rest = withoutPiece(geometry, piece);
  if (cutter.isNull() || rest.isNull()) return {};
  QgsGeometry cut = rest.difference(cutter);
  if (cut.isNull() || cut.isEmpty() || cut.type() != Qgis::GeometryType::Polygon) return {};
  // The layer stores one geometry type; keep a multi-part shape multi-part.
  if (geometry.isMultipart() && !cut.isMultipart() && !cut.convertToMultiType()) return {};
  return cut;
}

}  // namespace PolygonPieces
