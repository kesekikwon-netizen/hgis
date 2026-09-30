#include "DemAnalyzer.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>

#include <cpl_conv.h>
#include <gdal_priv.h>

namespace {

constexpr qint64 kStripCells = 4 * 1024 * 1024;

// Same rule as hornDeriv: the cell and every one of its 8 neighbours must hold data.
bool windowHasData(const std::vector<float>& z, int w, int x, int y, float noData) {
  for (int dy = -1; dy <= 1; ++dy)
    for (int dx = -1; dx <= 1; ++dx)
      if (z[static_cast<size_t>(y + dy) * static_cast<size_t>(w) + static_cast<size_t>(x + dx)] == noData)
        return false;
  return true;
}

bool fail(QString* errorOut, const QString& message) {
  if (errorOut) *errorOut = message;
  return false;
}

}  // namespace

namespace DemAnalyzer {

bool writeByteGeoTiff(const QString& outPath, const RasterInfo& info,
                      const std::vector<std::uint8_t>& gray, QString* errorOut) {
  GDALAllRegister();
  GDALDriver* drv = GetGDALDriverManager()->GetDriverByName("GTiff");
  if (!drv) return fail(errorOut, QStringLiteral("GTiff 드라이버 없음"));
  GDALDataset* ds = drv->Create(outPath.toUtf8().constData(), info.width, info.height, 1, GDT_Byte, nullptr);
  if (!ds) return fail(errorOut, QStringLiteral("GeoTIFF를 만들 수 없습니다."));
  ds->SetGeoTransform(const_cast<double*>(info.geotransform));
  if (!info.projectionWkt.isEmpty()) ds->SetProjection(info.projectionWkt.toUtf8().constData());
  GDALRasterBand* band = ds->GetRasterBand(1);
  band->SetNoDataValue(0);
  const CPLErr err = band->RasterIO(GF_Write, 0, 0, info.width, info.height, const_cast<std::uint8_t*>(gray.data()),
                                    info.width, info.height, GDT_Byte, 0, 0);
  GDALClose(ds);
  return err == CE_None || fail(errorOut, QStringLiteral("GeoTIFF 쓰기 실패"));
}

bool runHillshadeFile(const QString& demPath, const QString& outPath, const Options& opt,
                      QString* errorOut, int stripRows) {
  GDALAllRegister();
  CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "YES");
  GDALDataset* source = static_cast<GDALDataset*>(GDALOpen(demPath.toUtf8().constData(), GA_ReadOnly));
  if (!source) return fail(errorOut, QStringLiteral("DEM을 열 수 없습니다."));
  GDALRasterBand* band = source->GetRasterBand(1);
  RasterInfo info;
  info.width = source->GetRasterXSize();
  info.height = source->GetRasterYSize();
  if (!band || info.width < 3 || info.height < 3) {
    GDALClose(source);
    return fail(errorOut, QStringLiteral("DEM에 음영을 계산할 칸이 부족합니다."));
  }
  source->GetGeoTransform(info.geotransform);
  if (const char* wkt = source->GetProjectionRef()) info.projectionWkt = QString::fromUtf8(wkt);
  int hasNoData = 0;
  info.noData = static_cast<float>(band->GetNoDataValue(&hasNoData));
  info.hasNoData = hasNoData != 0;

  QDir().mkpath(QFileInfo(outPath).absolutePath());
  GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
  const char* create[] = {"COMPRESS=DEFLATE", "TILED=YES", "BIGTIFF=IF_SAFER", nullptr};
  GDALDataset* target = driver ? driver->Create(outPath.toUtf8().constData(), info.width, info.height, 1, GDT_Byte,
                                                const_cast<char**>(create))
                               : nullptr;
  if (!target) {
    GDALClose(source);
    return fail(errorOut, QStringLiteral("음영 GeoTIFF를 만들 수 없습니다. 저장 폴더의 쓰기 권한을 확인하세요."));
  }
  target->SetGeoTransform(info.geotransform);
  if (!info.projectionWkt.isEmpty()) target->SetProjection(info.projectionWkt.toUtf8().constData());
  GDALRasterBand* shade = target->GetRasterBand(1);
  shade->SetNoDataValue(0);

  const int w = info.width;
  const int h = info.height;
  const int rows = stripRows > 0 ? stripRows : static_cast<int>(std::max<qint64>(1, kStripCells / w));
  std::vector<float> z;
  std::vector<std::uint8_t> gray;
  std::vector<std::uint8_t> strip;
  bool ok = true;
  for (int y0 = 0; ok && y0 < h; y0 += rows) {
    const int y1 = std::min(h, y0 + rows);
    // One halo row on each side lets the strip's own edge rows use the full 3x3 window.
    const int r0 = std::max(0, y0 - 1);
    const int count = std::min(h, y1 + 1) - r0;
    z.assign(static_cast<size_t>(w) * static_cast<size_t>(count), info.noData);
    if (band->RasterIO(GF_Read, 0, r0, w, count, z.data(), w, count, GDT_Float32, 0, 0) != CE_None) {
      ok = false;
      break;
    }
    RasterInfo window = info;
    window.height = count;
    hillshadeHorn(z, window, opt, &gray);
    strip.assign(static_cast<size_t>(w) * static_cast<size_t>(y1 - y0), 0);
    for (int y = y0; y < y1; ++y) {
      const int local = y - r0;
      if (y == 0 || y == h - 1 || local == 0 || local == count - 1) continue;
      for (int x = 1; x < w - 1; ++x) {
        if (info.hasNoData && !windowHasData(z, w, x, local, info.noData)) continue;
        const std::uint8_t value = gray[static_cast<size_t>(local) * static_cast<size_t>(w) + static_cast<size_t>(x)];
        strip[static_cast<size_t>(y - y0) * static_cast<size_t>(w) + static_cast<size_t>(x)] =
            std::max<std::uint8_t>(value, 1);
      }
    }
    ok = shade->RasterIO(GF_Write, 0, y0, w, y1 - y0, strip.data(), w, y1 - y0, GDT_Byte, 0, 0) == CE_None;
  }
  GDALClose(target);
  GDALClose(source);
  if (!ok) {
    QFile::remove(outPath);
    return fail(errorOut, QStringLiteral("DEM을 읽거나 음영을 쓰다가 멈췄습니다. 파일과 저장 공간을 확인하세요."));
  }
  return true;
}

QString hillshadeOutputPath(const QString& demPath, const QString& surveyPath) {
  const QFileInfo dem(demPath);
  const QString stem = dem.completeBaseName().isEmpty() ? QStringLiteral("DEM") : dem.completeBaseName();
  const QDir dir(surveyPath.isEmpty()
                     ? dem.absolutePath()
                     : QDir(QFileInfo(surveyPath).absolutePath()).filePath(QStringLiteral("지형분석")));
  QString candidate = dir.filePath(stem + QStringLiteral("_hillshade.tif"));
  for (int n = 2; QFileInfo::exists(candidate) && n < 10000; ++n)
    candidate = dir.filePath(QStringLiteral("%1_hillshade_%2.tif").arg(stem).arg(n));
  return candidate;
}

}  // namespace DemAnalyzer
