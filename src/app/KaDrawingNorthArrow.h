#pragma once
// North arrow artwork for the drawing studio. The arrow is linked to the map in
// grid-north mode, so it points to the projected Y axis (도북), not true north.
#include <QIcon>
#include <QString>

class QPainter;
class QgsLayoutItem;

namespace KaDrawingNorth {
// Artwork kind from a sample path: 0 letter, 1 survey needle, 2 compass, 3 wind rose.
int kindFromRel(const QString& rel);
// Paints the artwork inside a 72x72 design box. Letters are drawn as outlines,
// so the result does not depend on installed fonts.
void paintMark(QPainter& painter, int kind);
// Writes the artwork as an SVG file (vector in PDF). Empty on failure.
QString writeSvg(int kind);
QIcon previewIcon(int kind);
// QGIS bundled arrow, used only when the generated file cannot be read.
QString qgisArrowSvg();
bool pictureNeedsRebuild(QgsLayoutItem* item);
// Kind encoded in a generated arrow file name (ka-hgis-north-<kind>.svg|png), or -1.
int generatedKind(const QgsLayoutItem* item);
// Sample path that selects kind in kindFromRel.
QString relForKind(int kind);
// Older sheets carry the generated arrow as a PNG raster; they are redrawn as SVG.
bool isLegacyRaster(const QgsLayoutItem* item);
// Text label used when no picture can be made. Grid north, like the picture.
QString fallbackLabel();
// Short explanation shown on the north buttons.
QString gridNorthNote();
}  // namespace KaDrawingNorth
