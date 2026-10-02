#include "SurveyContourTin.h"

#include <QPair>
#include <QSet>

#include <algorithm>
#include <cmath>

#include <gdal_alg.h>

namespace {

constexpr int kMaxBreaklineNodes = 200000;

QPair<qint64, qint64> millimetres(double x, double y) {
  return {qRound64(x * 1000.0), qRound64(y * 1000.0)};
}

}  // namespace

QVector<SurveyPoint> densifySurveyBreaklines(const QVector<SurveyPolyline>& lines, double spacing,
                                             const QVector<SurveyPoint>& existing) {
  QVector<SurveyPoint> added;
  if (lines.isEmpty()) return added;
  double total = 0;
  for (const SurveyPolyline& line : lines)
    for (int i = 1; i < line.size(); ++i)
      total += std::hypot(line[i].x - line[i - 1].x, line[i].y - line[i - 1].y);
  const double step = std::max({spacing > 0 ? spacing : 0.5, total / kMaxBreaklineNodes, 1e-3});
  QSet<QPair<qint64, qint64>> taken;
  for (const SurveyPoint& point : existing) taken.insert(millimetres(point.x, point.y));
  auto add = [&](double x, double y, double z) {
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return;
    const auto key = millimetres(x, y);
    if (taken.contains(key)) return;
    taken.insert(key);
    SurveyPoint node;
    node.x = x;
    node.y = y;
    node.z = z;
    node.row = -1;
    added.push_back(node);
  };
  for (const SurveyPolyline& line : lines) {
    for (int i = 0; i < line.size(); ++i) {
      const SurveyPoint& a = line[i];
      add(a.x, a.y, a.z);
      if (i + 1 >= line.size()) break;
      const SurveyPoint& b = line[i + 1];
      const int pieces = static_cast<int>(std::floor(std::hypot(b.x - a.x, b.y - a.y) / step));
      for (int k = 1; k < pieces; ++k) {
        const double t = static_cast<double>(k) / pieces;
        add(a.x + t * (b.x - a.x), a.y + t * (b.y - a.y), a.z + t * (b.z - a.z));
      }
    }
  }
  return added;
}

int maskLongSurveyTriangles(std::vector<float>& grid, int nx, int ny, double minX, double minY, double cell,
                            const std::vector<double>& xs, const std::vector<double>& ys, double maxEdge,
                            float noData) {
  if (!(maxEdge > 0) || !(cell > 0) || xs.size() < 3 || xs.size() != ys.size() || nx < 1 || ny < 1) return 0;
  if (grid.size() != static_cast<size_t>(nx) * static_cast<size_t>(ny)) return 0;
  if (!GDALHasTriangulation()) return -1;
  GDALTriangulation* tin = GDALTriangulationCreateDelaunay(static_cast<int>(xs.size()), xs.data(), ys.data());
  if (!tin) return -1;
  const double limit = maxEdge * maxEdge;
  int cleared = 0;
  for (int f = 0; f < tin->nFacets; ++f) {
    const int* v = tin->pasFacets[f].anVertexIdx;
    const double ax = xs[v[0]], ay = ys[v[0]], bx = xs[v[1]], by = ys[v[1]], cx = xs[v[2]], cy = ys[v[2]];
    const double longest = std::max({(ax - bx) * (ax - bx) + (ay - by) * (ay - by),
                                     (bx - cx) * (bx - cx) + (by - cy) * (by - cy),
                                     (cx - ax) * (cx - ax) + (cy - ay) * (cy - ay)});
    const double det = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy);
    if (longest <= limit || std::abs(det) < 1e-12) continue;
    ++cleared;
    const int c0 = std::max(0, static_cast<int>(std::floor((std::min({ax, bx, cx}) - minX) / cell - 0.5)));
    const int c1 = std::min(nx - 1, static_cast<int>(std::ceil((std::max({ax, bx, cx}) - minX) / cell - 0.5)));
    const int r0 = std::max(0, static_cast<int>(std::floor((std::min({ay, by, cy}) - minY) / cell - 0.5)));
    const int r1 = std::min(ny - 1, static_cast<int>(std::ceil((std::max({ay, by, cy}) - minY) / cell - 0.5)));
    for (int r = r0; r <= r1; ++r) {
      const double y = minY + (r + 0.5) * cell;
      for (int c = c0; c <= c1; ++c) {
        const double x = minX + (c + 0.5) * cell;
        const double l1 = ((by - cy) * (x - cx) + (cx - bx) * (y - cy)) / det;
        const double l2 = ((cy - ay) * (x - cx) + (ax - cx) * (y - cy)) / det;
        if (l1 >= 0.0 && l2 >= 0.0 && l1 + l2 <= 1.0)
          grid[static_cast<size_t>(r) * static_cast<size_t>(nx) + static_cast<size_t>(c)] = noData;
      }
    }
  }
  GDALTriangulationFree(tin);
  return cleared;
}
