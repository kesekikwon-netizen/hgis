#pragma once
// Mockup icon look (design 2026-10-01-mockup-match, "아이콘 체계"): a rounded light-grey tile
// with a navy Lucide line glyph. KaIcons::icon(id) calls mockupIcon() in the Mockup glyph
// style; other code asks KaIcons, not this header (tests excepted).
#include <QColor>
#include <QIcon>
#include <QString>

namespace KaIconsMockup {

// Lucide/ka SVG resource path for an app icon id, or an empty string when unmapped.
QString svgPathFor(const QString& id);

// Tile + glyph icon, drawn on demand at the size a widget asks for (any dpr).
// strong=true is the 저장-unsaved look (save_unsaved): solid blue tile, white glyph, warn dot.
// A null icon when id has no mapped SVG.
QIcon mockupIcon(const QString& id, bool strong = false);

// What one mode/state paints. tileBorder is invalid when the tile has no border;
// glyphOpacity fades the glyph as one group (disabled).
struct Look {
  QColor tile;
  QColor tileBorder;
  QColor glyph;
  qreal glyphOpacity = 1.0;
  bool dot = false;
};
Look lookFor(QIcon::Mode mode, QIcon::State state, bool strong);

}  // namespace KaIconsMockup
