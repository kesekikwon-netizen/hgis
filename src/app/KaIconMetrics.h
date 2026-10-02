#pragma once

#include <QtGlobal>

#include <algorithm>

// Stroke rules for KaIcons glyphs, drawn on a 64-unit grid and baked at 64 and
// 128 px. Kept apart so tests can check the rule without painting.
namespace KaIconMetrics {

// The thinnest stroke a glyph may paint, in device pixels of the 64 px bake:
// 2 px there is 1 px once Qt scales the bake down for a 32 px ribbon icon.
inline constexpr qreal kMinDeviceStroke = 2.0;

// Width in glyph units for the width a glyph asks for. The glyph's own width is
// used (2.2 for dense drawings, 3.2 for bold arrows), floored so no line drops
// under kMinDeviceStroke. deviceScale is device pixels per glyph unit.
inline qreal strokeWidth(qreal declared, qreal deviceScale) {
  if (!(deviceScale > 0.0)) return declared;
  return std::max(declared, kMinDeviceStroke / deviceScale);
}

// Outline style (KaIconsOutline*.cpp): the main stroke and the thin secondary
// stroke in glyph units, and the box every glyph stays inside (12..52 of 64) so an
// 8-unit band around it is always transparent and nothing touches a tile edge.
inline constexpr qreal kOutlineStroke = 3.2;
inline constexpr qreal kOutlineThin = 2.4;
inline constexpr int kOutlineInset = 12;
// Outline glyphs are drawn straight at the requested size. From 24 px up the
// 2 px floor applies (2 px at 32 px ribbon chips); below that a 2 px line turns
// a dense drawing into a blob, so menus, tabs and 14 px chips floor at 1.5 px.
inline constexpr qreal kOutlineSmallDeviceStroke = 1.5;
inline constexpr qreal kOutlineSmallPx = 24.0;

inline qreal outlineStrokeWidth(qreal declared, qreal deviceScale) {
  if (!(deviceScale > 0.0)) return declared;
  const qreal floorPx = deviceScale * 64.0 < kOutlineSmallPx ? kOutlineSmallDeviceStroke : kMinDeviceStroke;
  return std::max(declared, floorPx / deviceScale);
}

}  // namespace KaIconMetrics
