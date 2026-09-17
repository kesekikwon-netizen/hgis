#pragma once

#include <QString>
#include <functional>

class QgsMapSettings;

namespace MapGeoTiffExport {
// Render the supplied canvas snapshot, including its CRS and rotation, as RGB GeoTIFF.
// Call on the GUI thread; the caller must keep its layers alive until this returns.
bool write(const QgsMapSettings& snapshot, const QString& path, QString* error = nullptr,
           std::function<bool()> canceled = {});
}
