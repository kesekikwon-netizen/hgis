#include "GeorefPixelWindow.h"

#include <QScopeGuard>
#include <QVector>
#include <QtGlobal>
#include <algorithm>

#include <gdal.h>

namespace GeorefService {

QImage readPixelWindow(const QString& path, const QRect& window, const QSize& outSize,
                       QString* errorOut) {
  auto fail = [errorOut](const QString& message) {
    if (errorOut) *errorOut = message;
    return QImage();
  };
  if (path.isEmpty() || window.isEmpty() || outSize.isEmpty())
    return fail(QStringLiteral("읽을 범위가 없습니다"));
  GDALAllRegister();
  GDALDatasetH ds = GDALOpenEx(qUtf8Printable(path), GDAL_OF_RASTER | GDAL_OF_READONLY, nullptr,
                               nullptr, nullptr);
  if (!ds) return fail(QStringLiteral("그림을 열지 못했습니다"));
  const auto closeDs = qScopeGuard([ds]() { GDALClose(ds); });

  const QRect full(0, 0, GDALGetRasterXSize(ds), GDALGetRasterYSize(ds));
  if (!full.contains(window)) return fail(QStringLiteral("그림 밖을 읽으려 했습니다"));
  const int bands = GDALGetRasterCount(ds);
  GDALRasterBandH first = bands > 0 ? GDALGetRasterBand(ds, 1) : nullptr;
  if (!first || GDALGetRasterDataType(first) != GDT_Byte)
    return fail(QStringLiteral("8비트 그림만 원본에서 다시 읽습니다"));

  // Never upsample: one output pixel is at least one source pixel.
  const int ow = std::clamp(outSize.width(), 1, window.width());
  const int oh = std::clamp(outSize.height(), 1, window.height());
  // Palette indices cannot be averaged; they are picked, like the preview does.
  GDALColorTableH ct = bands < 3 ? GDALGetRasterColorTable(first) : nullptr;
  GDALRasterIOExtraArg extra;
  INIT_RASTERIO_EXTRA_ARG(extra);
  extra.eResampleAlg = (!ct && (ow < window.width() || oh < window.height()))
                           ? GRIORA_Average
                           : GRIORA_NearestNeighbour;
  CPLErr err = CE_Failure;
  QImage img;
  if (bands >= 3) {
    img = QImage(ow, oh, QImage::Format_RGB888);
    int bandMap[3] = {1, 2, 3};
    err = GDALDatasetRasterIOEx(ds, GF_Read, window.x(), window.y(), window.width(),
                                window.height(), img.bits(), ow, oh, GDT_Byte, 3, bandMap, 3,
                                img.bytesPerLine(), 1, &extra);
  } else {
    img = QImage(ow, oh, ct ? QImage::Format_Indexed8 : QImage::Format_Grayscale8);
    if (ct) {
      QVector<QRgb> colors(256, qRgb(0, 0, 0));
      const int n = std::min(256, GDALGetColorEntryCount(ct));
      for (int i = 0; i < n; ++i) {
        GDALColorEntry e;
        if (GDALGetColorEntryAsRGB(ct, i, &e)) colors[i] = qRgba(e.c1, e.c2, e.c3, e.c4);
      }
      img.setColorTable(colors);
    }
    err = GDALRasterIOEx(first, GF_Read, window.x(), window.y(), window.width(), window.height(),
                         img.bits(), ow, oh, GDT_Byte, 1, img.bytesPerLine(), &extra);
  }
  if (err != CE_None) return fail(QStringLiteral("원본에서 이 부분을 읽지 못했습니다"));
  return img;
}

}  // namespace GeorefService
