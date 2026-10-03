#include "LayoutBadgePlacer.h"

#include <QLineF>
#include <QList>
#include <QRectF>

#include <qgslayout.h>
#include <qgslayoutitemlegend.h>
#include <qgslayoutitemmap.h>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace {
constexpr double kPi = 3.14159265358979323846;
// Centres closer than this share of a step cover each other.
constexpr double kClearShare = 0.96;
// Keep the circle outline (0.15 mm) and a hair of paper inside the frame.
constexpr double kEdgeMarginMm = 0.3;
}  // namespace

LayoutBadgePlacer::LayoutBadgePlacer(const QTransform& mapToScene, const QPainterPath& allowedScene,
                                     double stepMapUnits)
    : m_toScene(mapToScene),
      m_allowed(allowedScene),
      m_limited(!allowedScene.isEmpty()),
      m_step(std::isfinite(stepMapUnits) && stepMapUnits > 0.0 ? stepMapUnits : 1.0) {
  if (!m_limited) return;
  const double stepMm = QLineF(m_toScene.map(QPointF(0.0, 0.0)), m_toScene.map(QPointF(m_step, 0.0))).length();
  const QRectF box = m_allowed.boundingRect();
  const double reachMm = std::hypot(box.width(), box.height());
  if (stepMm > 1e-9 && std::isfinite(reachMm))
    m_reachRings = std::clamp(static_cast<int>(std::ceil(reachMm / stepMm)) + 1, 1, kMaxRings);
}

int LayoutBadgePlacer::slotsOnRing(int ring) {
  const double half = std::min(0.999, 1.0 / (2.0 * double(std::max(1, ring))));
  const double count = kPi / std::asin(half);
  return std::max(4, static_cast<int>(std::floor(count + 1e-6)));
}

quint64 LayoutBadgePlacer::cellKey(qint64 cx, qint64 cy) const {
  return (quint64(quint32(qint32(cx))) << 32) | quint64(quint32(qint32(cy)));
}

bool LayoutBadgePlacer::isFree(const QgsPointXY& point) const {
  const qint64 cx = qint64(std::floor(point.x() / m_step));
  const qint64 cy = qint64(std::floor(point.y() / m_step));
  const double clear = m_step * kClearShare;
  for (qint64 dx = -1; dx <= 1; ++dx) {
    for (qint64 dy = -1; dy <= 1; ++dy) {
      const auto cell = m_cells.constFind(cellKey(cx + dx, cy + dy));
      if (cell == m_cells.cend()) continue;
      for (const QgsPointXY& other : *cell)
        if (point.distance(other) < clear) return false;
    }
  }
  return true;
}

bool LayoutBadgePlacer::fits(const QgsPointXY& point, double badgeDiameterMm) const {
  if (!m_limited) return true;
  const QPointF centre = m_toScene.map(QPointF(point.x(), point.y()));
  const double r = std::max(0.5, badgeDiameterMm * 0.5) + kEdgeMarginMm;
  return m_allowed.contains(QRectF(centre.x() - r, centre.y() - r, 2.0 * r, 2.0 * r));
}

void LayoutBadgePlacer::occupy(const QgsPointXY& point) {
  const qint64 cx = qint64(std::floor(point.x() / m_step));
  const qint64 cy = qint64(std::floor(point.y() / m_step));
  m_cells[cellKey(cx, cy)].append(point);
  ++m_count;
}

QgsPointXY LayoutBadgePlacer::place(const QgsPointXY& origin, double badgeDiameterMm) {
  QgsPointXY chosen = origin;
  bool found = false;
  bool haveFree = false;
  QgsPointXY firstFree;
  auto consider = [&](const QgsPointXY& candidate) {
    if (!isFree(candidate)) return false;
    if (fits(candidate, badgeDiameterMm)) {
      chosen = candidate;
      return true;
    }
    if (!haveFree) {
      haveFree = true;
      firstFree = candidate;
    }
    return false;
  };
  found = consider(origin);
  // Each placed badge blocks at most a few spots of any ring, while ring n
  // holds about 2*pi*n spots, so this many rings always has a free slot. On a
  // limited paper the search also reaches across it, so a badge whose site is
  // under a legend card wider than the base rings still gets clear of it.
  const int rings = std::min(kMaxRings, std::max(kBaseRings + m_count, m_reachRings));
  for (int ring = 1; !found && ring <= rings; ++ring) {
    const int count = slotsOnRing(ring);
    const double radius = m_step * ring;
    for (int slot = 0; slot < count && !found; ++slot) {
      const double angle = slot * (2.0 * kPi / double(count));
      found = consider(QgsPointXY(origin.x() + std::cos(angle) * radius,
                                  origin.y() + std::sin(angle) * radius));
    }
  }
  // No slot is both clear and fully on the paper: keep the circles apart
  // (overlap is never allowed) at the nearest clear slot.
  if (!found)
    chosen = haveFree ? firstFree : QgsPointXY(origin.x(), origin.y() + m_step * (rings + 1));
  occupy(chosen);
  return chosen;
}

