#pragma once

#include <QList>
#include <QString>

// The one table of drawing scale denominators Strata offers. Studio chips,
// the 「조정끝」 snap, the section scale list and the tile-print enlargement
// list are filtered views of it, so a standard scale is added in one place.
namespace StandardScales {

enum Use : unsigned {
  Snap = 1u << 0,          // 「조정끝」 snap: smallest standard that holds the extent (ends on 10)
  StudioChip = 1u << 1,    // drawing studio quick chips (첫 축척 is still the map scale)
  Section = 1u << 2,       // section sheet scale list (excavation 1:10 .. 1:250)
  PrintEnlarge = 1u << 3,  // tile print 「키워 찍기」 targets (excavation + cadastral)
  AnyUse = 0xFFu,
};

// Ascending denominators that carry any of the requested uses.
QList<int> denominators(unsigned uses);
// True when the rounded denominator is in the table for any requested use.
bool isStandard(double denominator, unsigned uses = AnyUse);
// Smallest listed denominator >= raw (never snaps down, so nothing is clipped).
// Returns the largest listed value when raw is beyond the table, 0 for bad input.
int snapUp(double raw, unsigned uses = Snap);
// "1:2,500" style label.
QString label(double denominator);

}  // namespace StandardScales
