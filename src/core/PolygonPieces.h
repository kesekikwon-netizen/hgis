#pragma once

#include <optional>

#include <qgsgeometry.h>
#include <qgspointxy.h>

// Inner pieces of one polygon feature: a hole (interior ring), or a part that lies inside
// another part of the same feature. Imported survey areas often carry their inner outlines
// this way, where they are not shapes of their own and cannot be picked or deleted as one.
namespace PolygonPieces {

struct Piece {
  int part = -1;
  int ring = -1;  // 0 = the part's own outline, 1.. = a hole of that part
  [[nodiscard]] bool isValid() const { return part >= 0 && ring >= 0; }
  [[nodiscard]] bool isHole() const { return ring > 0; }
  friend bool operator==(const Piece&, const Piece&) = default;
};

// The smallest inner piece under the point (layer coordinates), or none on the plain
// outer area. `tolerance` lets a click on the inner outline itself count.
[[nodiscard]] std::optional<Piece> innerPieceAt(const QgsGeometry& geometry, const QgsPointXY& layerPoint,
                                                double tolerance);

// The piece as a filled polygon of its own (highlight, area). Null for a stale piece.
[[nodiscard]] QgsGeometry outline(const QgsGeometry& geometry, const Piece& piece);

// The geometry without the piece: a hole is filled (islands standing in it go too), an inner
// part is dropped. Null when the piece is stale or is the only part.
[[nodiscard]] QgsGeometry withoutPiece(const QgsGeometry& geometry, const Piece& piece);

// The geometry with an inner part cut out of the parts around it, leaving a hole.
// Null for a hole piece (already cut out) or when the cut fails.
[[nodiscard]] QgsGeometry withPieceCutOut(const QgsGeometry& geometry, const Piece& piece);

}  // namespace PolygonPieces
