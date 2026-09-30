#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

class QgsRasterLayer;
class QgsCoordinateReferenceSystem;

// Raster alignment writes a world file and .prj next to the image, removes a GDAL .aux.xml
// that carries georeferencing, and replaces a GeoTIFF's own geotransform. Everything it is
// about to overwrite is copied first into the survey folder (정합백업), so 「되돌리기」 can
// put the image back exactly as it was when the alignment session started.
namespace GeorefBackup {

struct Sidecar {
  QString path;  // file next to the image
  QString copy;  // saved copy in the backup folder; empty when the file did not exist
};

struct Gcp {
  QByteArray id;
  QByteArray info;
  double pixel = 0, line = 0, x = 0, y = 0, z = 0;
};

struct RasterBackup {
  bool valid = false;
  QString imagePath;
  QString folder;
  QVector<Sidecar> sidecars;
  bool hadGeoTransform = false;  // internal (GeoTIFF) geotransform only
  double geoTransform[6] = {0, 1, 0, 0, 0, 1};
  QVector<Gcp> gcps;  // internal GCPs only
  QByteArray gcpWkt;
};

// <survey folder>/정합백업, or the app data folder when no survey is open.
QString backupRootFor(const QString& surveyPath);

// Both names GDAL uses for the PAM sidecar (image.tif.aux.xml, image.aux.xml).
QStringList pamSidecarPaths(const QString& imagePath);
// True when the .aux.xml holds a geotransform or GCPs, which would override a world file.
bool pamCarriesGeoref(const QString& auxPath);
// True when a GeoTIFF has its own geotransform or GCPs (GDAL then ignores world files).
bool hasInternalGeoref(const QString& imagePath);

bool backupRaster(const QString& imagePath, const QString& backupRoot, RasterBackup* out,
                  QString* errorOut = nullptr);
// Puts sidecars and the internal geotransform/GCPs back as recorded in the backup.
bool restoreRaster(const RasterBackup& backup, QString* errorOut = nullptr);
// restoreRaster, then re-reads the same QgsRasterLayer (no removeMapLayer, no new layer).
bool restoreRasterLayer(QgsRasterLayer* layer, const RasterBackup& backup,
                        const QgsCoordinateReferenceSystem& crs, QString* errorOut = nullptr);

// Closes the GDAL handle the layer's provider keeps open. QGIS writes the band statistics
// it computed into that handle's PAM state, and a dirty handle re-serialises everything it
// loaded (a PAM GeoTransform included) into the .aux.xml when it closes. Call it before the
// sidecars change, or that flush lands on top of them.
void releaseRasterHandle(QgsRasterLayer* layer);
// Same layer object, same id: the data source is read again from disk. A provider
// reloadData() alone leaves the layer extent stale.
void reopenRasterLayer(QgsRasterLayer* layer, const QgsCoordinateReferenceSystem& crs);
// True when either PAM sidecar of the image carries georeferencing.
bool pamGeorefPresent(const QString& imagePath);

// desiredPath if free, otherwise name_2.ext, name_3.ext, ... (never overwrites).
QString uniqueOutputPath(const QString& desiredPath);

}  // namespace GeorefBackup
