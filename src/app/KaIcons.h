#pragma once
#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

namespace KaIcons {
// Glyph styles. Outline (default): one charcoal outline on a transparent square; a
// checked button shows the accent glyph on an accentWash tile; save_unsaved alone keeps a
// solid accent tile with a warn dot. Tile: the legacy light group-coloured tile, kept
// behind KA_HGIS_ICON_STYLE=tile or setGlyphStyle(Tile). An id without an outline drawing
// renders its legacy drawing in plain ink, never a "?" box.
enum class GlyphStyle { Tile, Outline };
GlyphStyle glyphStyle();
void setGlyphStyle(GlyphStyle style);
// The start-up style: Outline unless KA_HGIS_ICON_STYLE is "tile".
GlyphStyle glyphStyleFromEnvironment();

// Field icons for every mode/state and DPI. An id without any glyph logs one warning and
// shows a dashed "?" box, never another icon.
QIcon icon(const QString& id);
// Tile: the same glyph on a solid group-coloured tile (저장·도면·인쇄). Outline: icon().
QIcon strongIcon(const QString& id);
// A valid ink requests a monochrome icon in every mode/state.
QIcon icon(const QString& id, const QColor& ink);
// One direct render in ink at px logical pixels (dpr device scale) for chips, dots, badges
// and search fields: the outline stroke is drawn at that size, not scaled from a bake.
QPixmap glyphPixmap(const QString& id, const QColor& ink, int px, qreal dpr = 1.0);
QIcon appIcon();
// True when id has its own glyph in either style (or is an alias of one).
bool hasIcon(const QString& id);
}
