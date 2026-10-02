#pragma once

#include <QString>
#include <qgsrectangle.h>

class QgsRasterLayer;
class QgsMapCanvas;
class QgsCoordinateReferenceSystem;

// Shared display policy. Elevations remain in the original datasource.
namespace DemPresentation {
bool apply(QgsRasterLayer* layer, const QString& preset = QStringLiteral("national"),
           const QgsRectangle& extent = QgsRectangle());
bool restore(QgsRasterLayer* layer);
void followCanvas(QgsRasterLayer* layer, QgsMapCanvas* canvas);
// Rough elevation range inside extent (layer CRS), read from a small window with GDAL only, so
// no statistics file is written beside the user's DEM. False when the window holds no data.
bool sampleRange(QgsRasterLayer* layer, const QgsRectangle& extent, double* minimum, double* maximum);
QString reliefSource(QgsRasterLayer* layer, const QgsCoordinateReferenceSystem& workCrs,
                     QString* error);
}
