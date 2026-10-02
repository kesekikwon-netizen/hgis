#include "DemAnalyzer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <gdal_priv.h>
#include <cpl_conv.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

constexpr double kDeg2Rad = M_PI / 180.0;

void hornDeriv(const std::vector<float>& z, int w, int h, int x, int y, double xres, double yres,
               float nd, bool hasNd, double zf, double* dzdx, double* dzdy, bool* ok) {
  *ok = false;
  if (x <= 0 || y <= 0 || x >= w - 1 || y >= h - 1)
    return;
  auto cell = [&](int cx, int cy) -> float { return z[static_cast<size_t>(cy) * w + cx]; };
  // Like gdaldem, a NoData centre cell has no shade even when its neighbours hold data.
  if (hasNd && cell(x, y) == nd)
    return;
  const float a = cell(x - 1, y - 1);
  const float b = cell(x, y - 1);
  const float c = cell(x + 1, y - 1);
  const float d = cell(x - 1, y);
  const float f = cell(x + 1, y);
  const float g = cell(x - 1, y + 1);
  const float hh = cell(x, y + 1);
  const float i = cell(x + 1, y + 1);
  if (hasNd) {
    if (a == nd || b == nd || c == nd || d == nd || f == nd || g == nd || hh == nd || i == nd)
      return;
  }
  const double dx = ((c + 2.0 * f + i) - (a + 2.0 * d + g)) / (8.0 * xres);
  const double dy = ((g + 2.0 * hh + i) - (a + 2.0 * b + c)) / (8.0 * std::abs(yres));
  *dzdx = dx * zf;
  *dzdy = dy * zf;
  *ok = true;
}

std::uint8_t shadeOne(double dzdx, double dzdy, double zenith, double azimuthMath) {
  const double slope = std::atan(std::sqrt(dzdx * dzdx + dzdy * dzdy));
  double aspect = std::atan2(dzdy, -dzdx);
  const double hs =
      std::cos(zenith) * std::cos(slope) + std::sin(zenith) * std::sin(slope) * std::cos(azimuthMath - aspect);
  const double v = std::clamp(hs, 0.0, 1.0);
  return static_cast<std::uint8_t>(std::lround(v * 255.0));
}

}  // namespace

