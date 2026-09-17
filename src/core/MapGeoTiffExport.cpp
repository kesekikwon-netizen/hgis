#include "MapGeoTiffExport.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSaveFile>
#include <QTemporaryFile>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <gdal.h>
#include <memory>
#include <qgsmaprendererparalleljob.h>
#include <qgsmapsettings.h>
#include <qgsproject.h>
#include <qgsproviderregistry.h>
#include <qgsrasterlayer.h>

namespace {
bool fail(QString* error, const QString& message) {
  if (error) *error = message;
  return false;
}

bool finiteExtent(const QgsRectangle& extent) {
  return std::isfinite(extent.xMinimum()) && std::isfinite(extent.yMinimum()) &&
         std::isfinite(extent.xMaximum()) && std::isfinite(extent.yMaximum()) &&
         std::isfinite(extent.width()) && std::isfinite(extent.height()) &&
         extent.width() > 0 && extent.height() > 0;
}

QString canonicalPath(const QString& path) {
  const QFileInfo info(path);
  const QString canonical = info.canonicalFilePath();
  return QDir::cleanPath(canonical.isEmpty() ? info.absoluteFilePath() : canonical);
}

bool isSourceRaster(const QgsMapSettings& settings, const QString& path) {
  QList<QgsMapLayer*> layers = settings.layers();
  // A hidden raster in the open survey must also survive an export.
  layers.append(QgsProject::instance()->mapLayers().values());
  const QString target = canonicalPath(path);
  for (QgsMapLayer* layer : layers) {
    if (!qobject_cast<QgsRasterLayer*>(layer)) continue;
    const QVariantMap parts = QgsProviderRegistry::instance()->decodeUri(layer->providerType(), layer->source());
    const QString source = parts.value(QStringLiteral("path"), layer->source()).toString();
    if (QFileInfo(source).isFile() && canonicalPath(source).compare(target, Qt::CaseInsensitive) == 0)
      return true;
  }
  return false;
}
}

