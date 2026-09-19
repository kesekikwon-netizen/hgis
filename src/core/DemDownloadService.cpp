#include "KaSessionLog.h"
#include "DemDownloadService.h"

#include <QDir>
#include <QFileInfo>
#include <qgsfeedback.h>
#include <qgsrectangle.h>

#include <cpl_conv.h>
#include <cpl_error.h>
#include <cpl_string.h>
#include <gdal.h>
#include <gdal_utils.h>
#include <ogr_srs_api.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

namespace {

constexpr int kMaximumTiles = 24;

struct CloseDataset {
  void operator()(void* dataset) const { if (dataset) GDALClose(dataset); }
};
using Dataset = std::unique_ptr<void, CloseDataset>;

class ThreadConfiguration {
public:
  ThreadConfiguration() : m_previous(CPLGetThreadLocalConfigOptions()) {
    CPLSetThreadLocalConfigOption("GDAL_HTTP_CONNECTTIMEOUT", "5");
    CPLSetThreadLocalConfigOption("GDAL_HTTP_TIMEOUT", "15");
    CPLSetThreadLocalConfigOption("GDAL_HTTP_MAX_RETRY", "0");
    CPLSetThreadLocalConfigOption("GDAL_DISABLE_READDIR_ON_OPEN", "EMPTY_DIR");
  }
  ~ThreadConfiguration() {
    CPLSetThreadLocalConfigOptions(m_previous);
    CSLDestroy(m_previous);
  }
  ThreadConfiguration(const ThreadConfiguration&) = delete;
  ThreadConfiguration& operator=(const ThreadConfiguration&) = delete;
private:
  char** m_previous;
};

class QuietErrors {
public:
  QuietErrors() { CPLPushErrorHandler(CPLQuietErrorHandler); }
  ~QuietErrors() { CPLPopErrorHandler(); }
};

struct Progress {
  QgsFeedback* feedback;
  const std::function<bool()>& cancellationRequested;
  double begin = 0.0;
  double span = 100.0;
  bool callbackFailed = false;

  bool isCancelled() const {
    return (feedback && feedback->isCanceled()) ||
           (cancellationRequested && cancellationRequested());
  }

  static int CPL_STDCALL update(double fraction, const char*, void* data) {
    auto* progress = static_cast<Progress*>(data);
    try {
      if (progress->isCancelled()) return FALSE;
      if (progress->feedback)
        progress->feedback->setProgress(progress->begin +
            std::clamp(fraction, 0.0, 1.0) * progress->span);
      return TRUE;
    } catch (...) {
      KaSessionLog::line(QStringLiteral("[except] core/DemDownloadService.cpp:74"));
      // GDAL must not unwind through a C callback.
      progress->callbackFailed = true;
      return FALSE;
    }
  }
};

QString tileUri(int latitude, int longitude) {
  const QString north = QStringLiteral("%1%2")
      .arg(latitude >= 0 ? QLatin1Char('N') : QLatin1Char('S'))
      .arg(std::abs(latitude), 2, 10, QLatin1Char('0'));
  const QString east = QStringLiteral("%1%2")
      .arg(longitude >= 0 ? QLatin1Char('E') : QLatin1Char('W'))
      .arg(std::abs(longitude), 3, 10, QLatin1Char('0'));
  const QString name = QStringLiteral("Copernicus_DSM_COG_10_%1_00_%2_00_DEM").arg(north, east);
  return QStringLiteral("/vsicurl/https://copernicus-dem-30m.s3.amazonaws.com/%1/%1.tif").arg(name);
}

bool isMissingTile(const QString& error) {
  return error.contains(QStringLiteral("HTTP response code: 404"), Qt::CaseInsensitive) ||
         error.contains(QStringLiteral("HTTP error code : 404"), Qt::CaseInsensitive) ||
         error.contains(QStringLiteral("404 Not Found"), Qt::CaseInsensitive);
}

bool gridAligned(double delta, double spacing) {
  const double cells = delta / spacing;
  return std::abs(cells - std::round(cells)) < 1e-5;
}

}  // namespace

