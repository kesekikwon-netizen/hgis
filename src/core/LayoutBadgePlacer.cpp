#include "LayoutBadgePlacer.h"

#include <QLineF>
#include <QList>
#include <QRectF>

#include <qgslayout.h>
#include <qgslayoutitemlegend.h>
#include <qgslayoutitemmap.h>

#include <algorithm>
#include <cmath>

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
  // Each placed badge blocks at most a few slots of any ring, while ring n
  // holds about 2*pi*n slots, so this many rings always has a free slot. On a
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
