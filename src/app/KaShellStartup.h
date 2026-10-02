#pragma once

#include "KaUserError.h"

#include <QString>
#include <QStringList>

// Startup failures that used to end or degrade the app with only a qCritical line.
// A desktop launch has no console and the splash is already up, so the user saw the window
// vanish or the map stay blank. These specs tell what failed and how to fix it; the dialog's
// only extra is KaUserError's 「로그 폴더 열기」. Nothing here restores or loads anything.
namespace KaShellStartup {

// The QGIS folder (apps/qgis-dev next to the program, QGIS_PREFIX_PATH or OSGEO4W_ROOT)
// was not found. The app cannot start without it.
KaUserError::Spec missingQgisSpec(const QString& exeDir);

// Map-data providers the app needs. wms draws 위성·지적, ogr opens survey GPKG/SHP,
// gdal opens GeoTIFF/DEM.
QStringList requiredProviders();
// Entries of requiredProviders() missing from the registry list, in that order.
QStringList missingProviders(const QStringList& available);
// What will not work without the missing providers. The app keeps running.
KaUserError::Spec missingProvidersSpec(const QStringList& missing, const QString& prefix);

// Points KaUserError's 「로그 폴더 열기」 at the session log folder unless already set.
void ensureLogFolder();
// Shows spec as a critical dialog with the log-folder button.
void showCritical(const KaUserError::Spec& spec);

}  // namespace KaShellStartup
