#pragma once

#include <QList>
#include <QString>
#include <functional>

class QgsMapLayer;
class QgsMapSettings;

namespace MapGeoTiffExport {
// Render the supplied canvas snapshot, including its CRS and rotation, as RGB GeoTIFF.
// overlayLayers (top first) are drawn after the snapshot and its labels, the way the
// canvas draws its above-labels pass (survey shapes, heritage); a layer already in the
// snapshot is not drawn twice. Rendering is sequential, never parallel.
// Call on the GUI thread; the caller must keep its layers alive until this returns.
bool write(const QgsMapSettings& snapshot, const QString& path, QString* error = nullptr,
           std::function<bool()> canceled = {}, const QList<QgsMapLayer*>& overlayLayers = {});
}
