#pragma once
#include <QColor>
#include <QIcon>
#include <QString>

namespace KaIcons {
// Rounded group-colored field icons, including high-DPI and state variants: a light tint tile
// with the glyph in a deep shade of the group color, so the map stays the loudest thing on screen.
QIcon icon(const QString& id);
// The same glyph on a solid group-colored tile, for the few buttons people press most
// (저장·도면·인쇄).
QIcon strongIcon(const QString& id);
// A valid ink requests a monochrome icon in every mode/state.
QIcon icon(const QString& id, const QColor& ink);
QIcon appIcon();
}
