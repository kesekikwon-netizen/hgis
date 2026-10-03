#pragma once

#include <QHash>
#include <QPainterPath>
#include <QPolygonF>
#include <QTransform>
#include <QVector>

#include <qgspointxy.h>

class QgsLayoutItemMap;

// Places layout number badges (heritage site numbers) in map units.
// - No two badge circles cover each other (centre distance >= 0.96 step).
// - Each circle sits fully on the allowed paper: the map frame on the page,
//   minus any legend card that overlaps the map. A number clipped by the frame
//   or hidden under the legend would still be listed in the legend.
// A badge that cannot stay on its site moves to the nearest free slot on rings
// around it. The ring count grows with the number of placed badges, so a dense
// cluster never falls back to stacking badges on one spot.
class LayoutBadgePlacer {
public:
  // mapToScene: map coordinates -> layout scene (mm). allowedScene empty = no paper limit.
  LayoutBadgePlacer(const QTransform& mapToScene, const QPainterPath& allowedScene, double stepMapUnits);

  // Chosen centre for a badge of the given diameter; the result is occupied.
  QgsPointXY place(const QgsPointXY& origin, double badgeDiameterMm);
  // Places every badge of the sheet at once, in the given (number) order. Sites
  // chained closer than the clear distance form a crowd: the badges of a crowd of
  // kBlockFrom or more stand on one square grid around its sites (aligned rows
  // and columns, sites left uncovered), each on the free cell nearest its own
  // site. Lone sites and smaller crowds use place().
  QVector<QgsPointXY> placeAll(const QVector<QgsPointXY>& origins, const QVector<double>& badgeDiametersMm);
  // Occupy a position without searching (e.g. a site whose transform failed).
  void occupy(const QgsPointXY& point);
  int count() const { return m_count; }

  // Scene polygons of legend cards that overlap the map frame.
  static QVector<QPolygonF> legendBlocks(QgsLayoutItemMap* map, bool exporting);
  // visibleScene minus the given blocks.
  static QPainterPath withoutBlocks(const QPainterPath& visibleScene, const QVector<QPolygonF>& blocks);
  // Slots on ring n whose neighbours are one step apart (at least 4).
  static int slotsOnRing(int ring);

  static constexpr int kBaseRings = 6;
  static constexpr int kMaxRings = 60;
  static constexpr int kBlockFrom = 5;

private:
  void placeOnGrid(const QVector<int>& crowd, const QVector<QgsPointXY>& origins,
                   const QVector<double>& badgeDiametersMm, QVector<QgsPointXY>& placed);
  bool isFree(const QgsPointXY& point) const;
  bool fits(const QgsPointXY& point, double badgeDiameterMm) const;
  quint64 cellKey(qint64 cx, qint64 cy) const;

  QTransform m_toScene;
  QPainterPath m_allowed;
  bool m_limited = false;
  double m_step = 1.0;
  // Rings that span the allowed paper: a site under a wide legend card needs
  // more than the base rings to get clear of it, whatever the badge count.
  int m_reachRings = 0;
  int m_count = 0;
  QHash<quint64, QVector<QgsPointXY>> m_cells;
};
