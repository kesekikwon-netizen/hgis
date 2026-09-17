#pragma once
#include <QColor>
#include <QIcon>
#include <QString>

namespace KaIcons {
// Rounded glossy group-colored field icons, including high-DPI and state variants.
QIcon icon(const QString& id);
// A valid ink requests a monochrome icon in every mode/state.
QIcon icon(const QString& id, const QColor& ink);
QIcon appIcon();
}