QVector<QgsPointXY> LayoutBadgePlacer::placeAll(const QVector<QgsPointXY>& origins,
                                                const QVector<double>& badgeDiametersMm) {
  const int n = origins.size();
  QVector<QgsPointXY> placed(n);
  // Crowds: sites chained closer than the clear distance (union-find over step cells).
  QVector<int> root(n);
  std::iota(root.begin(), root.end(), 0);
  auto find = [&root](int i) {
    while (root[i] != i) i = root[i] = root[root[i]];
    return i;
  };
  QHash<quint64, QVector<int>> cells;
  for (int i = 0; i < n; ++i) {
    const qint64 cx = qint64(std::floor(origins.at(i).x() / m_step));
    const qint64 cy = qint64(std::floor(origins.at(i).y() / m_step));
    for (qint64 dx = -1; dx <= 1; ++dx) {
      for (qint64 dy = -1; dy <= 1; ++dy) {
        for (int j : cells.value(cellKey(cx + dx, cy + dy)))
          if (origins.at(i).distance(origins.at(j)) < m_step * kClearShare) root[find(i)] = find(j);
      }
    }
    cells[cellKey(cx, cy)].append(i);
  }
  QVector<QVector<int>> crowds(n);
  for (int i = 0; i < n; ++i) crowds[find(i)].append(i);
  // Lone sites first, so no crowd takes a site's own place; then the crowded
  // ones in number order, each on its site or the nearest free slot around it.
  for (int i = 0; i < n; ++i)
    if (crowds.at(find(i)).size() == 1) placed[i] = place(origins.at(i), badgeDiametersMm.value(i));
  for (int i = 0; i < n; ++i)
    if (crowds.at(find(i)).size() > 1) placed[i] = place(origins.at(i), badgeDiametersMm.value(i));
  // Two badges of a crowd trade places whenever that shortens their leader
  // lines. Crossing lines always get shorter by trading, so none stay crossed.
  for (const QVector<int>& crowd : crowds) {
    bool traded = crowd.size() > 1;
    for (int pass = 0; traded && pass < kMaxRings; ++pass) {
      traded = false;
      for (int a : crowd) {
        for (int b : crowd) {
          if (a >= b) continue;
          const double now = origins.at(a).distance(placed.at(a)) + origins.at(b).distance(placed.at(b));
          const double then = origins.at(a).distance(placed.at(b)) + origins.at(b).distance(placed.at(a));
          if (then >= now - m_step * 1e-6) continue;
          if (!fits(placed.at(b), badgeDiametersMm.value(a)) || !fits(placed.at(a), badgeDiametersMm.value(b))) continue;
          std::swap(placed[a], placed[b]);
          traded = true;
        }
      }
    }
  }
  return placed;
}

QVector<QPolygonF> LayoutBadgePlacer::legendBlocks(QgsLayoutItemMap* map, bool exporting) {
  QVector<QPolygonF> blocks;
  if (!map || !map->layout()) return blocks;
  const QRectF frame = map->sceneBoundingRect();
  QList<QgsLayoutItemLegend*> legends;
  map->layout()->layoutItems(legends);
  for (QgsLayoutItemLegend* legend : legends) {
    if (!legend || !legend->isVisible()) continue;
    if (exporting && legend->excludeFromExports()) continue;
    if (!legend->sceneBoundingRect().intersects(frame)) continue;
    blocks.append(legend->mapToScene(legend->rect()));
  }
  return blocks;
}

QPainterPath LayoutBadgePlacer::withoutBlocks(const QPainterPath& visibleScene,
                                              const QVector<QPolygonF>& blocks) {
  QPainterPath allowed = visibleScene;
  for (const QPolygonF& block : blocks) {
    QPainterPath cut;
    cut.addPolygon(block);
    cut.closeSubpath();
    allowed = allowed.subtracted(cut);
  }
  return allowed;
}