bool MapGeoTiffExport::write(const QgsMapSettings& snapshot, const QString& path,
                           QString* error, std::function<bool()> canceled) {
  if (error) error->clear();
  const auto isCanceled = [&]() { return canceled && canceled(); };
  const auto cancel = [&]() { return fail(error, QStringLiteral("GeoTIFF 내보내기를 취소했습니다.")); };
  if (isCanceled()) return cancel();
  QgsMapSettings settings(snapshot);
  settings.setDevicePixelRatio(1);
  settings.setFlag(Qgis::MapSettingsFlag::RenderPartialOutput, false);
  const QSize size = settings.outputSize();
  if (!settings.destinationCrs().isValid() || !finiteExtent(settings.extent()) ||
      !finiteExtent(settings.visibleExtent()) || !std::isfinite(settings.rotation()))
    return fail(error, QStringLiteral("현재 지도의 좌표계 또는 표시 범위를 확인해 주세요."));
  if (size.width() <= 0 || size.height() <= 0 || size.width() > 8192 || size.height() > 8192 ||
      qint64(size.width()) * size.height() > 32000000)
    return fail(error, QStringLiteral("지도 영상은 한 변 8,192픽셀, 총 3,200만 픽셀 이내여야 합니다."));
  if (settings.layers().isEmpty())
    return fail(error, QStringLiteral("현재 지도에 표시할 레이어가 없습니다."));
  for (QgsMapLayer* layer : settings.layers()) {
    if (!layer || !layer->isValid())
      return fail(error, QStringLiteral("표시 중인 레이어를 읽을 수 없습니다. 지도 레이어를 확인해 주세요."));
  }
  if (path.trimmed().isEmpty() || !QFileInfo(path).dir().exists())
    return fail(error, QStringLiteral("GeoTIFF를 저장할 폴더를 확인해 주세요."));
  if (isSourceRaster(settings, path))
    return fail(error, QStringLiteral("현재 조사의 원본 영상은 덮어쓸 수 없습니다. 다른 파일 이름을 지정해 주세요."));

  QgsMapRendererParallelJob job(settings);
  QEventLoop loop;
  QTimer cancellationTimer;
  bool wasCanceled = false;
  QObject::connect(&job, &QgsMapRendererJob::finished, &loop, &QEventLoop::quit);
  QObject::connect(&cancellationTimer, &QTimer::timeout, &loop, [&]() {
    if (isCanceled()) {
      wasCanceled = true;
      job.cancelWithoutBlocking();
    }
  });
  cancellationTimer.start(50);
  job.start();
  if (job.isActive()) loop.exec();
  cancellationTimer.stop();
  if (wasCanceled || isCanceled()) return cancel();
  if (!job.errors().isEmpty())
    return fail(error, QStringLiteral("지도 일부를 그리지 못해 저장하지 않았습니다. 배경 지도 연결과 레이어 상태를 확인해 주세요."));
  const QImage image = job.renderedImage().convertToFormat(QImage::Format_RGB888);
  if (image.isNull() || image.size() != size)
    return fail(error, QStringLiteral("지도 영상을 만들지 못했습니다."));

  // The map-to-pixel inverse includes the actual fitted extent and any canvas rotation.
  // GeoTIFF's origin is the upper-left pixel corner, not its center.
  const QgsPointXY origin = settings.mapToPixel().toMapCoordinates(0, 0);
  const QgsPointXY right = settings.mapToPixel().toMapCoordinates(1, 0);
  const QgsPointXY down = settings.mapToPixel().toMapCoordinates(0, 1);
  double transform[6] = {origin.x(), right.x() - origin.x(), down.x() - origin.x(),
                         origin.y(), right.y() - origin.y(), down.y() - origin.y()};
  for (double value : transform) {
    if (!std::isfinite(value))
      return fail(error, QStringLiteral("현재 지도의 좌표 변환을 계산하지 못했습니다."));
  }
  if (transform[1] * transform[5] - transform[2] * transform[4] == 0)
    return fail(error, QStringLiteral("현재 지도의 픽셀 좌표 변환이 유효하지 않습니다."));

  QTemporaryFile temporary(QFileInfo(path).dir().filePath(QStringLiteral(".ka-geotiff-XXXXXX.tif")));
  if (!temporary.open()) return fail(error, QStringLiteral("저장 폴더에 임시 파일을 만들 수 없습니다."));
  temporary.close();
  GDALAllRegister();
  GDALDriverH driver = GDALGetDriverByName("GTiff");
  if (!driver) return fail(error, QStringLiteral("GeoTIFF 저장 드라이버를 찾을 수 없습니다."));
  char compression[] = "COMPRESS=NONE";
  char classic[] = "BIGTIFF=NO";
  char photometric[] = "PHOTOMETRIC=RGB";
  char interleave[] = "INTERLEAVE=PIXEL";
  char* options[] = {compression, classic, photometric, interleave, nullptr};
  using Dataset = std::unique_ptr<void, decltype(&GDALClose)>;
  Dataset dataset(GDALCreate(driver, temporary.fileName().toUtf8().constData(), size.width(),
                            size.height(), 3, GDT_Byte, options), GDALClose);
  if (!dataset) return fail(error, QStringLiteral("GeoTIFF 파일을 만들지 못했습니다."));
  const QByteArray wkt = settings.destinationCrs().toWkt().toUtf8();
  if (GDALSetGeoTransform(dataset.get(), transform) != CE_None ||
      GDALSetProjection(dataset.get(), wkt.constData()) != CE_None)
    return fail(error, QStringLiteral("GeoTIFF 좌표계 정보를 저장하지 못했습니다."));
  int bands[] = {1, 2, 3};
  for (int row = 0; row < size.height(); row += 64) {
    QCoreApplication::processEvents();
    if (isCanceled()) return cancel();
    const int rows = std::min(64, size.height() - row);
    if (GDALDatasetRasterIO(dataset.get(), GF_Write, 0, row, size.width(), rows,
                            const_cast<uchar*>(image.constScanLine(row)), size.width(), rows,
                            GDT_Byte, 3, bands, 3, image.bytesPerLine(), 1) != CE_None)
      return fail(error, QStringLiteral("GeoTIFF 영상 데이터를 저장하지 못했습니다."));
  }
  if (GDALFlushCache(dataset.get()) != CE_None)
    return fail(error, QStringLiteral("GeoTIFF 파일 기록을 완료하지 못했습니다."));
  if (GDALClose(dataset.release()) != CE_None)
    return fail(error, QStringLiteral("GeoTIFF 파일 기록을 마무리하지 못했습니다."));

  QFile input(temporary.fileName());
  QSaveFile output(path);
  // QSaveFile keeps an existing destination intact on write errors and cancellation.
  output.setDirectWriteFallback(false);
  if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly))
    return fail(error, QStringLiteral("GeoTIFF 저장 파일을 열 수 없습니다."));
  while (!input.atEnd()) {
    QCoreApplication::processEvents();
    if (isCanceled()) return cancel();
    const QByteArray chunk = input.read(1024 * 1024);
    if (chunk.isEmpty() || output.write(chunk) != chunk.size())
      return fail(error, QStringLiteral("GeoTIFF를 저장하지 못했습니다. 저장 공간을 확인해 주세요."));
  }
  if (isCanceled()) return cancel();
  if (!output.commit())
    return fail(error, QStringLiteral("GeoTIFF 파일을 확정하지 못했습니다. 파일 사용 여부와 저장 권한을 확인해 주세요."));
  return true;
}