PreparedReferenceMap DemDownloadService::prepare(
    const QgsRectangle& wgs84Extent, const QString& targetBasePath,
    QgsFeedback* feedback, const std::function<bool()>& cancellationRequested) {
  PreparedReferenceMap result;
  Progress progress{feedback, cancellationRequested};
  const auto cancelled = [&]() {
    if (!progress.isCancelled()) return false;
    result.status = PreparedReferenceMap::Status::Cancelled;
    result.error = QStringLiteral("DEM 내려받기를 취소했습니다. 기존 지도는 유지됩니다.");
    return true;
  };
  const auto fail = [&](const QString& message) {
    result.error = message;
    result.status = PreparedReferenceMap::Status::Failed;
  };

  try {
    if (cancelled()) return result;
    if (wgs84Extent.isEmpty() || !wgs84Extent.isFinite() ||
        wgs84Extent.xMinimum() < -180.0 || wgs84Extent.xMaximum() > 180.0 ||
        wgs84Extent.yMinimum() < -90.0 || wgs84Extent.yMaximum() > 90.0) {
      fail(QStringLiteral("DEM을 받을 지도 범위가 올바르지 않습니다. 조사지역으로 이동한 뒤 다시 내려받으세요."));
      return result;
    }
    if (targetBasePath.trimmed().isEmpty() || QFileInfo(targetBasePath).fileName().isEmpty()) {
      fail(QStringLiteral("DEM 저장 위치가 없습니다. 저장 폴더를 선택하고 다시 내려받으세요."));
      return result;
    }

    const int lonFrom = static_cast<int>(std::floor(wgs84Extent.xMinimum()));
    const int lonTo = static_cast<int>(std::ceil(wgs84Extent.xMaximum())) - 1;
    const int latFrom = static_cast<int>(std::floor(wgs84Extent.yMinimum()));
    const int latTo = static_cast<int>(std::ceil(wgs84Extent.yMaximum())) - 1;
    const int tileCount = (lonTo - lonFrom + 1) * (latTo - latFrom + 1);
    if (tileCount <= 0 || tileCount > kMaximumTiles) {
      fail(QStringLiteral("DEM을 받을 범위가 너무 넓습니다. 한 번에 최대 %1칸까지 받을 수 있습니다. 조사지역으로 확대하고 다시 내려받으세요.")
               .arg(kMaximumTiles));
      return result;
    }
    if (!ReferenceMapPreparation::initializeStorage(result, targetBasePath)) return result;
    // This result is a raster, not a GeoPackage. No existing target is opened.
    result.gpkgPath.clear();
    const QString rasterPath = QDir(result.storage->path()).filePath(QStringLiteral("dem.tif"));
    ThreadConfiguration configuration;
    QuietErrors quietErrors;
    std::vector<Dataset> sources;
    std::vector<GDALDatasetH> sourceHandles;
    std::array<double, 6> firstTransform{};
    int missingTiles = 0;
    int openedTiles = 0;

    for (int latitude = latFrom; latitude <= latTo; ++latitude) {
      for (int longitude = lonFrom; longitude <= lonTo; ++longitude) {
        if (cancelled()) return result;
        CPLErrorReset();
        Dataset source(GDALOpenEx(tileUri(latitude, longitude).toUtf8().constData(),
            GDAL_OF_RASTER | GDAL_OF_READONLY, nullptr, nullptr, nullptr));
        const QString openError = QString::fromUtf8(CPLGetLastErrorMsg());
        if (cancelled()) return result;
        if (!source) {
          if (isMissingTile(openError)) {
            ++missingTiles;
          } else {
            fail(QStringLiteral("DEM 서버에 연결하지 못했거나 응답이 늦어 중단했습니다. 인터넷 연결을 확인한 뒤 다시 내려받으세요. 기존 지도는 유지됩니다."));
            return result;
          }
        } else {
          std::array<double, 6> transform{};
          const OGRSpatialReferenceH spatialReference = GDALGetSpatialRef(source.get());
          const char* crsCode = spatialReference ? OSRGetAuthorityCode(spatialReference, nullptr) : nullptr;
          if (GDALGetRasterCount(source.get()) != 1 ||
              GDALGetRasterXSize(source.get()) <= 0 || GDALGetRasterYSize(source.get()) <= 0 ||
              GDALGetGeoTransform(source.get(), transform.data()) != CE_None ||
              !std::all_of(transform.begin(), transform.end(), [](double value) { return std::isfinite(value); }) ||
              GDALGetRasterDataType(GDALGetRasterBand(source.get(), 1)) != GDT_Float32 ||
              !(transform[1] > 0.0) || !(transform[5] < 0.0) ||
              transform[2] != 0.0 || transform[4] != 0.0 ||
              !crsCode || QByteArray(crsCode) != QByteArrayLiteral("4326")) {
            fail(QStringLiteral("DEM 자료의 좌표나 격자 형식이 예상과 다릅니다. 기존 지도는 유지됩니다. 다른 표고 자료를 불러오세요."));
            return result;
          }
          if (sources.empty()) {
            firstTransform = transform;
          } else if (std::abs(transform[1] - firstTransform[1]) > 1e-12 ||
                     std::abs(transform[5] - firstTransform[5]) > 1e-12 ||
                     !gridAligned(transform[0] - firstTransform[0], firstTransform[1]) ||
                     !gridAligned(transform[3] - firstTransform[3], firstTransform[5])) {
            fail(QStringLiteral("이 범위의 DEM 격자 간격이 서로 다릅니다. 원본 정밀도를 유지하려면 범위를 좁혀 각각 내려받으세요."));
            return result;
          }
          sourceHandles.push_back(source.get());
          sources.push_back(std::move(source));
        }
        ++openedTiles;
        if (feedback) feedback->setProgress(20.0 * openedTiles / tileCount);
      }
    }
    if (sources.empty()) {
      fail(QStringLiteral("이 범위에는 제공되는 DEM 자료가 없습니다. 바다를 제외한 조사지역으로 이동하고 다시 내려받으세요."));
      return result;
    }
    if (missingTiles > 0)
      result.warnings.append(QStringLiteral("요청 범위 중 %1칸은 바다이거나 DEM이 제공되지 않아 빈 영역으로 남습니다.").arg(missingTiles));

    CPLStringList vrtArguments;
    vrtArguments.AddString("-resolution");
    vrtArguments.AddString("highest");
    vrtArguments.AddString("-vrtnodata");
    vrtArguments.AddString("nan");
    vrtArguments.AddString("-strict");
    std::unique_ptr<GDALBuildVRTOptions, decltype(&GDALBuildVRTOptionsFree)> vrtOptions(
        GDALBuildVRTOptionsNew(vrtArguments.List(), nullptr), GDALBuildVRTOptionsFree);
    if (!vrtOptions) {
      fail(QStringLiteral("DEM 연결 설정을 만들지 못했습니다. 앱을 다시 실행하고 내려받으세요."));
      return result;
    }
    progress.begin = 20.0;
    progress.span = 5.0;
    GDALBuildVRTOptionsSetProgress(vrtOptions.get(), &Progress::update, &progress);
    int usageError = 0;
    CPLErrorReset();
    Dataset mosaic(GDALBuildVRT("", static_cast<int>(sourceHandles.size()), sourceHandles.data(),
        nullptr, vrtOptions.get(), &usageError));
    if (cancelled()) return result;
    if (!mosaic || usageError || progress.callbackFailed || CPLGetLastErrorType() >= CE_Failure) {
      fail(QStringLiteral("DEM 자료를 하나로 연결하지 못했습니다. 기존 지도는 유지됩니다. 다시 내려받으세요."));
      return result;
    }

    std::array<double, 6> transform{};
    if (GDALGetGeoTransform(mosaic.get(), transform.data()) != CE_None) {
      fail(QStringLiteral("DEM의 지도 범위를 확인하지 못했습니다. 다시 내려받으세요."));
      return result;
    }
    // Integer source pixels preserve source elevations and cell size. The
    // coverage may extend by one original cell at the requested boundary.
    const double width = GDALGetRasterXSize(mosaic.get());
    const double height = GDALGetRasterYSize(mosaic.get());
    const int x0 = static_cast<int>(std::clamp(std::floor((wgs84Extent.xMinimum() - transform[0]) / transform[1]), 0.0, width));
    const int x1 = static_cast<int>(std::clamp(std::ceil((wgs84Extent.xMaximum() - transform[0]) / transform[1]), 0.0, width));
    const int y0 = static_cast<int>(std::clamp(std::floor((wgs84Extent.yMaximum() - transform[3]) / transform[5]), 0.0, height));
    const int y1 = static_cast<int>(std::clamp(std::ceil((wgs84Extent.yMinimum() - transform[3]) / transform[5]), 0.0, height));
    if (x1 <= x0 || y1 <= y0) {
      fail(QStringLiteral("선택 범위에 표고 자료가 없습니다. 조사지역을 확인한 뒤 다시 내려받으세요."));
      return result;
    }
    CPLStringList translateArguments;
    for (const char* option : {"-of", "GTiff", "-co", "TILED=YES", "-co", "COMPRESS=DEFLATE",
                               "-co", "PREDICTOR=3", "-co", "BIGTIFF=IF_SAFER", "-srcwin"})
      translateArguments.AddString(option);
    for (int value : {x0, y0, x1 - x0, y1 - y0})
      translateArguments.AddString(QByteArray::number(value).constData());
    std::unique_ptr<GDALTranslateOptions, decltype(&GDALTranslateOptionsFree)> translateOptions(
        GDALTranslateOptionsNew(translateArguments.List(), nullptr), GDALTranslateOptionsFree);
    if (!translateOptions) {
      fail(QStringLiteral("DEM 저장 설정을 만들지 못했습니다. 다시 내려받으세요."));
      return result;
    }
    progress.begin = 25.0;
    progress.span = 74.0;
    GDALTranslateOptionsSetProgress(translateOptions.get(), &Progress::update, &progress);
    usageError = 0;
    CPLErrorReset();
    Dataset output(GDALTranslate(rasterPath.toUtf8().constData(), mosaic.get(), translateOptions.get(), &usageError));
    if (cancelled()) return result;
    if (!output || usageError || progress.callbackFailed || CPLGetLastErrorType() >= CE_Failure) {
      fail(QStringLiteral("DEM을 끝까지 내려받거나 저장하지 못했습니다. 인터넷 연결과 저장 공간을 확인하세요. 기존 지도는 유지됩니다."));
      return result;
    }
    if (GDALClose(output.release()) != CE_None) {
      fail(QStringLiteral("DEM 파일 저장을 마무리하지 못했습니다. 저장 공간을 확인한 뒤 다시 내려받으세요."));
      return result;
    }
    mosaic.reset();
    sources.clear();
    if (cancelled()) return result;
    Dataset check(GDALOpenEx(rasterPath.toUtf8().constData(), GDAL_OF_RASTER | GDAL_OF_READONLY,
        nullptr, nullptr, nullptr));
    std::array<double, 6> savedTransform{};
    if (!check || GDALGetRasterCount(check.get()) != 1 ||
        GDALGetRasterXSize(check.get()) != x1 - x0 || GDALGetRasterYSize(check.get()) != y1 - y0 ||
        GDALGetGeoTransform(check.get(), savedTransform.data()) != CE_None ||
        std::abs(savedTransform[1] - transform[1]) > 1e-12 ||
        std::abs(savedTransform[5] - transform[5]) > 1e-12 || QFileInfo(rasterPath).size() <= 0) {
      fail(QStringLiteral("저장한 DEM을 다시 확인하지 못했습니다. 기존 지도는 유지됩니다. 다른 저장 폴더에서 다시 내려받으세요."));
      return result;
    }
    if (cancelled()) return result;
    if (feedback) feedback->setProgress(100.0);
    result.rasterUri = rasterPath;
    result.status = PreparedReferenceMap::Status::Ready;
  } catch (...) {
    KaSessionLog::line(QStringLiteral("[except] core/DemDownloadService.cpp:297"));
    fail(QStringLiteral("DEM 내려받기를 완료하지 못했습니다. 인터넷 연결과 저장 공간을 확인한 뒤 다시 시도하세요. 기존 지도는 유지됩니다."));
  }
  return result;
}
