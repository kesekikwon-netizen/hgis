#pragma once
// Internal API of the outline glyph set (KaIconsOutlineA/B/Ui.cpp draw, KaIconsOutlineEngine.cpp
// renders). KaIcons.cpp is the only consumer; the app asks KaIcons::icon(id).
#include <QColor>
#include <QImage>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QString>

#include "KaIconMetrics.h"

class QIcon;

namespace KaIconsOutline {

using Glyph = void (*)(QPainter&);

// Drawer contract: the painter arrives scaled to the 64-unit grid, antialiased, with the pen
// colour set to the ink. Drawers keep that colour, pick widths with stroke(), fill nothing but
// dots of 4 units or less, and stay inside KaIconMetrics::kOutlineInset .. 64 - kOutlineInset.
inline QColor ink(const QPainter& p) { return p.pen().color(); }

inline void stroke(QPainter& p, qreal width = KaIconMetrics::kOutlineStroke,
                   Qt::PenStyle style = Qt::SolidLine) {
  p.setPen(QPen(ink(p), KaIconMetrics::outlineStrokeWidth(width, p.worldTransform().m11()), style, Qt::RoundCap,
                Qt::RoundJoin));
  p.setBrush(Qt::NoBrush);
}

inline void dot(QPainter& p, const QPointF& centre, qreal radius) {
  const QPen kept = p.pen();
  p.setPen(Qt::NoPen);
  p.setBrush(kept.color());
  p.drawEllipse(centre, radius, radius);
  p.setPen(kept);
  p.setBrush(Qt::NoBrush);
}

// Glyph tables by ribbon group: A = 조사·기록·자료 받기, B = 배경 지도·정합·내보내기·기타,
// Ui = tabs, home, panels, chips and map overlay. Null when that file does not draw the id.
Glyph outlineGlyphA(const QString& id);
Glyph outlineGlyphB(const QString& id);
Glyph outlineGlyphUi(const QString& id);
// The union of the three tables; aliases resolve to the same drawer.
Glyph outlineGlyphFor(const QString& id);
bool hasOutlineGlyph(const QString& id);

// Legacy drawing of an id without an outline glyph, flat (no tile) in charcoal, Normal/Off,
// `size` px square. Supplied by KaIcons.cpp; the engine tints it per mode and state.
using FallbackImage = QImage (*)(const QString& id, int size);

// An icon rendered on demand at any size in the outline style: ink glyph, accentWash tile
// when checked, neutral grey when disabled, the selection ring, and for save_unsaved the
// solid accent tile with a warn dot. A valid fixedInk makes every mode and state that one
// colour (disabled at 45 % opacity) with no tile and no ring.
QIcon outlineIcon(const QString& id, Glyph glyph, FallbackImage fallback, const QColor& fixedInk = QColor());
// One direct render of a glyph in ink at px logical pixels; the pixmap carries dpr.
QPixmap renderGlyph(Glyph glyph, const QColor& ink, int px, qreal dpr = 1.0);
// Keeps each pixel's coverage but paints it in ink (mask tint of a flat legacy render).
QImage tintToInk(const QImage& source, const QColor& ink);

}  // namespace KaIconsOutline
