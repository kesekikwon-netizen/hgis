#pragma once

#include <QString>
#include <QVector>

#include <qgsgeometry.h>
#include <qgspointxy.h>

// Closes a sketched ring into the single polygon the digitizing layers store.
// A self-intersecting sketch (a bow-tie) splits into several parts under makeValid();
// only the largest part is kept, so the caller must be able to tell the user, after the
// shape is finished, what was left out or that the shape has almost no area.
namespace EditGeometryRepair {

struct PolygonResult {
  QgsGeometry geometry;     // single-part polygon; empty when nothing usable remains
  bool repaired = false;    // makeValid() had to change the sketch
  int droppedParts = 0;     // polygon parts that were not kept
  double partsArea = 0.0;   // area of every polygon part after repair (map units²)
  double keptArea = 0.0;    // area of the kept part
  bool degenerate = false;  // points almost on one line: close to zero area
};

// ring: sketch vertices in map units (not closed). minSpacing drops a vertex that sits
// on top of the previous one (double clicks, snapping to the same point twice).
PolygonResult closePolygon(const QVector<QgsPointXY>& ring, double minSpacing);

// Short Korean note for the status line after drawing; empty when the sketch was kept
// as drawn.
QString notice(const PolygonResult& result);

}  // namespace EditGeometryRepair
