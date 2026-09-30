#include "DemPresentation.h"
#include "DemAnalyzer.h"
#include "DemColorRampLegend.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QDateTime>
#include <QPointer>
#include <QTimer>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <gdal.h>
#include <gdal_utils.h>
#include <qgsbilinearrasterresampler.h>
#include <qgscolorrampimpl.h>
#include <qgscolorrampshader.h>
#include <qgscolorramplegendnodesettings.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsrasterbandstats.h>
#include <qgsrasterdataprovider.h>
#include <qgsrasterlayer.h>
#include <qgsmapcanvas.h>
#include <qgscoordinatetransform.h>
#include <qgsrasterresamplefilter.h>
#include <qgsrastershader.h>
#include <qgssinglebandpseudocolorrenderer.h>

namespace {
struct Stop { double z; const char* color; };
constexpr std::array<Stop, 16> kStops = {{{0,"#72c6b5"}, {10,"#81ceaa"},
    {25,"#91d396"}, {50,"#a5d680"}, {75,"#bad975"}, {100,"#d0dc78"},
    {150,"#e0da7b"}, {200,"#ebd27a"}, {300,"#edbe70"}, {400,"#e8a666"},
    {600,"#d98b5a"}, {800,"#c47452"}, {1000,"#ad6650"}, {1300,"#b69886"},
    {1600,"#d1c5bb"}, {2000,"#f4f3ef"}}};
constexpr auto kPreset = "ka_hgis/dem_preset";
constexpr auto kVersion = "ka_hgis/dem_display_version";
}

bool DemPresentation::apply(QgsRasterLayer* layer, const QString& preset,
                            const QgsRectangle& extent) {
  // RGB/XYZ relief pictures contain colors, not metre elevations.
  if (!layer || !layer->isValid() || !layer->dataProvider() || layer->bandCount() != 1 ||
      layer->providerType() != QLatin1String("gdal"))
    return false;
  if (preset != QLatin1String("national") && preset != QLatin1String("lowland") &&
      preset != QLatin1String("viewport")) return false;
  double minimum = 0., maximum = 2000.;
  if (preset == QLatin1String("viewport")) {
    if (extent.isEmpty() || !extent.isFinite()) return false;
    const auto clipped = extent.intersect(layer->extent());
    if (clipped.isEmpty()) return false;
    const auto stats = layer->dataProvider()->bandStatistics(1,
        Qgis::RasterBandStatistic::Min | Qgis::RasterBandStatistic::Max, clipped, 25000);
    if (!std::isfinite(stats.minimumValue) || !std::isfinite(stats.maximumValue) ||
        stats.maximumValue < stats.minimumValue) return false;
    minimum = std::floor(stats.minimumValue);
    maximum = std::ceil(stats.maximumValue);
    if (maximum <= minimum) maximum = minimum + 1.;
  }
  QList<QgsColorRampShader::ColorRampItem> items;
  QgsGradientStopsList gradientStops;
  for (size_t i = 0; i < kStops.size(); ++i) {
    double z = kStops[i].z;
    if (preset == QLatin1String("lowland")) {
      // Half the palette describes 0–100 m, while retaining the national top.
      z = z <= 200. ? z * .5 : 100. + (z - 200.) * (1900. / 1800.);
    } else if (preset == QLatin1String("viewport")) {
      z = minimum + (maximum - minimum) * double(i) / double(kStops.size() - 1);
    }
    const QColor color(QString::fromLatin1(kStops[i].color));
    items.append({z, color, QStringLiteral("%1 m").arg(z, 0, 'f', 0)});
    if (i > 0 && i + 1 < kStops.size())
      gradientStops.append(QgsGradientStop((z - minimum) / (maximum - minimum), color));
  }
  auto ramp = std::make_unique<QgsColorRampShader>(minimum, maximum);
  ramp->setColorRampType(Qgis::ShaderInterpolationMethod::Linear);
  ramp->setClip(false); // Clamp finite outliers to end colors; provider retains NoData.
  ramp->setColorRampItemList(items);
  ramp->setSourceColorRamp(new QgsGradientColorRamp(items.first().color, items.last().color,
                                                  false, gradientStops));
  auto* legend = new QgsColorRampLegendNodeSettings();
  legend->setUseContinuousLegend(true);
  legend->setOrientation(Qt::Vertical);
  legend->setSuffix(QStringLiteral(" m"));
  ramp->setLegendSettings(legend);
  auto* shader = new QgsRasterShader(minimum, maximum);
  shader->setRasterShaderFunction(ramp.release());
  auto* renderer = new QgsSingleBandPseudoColorRenderer(layer->dataProvider(), 1, shader);
  renderer->setClassificationMin(minimum);
  renderer->setClassificationMax(maximum);
  layer->setRenderer(renderer);
  if (auto* resample = layer->resampleFilter()) {
    resample->setZoomedInResampler(new QgsBilinearRasterResampler());
    resample->setZoomedOutResampler(new QgsBilinearRasterResampler());
  }
  if (layer->customProperty(QString::fromLatin1(kVersion)).toInt() != 1)
    layer->setOpacity(1.);
  layer->setCustomProperty(QString::fromLatin1(kPreset), preset);
  layer->setCustomProperty(QString::fromLatin1(kVersion), 1);
  DemColorRampLegend::install(layer);
  layer->triggerRepaint();
  return true;
}

