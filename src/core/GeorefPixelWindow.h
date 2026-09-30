#pragma once

#include <QImage>
#include <QRect>
#include <QSize>
#include <QString>

namespace GeorefService {

// Reads one pixel window of a (possibly huge) image straight from the file through GDAL,
// resampled to outSize. The align panel shows a reduced preview of scans Qt cannot open;
// when the user zooms in, the visible window is read again from the original with this.
// Supports 8-bit grey, paletted and RGB(A) images. Returns a null image otherwise.
QImage readPixelWindow(const QString& path, const QRect& window, const QSize& outSize,
                       QString* errorOut = nullptr);

}  // namespace GeorefService
