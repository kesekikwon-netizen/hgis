#pragma once

#include "SurveyPointReader.h"

#include <vector>

// Breakline vertices spaced about `spacing` apart along each segment, as extra TIN nodes with
// row = -1. Nodes within 1 mm of an existing point are skipped. The total is capped so a CAD
// drawing full of contour polylines cannot stall the grid step.
QVector<SurveyPoint> densifySurveyBreaklines(const QVector<SurveyPolyline>& lines, double spacing,
                                             const QVector<SurveyPoint>& existing);

// Clears grid cells inside Delaunay triangles that have an edge longer than maxEdge, so the
// surface does not bridge trenches, pits or a concave survey boundary. The grid is in
// GDALGridCreate order (first row at minY) with cell centres at min + (i + 0.5) * cell.
// Returns the number of triangles cleared, or -1 when GDAL cannot triangulate.
int maskLongSurveyTriangles(std::vector<float>& grid, int nx, int ny, double minX, double minY, double cell,
                            const std::vector<double>& xs, const std::vector<double>& ys, double maxEdge,
                            float noData);