bool DemPresentation::restore(QgsRasterLayer* layer) {
  if (!layer) return false;
  if (layer->customProperty(QString::fromLatin1(kVersion)).toInt() != 1)
    return apply(layer); // Explicit project open migrates the old dark discrete style in memory.
  return DemColorRampLegend::install(layer);
}

void DemPresentation::followCanvas(QgsRasterLayer* layer, QgsMapCanvas* canvas) {
  if (!layer || !canvas || layer->findChild<QTimer*>(QStringLiteral("demViewportTimer"))) return;
  auto* timer = new QTimer(layer);
  timer->setObjectName(QStringLiteral("demViewportTimer"));
  timer->setSingleShot(true);
  timer->setInterval(350);
  const QPointer<QgsMapCanvas> guardedCanvas(canvas);
  QObject::connect(canvas, &QgsMapCanvas::extentsChanged, timer, [timer, layer] {
    if (layer->customProperty(QString::fromLatin1(kPreset)).toString() == QLatin1String("viewport")) timer->start();
  });
  QObject::connect(timer, &QTimer::timeout, layer, [layer, guardedCanvas] {
    if (!guardedCanvas || layer->customProperty(QString::fromLatin1(kPreset)).toString() != QLatin1String("viewport")) return;
    try {
      const QgsCoordinateTransform transform(guardedCanvas->mapSettings().destinationCrs(),
          layer->crs(), guardedCanvas->mapSettings().transformContext());
      const auto extent = transform.transformBoundingBox(guardedCanvas->extent());
      if (!apply(layer, QStringLiteral("viewport"), extent))
        emit guardedCanvas->messageEmitted(QStringLiteral("DEM"), QStringLiteral("현재 화면에 표고 자료가 없어 이전 색띠를 유지합니다."), Qgis::MessageLevel::Warning);
    } catch (const QgsCsException&) {
      emit guardedCanvas->messageEmitted(QStringLiteral("DEM"), QStringLiteral("화면 좌표를 변환하지 못해 이전 색띠를 유지합니다."), Qgis::MessageLevel::Warning);
    }
  });
}

bool DemPresentation::sampleRange(QgsRasterLayer* layer, const QgsRectangle& extent, double* minimum,
                                  double* maximum) {
  if (!layer || !layer->isValid() || layer->providerType() != QLatin1String("gdal") || !minimum || !maximum ||
      extent.isEmpty() || !extent.isFinite())
    return false;
  std::vector<float> z;
  DemAnalyzer::RasterInfo info;
  if (!DemAnalyzer::readFloatWindow(layer->source(), extent.xMinimum(), extent.yMinimum(), extent.xMaximum(),
                                    extent.yMaximum(), 128, &z, &info, nullptr))
    return false;
  bool found = false;
  for (const float value : z) {
    if (!std::isfinite(value) || (info.hasNoData && value == info.noData)) continue;
    *minimum = found ? std::min(*minimum, double(value)) : double(value);
    *maximum = found ? std::max(*maximum, double(value)) : double(value);
    found = true;
  }
  return found;
}