namespace DemAnalyzer {

bool readFloatBand(const QString& tifPath, std::vector<float>* out, RasterInfo* info,
                   QString* errorOut) {
  if (!out || !info) {
    if (errorOut)
      *errorOut = QStringLiteral("null buffer");
    return false;
  }
  GDALAllRegister();
  GDALDataset* ds = static_cast<GDALDataset*>(
      GDALOpen(tifPath.toUtf8().constData(), GA_ReadOnly));
  if (!ds) {
    if (errorOut)
      *errorOut = QStringLiteral("DEM을 열 수 없습니다.");
    return false;
  }
  GDALRasterBand* band = ds->GetRasterBand(1);
  if (!band) {
    GDALClose(ds);
    if (errorOut)
      *errorOut = QStringLiteral("밴드가 없습니다.");
    return false;
  }
  info->width = ds->GetRasterXSize();
  info->height = ds->GetRasterYSize();
  ds->GetGeoTransform(info->geotransform);
  if (const char* wkt = ds->GetProjectionRef())
    info->projectionWkt = QString::fromUtf8(wkt);
  int hasNd = 0;
  const double nd = band->GetNoDataValue(&hasNd);
  info->hasNoData = hasNd != 0;
  info->noData = static_cast<float>(nd);
  const size_t n = static_cast<size_t>(info->width) * static_cast<size_t>(info->height);
  out->assign(n, info->noData);
  const CPLErr err =
      band->RasterIO(GF_Read, 0, 0, info->width, info->height, out->data(), info->width,
                     info->height, GDT_Float32, 0, 0);
  GDALClose(ds);
  if (err != CE_None) {
    if (errorOut)
      *errorOut = QStringLiteral("DEM 읽기 실패");
    return false;
  }
  return true;
}

bool readFloatWindow(const QString& tifPath, double xMin, double yMin, double xMax, double yMax,
                     int maxEdge, std::vector<float>* out, RasterInfo* info, QString* errorOut) {
  if (!out || !info) {
    if (errorOut)
      *errorOut = QStringLiteral("null buffer");
    return false;
  }
  if (!(xMax > xMin) || !(yMax > yMin)) {
    if (errorOut)
      *errorOut = QStringLiteral("자를 범위가 없습니다.");
    return false;
  }
  GDALAllRegister();
  if (tifPath.contains(QLatin1String("vsicurl")) || tifPath.contains(QLatin1String("http")))
    CPLSetConfigOption("GDAL_DISABLE_READDIR_ON_OPEN", "EMPTY_DIR");
  GDALDataset* ds = static_cast<GDALDataset*>(GDALOpen(tifPath.toUtf8().constData(), GA_ReadOnly));
  if (!ds) {
    if (errorOut)
      *errorOut = QStringLiteral("DEM을 열 수 없습니다.");
    return false;
  }
  GDALRasterBand* band = ds->GetRasterBand(1);
  if (!band) {
    GDALClose(ds);
    if (errorOut)
      *errorOut = QStringLiteral("밴드가 없습니다.");
    return false;
  }
  double gt[6] = {0, 1, 0, 0, 0, -1};
  ds->GetGeoTransform(gt);
  double inv[6] = {0, 1, 0, 0, 0, -1};
  if (!GDALInvGeoTransform(gt, inv)) {
    GDALClose(ds);
    if (errorOut)
      *errorOut = QStringLiteral("좌표 변환이 안됩니다.");
    return false;
  }
  const int rw = ds->GetRasterXSize();
  const int rh = ds->GetRasterYSize();
  const double xs[4] = {xMin, xMax, xMin, xMax};
  const double ys[4] = {yMin, yMin, yMax, yMax};
  double minC = 1e300, maxC = -1e300, minR = 1e300, maxR = -1e300;
  for (int i = 0; i < 4; ++i) {
    double c = 0, r = 0;
    GDALApplyGeoTransform(inv, xs[i], ys[i], &c, &r);
    minC = std::min(minC, c);
    maxC = std::max(maxC, c);
    minR = std::min(minR, r);
    maxR = std::max(maxR, r);
  }
  int x0 = std::max(0, static_cast<int>(std::floor(minC)));
  int y0 = std::max(0, static_cast<int>(std::floor(minR)));
  int x1 = std::min(rw, static_cast<int>(std::ceil(maxC)));
  int y1 = std::min(rh, static_cast<int>(std::ceil(maxR)));
  int winW = x1 - x0;
  int winH = y1 - y0;
  if (winW < 2 || winH < 2) {
    GDALClose(ds);
    if (errorOut)
      *errorOut = QStringLiteral("화면 범위에 DEM이 없습니다.");
    return false;
  }
  int outW = winW;
  int outH = winH;
  const int cap = maxEdge > 1 ? maxEdge : winW;
  const int longEdge = std::max(winW, winH);
  if (longEdge > cap) {
    const double s = static_cast<double>(cap) / static_cast<double>(longEdge);
    outW = std::max(2, static_cast<int>(std::lround(winW * s)));
    outH = std::max(2, static_cast<int>(std::lround(winH * s)));
  }
  int hasNd = 0;
  const double nd = band->GetNoDataValue(&hasNd);
  info->hasNoData = hasNd != 0;
  info->noData = static_cast<float>(nd);
  info->width = outW;
  info->height = outH;
  if (const char* wkt = ds->GetProjectionRef())
    info->projectionWkt = QString::fromUtf8(wkt);
  double ox = 0, oy = 0;
  GDALApplyGeoTransform(gt, x0, y0, &ox, &oy);
  const double sx = static_cast<double>(winW) / static_cast<double>(outW);
  const double sy = static_cast<double>(winH) / static_cast<double>(outH);
  info->geotransform[0] = ox;
  info->geotransform[1] = gt[1] * sx;
  info->geotransform[2] = gt[2] * sy;
  info->geotransform[3] = oy;
  info->geotransform[4] = gt[4] * sx;
  info->geotransform[5] = gt[5] * sy;
  out->assign(static_cast<size_t>(outW) * static_cast<size_t>(outH), info->noData);
  const CPLErr err = band->RasterIO(GF_Read, x0, y0, winW, winH, out->data(), outW, outH,
                                    GDT_Float32, 0, 0);
  GDALClose(ds);
  if (err != CE_None) {
    if (errorOut)
      *errorOut = QStringLiteral("DEM 창 읽기 실패");
    return false;
  }
  return true;
}

void hillshadeHorn(const std::vector<float>& z, const RasterInfo& info, const Options& opt,
                   std::vector<std::uint8_t>* outGray) {
  const int w = info.width;
  const int h = info.height;
  outGray->assign(static_cast<size_t>(w) * h, 0);
  const double xres = info.geotransform[1] != 0.0 ? info.geotransform[1] : 1.0;
  const double yres = info.geotransform[5] != 0.0 ? info.geotransform[5] : -1.0;
  const double zenith = (90.0 - opt.altitudeDeg) * kDeg2Rad;
  auto azMath = [](double azCwNorth) {
    return (360.0 - azCwNorth + 90.0) * kDeg2Rad;
  };
  const double azs[4] = {225.0, 270.0, 315.0, 360.0};

#if defined(_OPENMP)
#pragma omp parallel for schedule(static)
#endif
  for (int y = 1; y < h - 1; ++y) {
    for (int x = 1; x < w - 1; ++x) {
      double dzdx = 0, dzdy = 0;
      bool ok = false;
      hornDeriv(z, w, h, x, y, xres, yres, info.noData, info.hasNoData, opt.zFactor, &dzdx, &dzdy,
                &ok);
      if (!ok)
        continue;
      std::uint8_t v = 0;
      if (opt.hillshade == HillshadeMode::Multi) {
        int sum = 0;
        for (double az : azs)
          sum += shadeOne(dzdx, dzdy, zenith, azMath(az));
        v = static_cast<std::uint8_t>(sum / 4);
      } else {
        v = shadeOne(dzdx, dzdy, zenith, azMath(opt.azimuthDeg));
      }
      (*outGray)[static_cast<size_t>(y) * w + x] = v;
    }
  }
}

void slopeDegrees(const std::vector<float>& z, const RasterInfo& info, double zFactor,
                  std::vector<float>* outDeg) {
  const int w = info.width;
  const int h = info.height;
  outDeg->assign(static_cast<size_t>(w) * h, 0.0f);
  const double xres = info.geotransform[1] != 0.0 ? info.geotransform[1] : 1.0;
  const double yres = info.geotransform[5] != 0.0 ? info.geotransform[5] : -1.0;
#if defined(_OPENMP)
#pragma omp parallel for schedule(static)
#endif
  for (int y = 1; y < h - 1; ++y) {
    for (int x = 1; x < w - 1; ++x) {
      double dzdx = 0, dzdy = 0;
      bool ok = false;
      hornDeriv(z, w, h, x, y, xres, yres, info.noData, info.hasNoData, zFactor, &dzdx, &dzdy, &ok);
      if (!ok)
        continue;
      const double deg = std::atan(std::sqrt(dzdx * dzdx + dzdy * dzdy)) * 180.0 / M_PI;
      (*outDeg)[static_cast<size_t>(y) * w + x] = static_cast<float>(deg);
    }
  }
}

// writeByteGeoTiff, runHillshadeFile and hillshadeOutputPath live in DemHillshadeFile.cpp.

}  // namespace DemAnalyzer