QString DemPresentation::reliefSource(QgsRasterLayer* layer,
    const QgsCoordinateReferenceSystem& workCrs, QString* error) {
  if (error) error->clear();
  if (!layer || !layer->isValid() || !layer->crs().isValid() ||
      layer->providerType() != QLatin1String("gdal")) {
    if (error) *error = QStringLiteral("표고 자료의 좌표계를 확인할 수 없어 음영을 만들지 못했습니다. 좌표계가 있는 DEM을 불러오세요.");
    return {};
  }
  if (layer->crs().mapUnits() == Qgis::DistanceUnit::Meters) return layer->source();
  const auto target = (workCrs.authid() == QLatin1String("EPSG:5186") ||
                       workCrs.authid() == QLatin1String("EPSG:5187"))
      ? workCrs : QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186"));
  const QFileInfo input(layer->source());
  const QByteArray signature = (layer->source() + QLatin1Char('|') +
      QString::number(input.size()) + QLatin1Char('|') +
      QString::number(input.lastModified().toMSecsSinceEpoch()) + QLatin1Char('|') +
      layer->crs().toWkt() + QLatin1Char('|') + target.toWkt()).toUtf8();
  const QString cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
                        QStringLiteral("/dem-relief");
  if (!QDir().mkpath(cache)) {
    if (error) *error = QStringLiteral("음영 임시 폴더를 만들지 못했습니다. 저장 공간과 폴더 쓰기 권한을 확인하세요.");
    return {};
  }
  const QString path = cache + QLatin1Char('/') +
      QString::fromLatin1(QCryptographicHash::hash(signature, QCryptographicHash::Sha256).toHex()) +
      QStringLiteral(".vrt");
  if (QFileInfo::exists(path)) return path;
  GDALAllRegister();
  auto close = [](void* dataset) { if (dataset) GDALClose(dataset); };
  std::unique_ptr<void, decltype(close)> source(GDALOpen(layer->source().toUtf8().constData(), GA_ReadOnly), close);
  QTemporaryFile temporary(cache + QStringLiteral("/warp-XXXXXX.vrt"));
  if (!source || !temporary.open()) {
    if (error) *error = QStringLiteral("음영 계산용 표고 자료를 열지 못했습니다. DEM 파일과 저장 공간을 확인하세요.");
    return {};
  }
  const QString temporaryPath = temporary.fileName();
  temporary.close();
  const QByteArray srcWkt = layer->crs().toWkt().toUtf8();
  const QByteArray dstWkt = target.toWkt().toUtf8();
  const char* arguments[] = {"-of", "VRT", "-s_srs", srcWkt.constData(), "-t_srs",
      dstWkt.constData(), "-r", "bilinear", nullptr};
  std::unique_ptr<GDALWarpAppOptions, decltype(&GDALWarpAppOptionsFree)> options(
      GDALWarpAppOptionsNew(const_cast<char**>(arguments), nullptr), GDALWarpAppOptionsFree);
  GDALDatasetH handle = source.get();
  int usageError = 0;
  std::unique_ptr<void, decltype(close)> warped(
      GDALWarp(temporaryPath.toUtf8().constData(), nullptr, 1, &handle, options.get(), &usageError), close);
  if (!warped || usageError) {
    if (error) *error = QStringLiteral("표고를 작업 좌표계로 변환하지 못해 음영을 만들지 못했습니다. DEM 좌표계를 확인하세요.");
    return {};
  }
  warped.reset();
  // QTemporaryFile may retain its native handle after close() on Windows.
  // Rename through the owning object so it can close/reopen that handle safely.
  if (temporary.rename(path)) {
    temporary.setAutoRemove(false);
    return path;
  }
  if (!QFileInfo::exists(path)) {
    if (error) *error = QStringLiteral("음영 캐시를 저장하지 못했습니다. 저장 공간과 권한을 확인하세요.");
    return {};
  }
  return path;
}
