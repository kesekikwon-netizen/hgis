#include "SurveyContourClip.h"
#include "SurveyContourWrite.h"

#include "SurveyContourMath.h"
#include "SurveyContourTin.h"

#include <QDir>
#include <QFile>

#include <algorithm>
#include <cmath>
#include <vector>

#include <cpl_conv.h>
#include <cpl_string.h>
#include <gdal_alg.h>
#include <gdal_priv.h>
#include <ogr_spatialref.h>
#include <ogrsf_frmts.h>

namespace {

constexpr double kNoData = -9999.0;

struct Tick {
  const std::function<bool()>* cancel = nullptr;
  const std::function<void(double)>* progress = nullptr;
  double from = 0;
  double to = 1;
};

int CPL_STDCALL tick(double done, const char*, void* raw) {
  auto* step = static_cast<Tick*>(raw);
  if (step->cancel && *step->cancel && (*step->cancel)()) return 0;
  if (step->progress && *step->progress)
    (*step->progress)((step->from + done * (step->to - step->from)) * 100.0);
  return 1;
}

bool stopped(const std::function<bool()>& cancel) { return cancel && cancel(); }

QString gdalError(const QString& fallback) {
  const char* message = CPLGetLastErrorMsg();
  return (message && message[0]) ? fallback + QLatin1Char(' ') + QString::fromUtf8(message) : fallback;
}

void addField(OGRLayer* layer, const char* name, OGRFieldType type) {
  OGRFieldDefn field(name, type);
  layer->CreateField(&field);
}

void removeOutputs(const QString& dir) {
  QFile::remove(QDir(dir).filePath(QStringLiteral("contours.gpkg")));
  QFile::remove(QDir(dir).filePath(QStringLiteral("contours.gpkg-wal")));
  QFile::remove(QDir(dir).filePath(QStringLiteral("contours.gpkg-shm")));
  QFile::remove(QDir(dir).filePath(QStringLiteral("surface.tif")));
}

// GDALGridCreate stores the first row at dfYMin. A north-up GeoTIFF stores the first row at the north edge.
void flipGridNorthUp(std::vector<float>& grid, int nx, int ny) {
  for (int row = 0; row < ny / 2; ++row) {
    float* south = grid.data() + static_cast<size_t>(row) * static_cast<size_t>(nx);
    float* north = grid.data() + static_cast<size_t>(ny - 1 - row) * static_cast<size_t>(nx);
    std::swap_ranges(south, south + nx, north);
  }
}

bool prepareSrs(OGRSpatialReference* srs, const QString& auth, QString* error) {
  if (srs->importFromEPSG(auth.section(QLatin1Char(':'), 1).toInt()) != OGRERR_NONE) {
    if (error) *error = QStringLiteral("좌표계 %1 을 읽지 못했습니다.").arg(auth);
    return false;
  }
  srs->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
  return true;
}

}  // namespace

SurveyContourResult writeSurveyContourFiles(const QVector<SurveyPoint>& points, const SurveyContourJob& job,
                                            int minCm, int maxCm, double minX, double maxX, double minY,
                                            double maxY, double cell, const std::function<bool()>& cancel,
                                            const std::function<void(double)>& progress) {
  SurveyContourResult result;
  result.minCm = minCm;
  result.maxCm = maxCm;
  result.cellSizeM = cell;
  result.bandIntervalCm = job.bandIntervalCm > 0 ? job.bandIntervalCm
                                                 : SurveyContourBuilder::autoBandIntervalCm(minCm, maxCm);
  const int nx = std::max(2, static_cast<int>(std::ceil((maxX - minX) / cell)));
  const int ny = std::max(2, static_cast<int>(std::ceil((maxY - minY) / cell)));
  if (static_cast<qint64>(nx) * ny > SurveyContourBuilder::kMaxCells) {
    result.error = QStringLiteral("격자 칸이 너무 많습니다. 격자 크기를 키우세요.");
    return result;
  }
  maxX = minX + nx * cell;
  maxY = minY + ny * cell;
  std::vector<double> xs, ys, zs;
  xs.reserve(points.size());
  for (const SurveyPoint& point : points) {
    xs.push_back(point.x);
    ys.push_back(point.y);
    zs.push_back(point.z);
  }
  std::vector<float> grid(static_cast<size_t>(nx) * static_cast<size_t>(ny), static_cast<float>(kNoData));
  Tick step{&cancel, &progress, 0.0, 0.45};
  CPLErrorReset();
  // Linear TIN with radius 0: cells outside the point hull stay NoData.
  GDALGridLinearOptions options{};
  options.nSizeOfStructure = sizeof(options);
  options.dfRadius = 0;
  options.dfNoDataValue = kNoData;
  const CPLErr gridErr = GDALGridCreate(GGA_Linear, &options, static_cast<GUInt32>(points.size()), xs.data(),
                                        ys.data(), zs.data(), minX, maxX, minY, maxY, static_cast<GUInt32>(nx),
                                        static_cast<GUInt32>(ny), GDT_Float32, grid.data(), tick, &step);
  if (stopped(cancel)) {
    result.canceled = true;
    result.error = QStringLiteral("등고선 만들기를 취소했습니다.");
    return result;
  }
  if (gridErr != CE_None) {
    result.error = gdalError(QStringLiteral("표고 격자를 만들지 못했습니다."));
    return result;
  }
  if (job.maxEdgeM > 0)
    result.maskedTriangles = maskLongSurveyTriangles(grid, nx, ny, minX, minY, cell, xs, ys, job.maxEdgeM,
                                                     static_cast<float>(kNoData));
  flipGridNorthUp(grid, nx, ny);

  const QString tifPath = QDir(job.outputDir).filePath(QStringLiteral("surface.tif"));
  const QString gpkgPath = QDir(job.outputDir).filePath(QStringLiteral("contours.gpkg"));
  removeOutputs(job.outputDir);
  GDALDriver* tiff = GetGDALDriverManager()->GetDriverByName("GTiff");
  const char* create[] = {"COMPRESS=DEFLATE", "TILED=YES", "PREDICTOR=3", nullptr};
  GDALDataset* raster = tiff ? tiff->Create(tifPath.toUtf8().constData(), nx, ny, 1, GDT_Float32, create) : nullptr;
  OGRSpatialReference srs;
  QString srsError;
  if (!raster || !prepareSrs(&srs, job.read.crsAuthId, &srsError)) {
    if (raster) GDALClose(raster);
    removeOutputs(job.outputDir);
    result.error = raster ? srsError : gdalError(QStringLiteral("표고 격자 파일을 쓰지 못했습니다."));
    return result;
  }
  double transform[6] = {minX, cell, 0, maxY, 0, -cell};
  raster->SetGeoTransform(transform);
  char* wkt = nullptr;
  srs.exportToWkt(&wkt);
  raster->SetProjection(wkt);
  CPLFree(wkt);
  GDALRasterBand* band = raster->GetRasterBand(1);
  band->SetNoDataValue(kNoData);
  band->RasterIO(GF_Write, 0, 0, nx, ny, grid.data(), nx, ny, GDT_Float32, 0, 0);
  GDALClose(raster);

  GDALDriver* gpkg = GetGDALDriverManager()->GetDriverByName("GPKG");
  GDALDataset* vectors = gpkg ? gpkg->Create(gpkgPath.toUtf8().constData(), 0, 0, 0, GDT_Unknown, nullptr) : nullptr;
  raster = static_cast<GDALDataset*>(GDALOpen(tifPath.toUtf8().constData(), GA_ReadOnly));
  if (!vectors || !raster) {
    if (vectors) GDALClose(vectors);
    if (raster) GDALClose(raster);
    removeOutputs(job.outputDir);
    result.error = QStringLiteral("등고선 파일을 만들지 못했습니다.");
    return result;
  }
  band = raster->GetRasterBand(1);
  OGRLayer* pointLayer = vectors->CreateLayer("survey_points", &srs, wkbPoint25D, nullptr);
  addField(pointLayer, "name", OFTString);
  addField(pointLayer, "elev", OFTReal);
  addField(pointLayer, "elev_cm", OFTInteger);
  for (const SurveyPoint& point : points) {
    if (point.row < 0) continue;  // breakline nodes shape the surface but are not surveyed points
    OGRFeature* feature = OGRFeature::CreateFeature(pointLayer->GetLayerDefn());
    OGRPoint geometry(point.x, point.y, point.z);
    feature->SetGeometry(&geometry);
    feature->SetField("name", point.name.toUtf8().constData());
    const int cm = surveyMetersToCm(point.z);
    feature->SetField("elev", surveyCmToMeters(cm));
    feature->SetField("elev_cm", cm);
    pointLayer->CreateFeature(feature);
    OGRFeature::DestroyFeature(feature);
    ++result.pointCount;
  }

  OGRLayer* lines = vectors->CreateLayer("contour_lines", &srs, wkbLineString, nullptr);
  addField(lines, "id", OFTInteger);
  addField(lines, "elev", OFTReal);
  const int idField = lines->GetLayerDefn()->GetFieldIndex("id");
  const int elevField = lines->GetLayerDefn()->GetFieldIndex("elev");
  const QByteArray interval = QByteArray::number(surveyCmToMeters(job.intervalCm), 'f', 2);
  const QByteArray base = QByteArray::number(surveyCmToMeters(job.baseCm), 'f', 2);
  char** lineOpts = nullptr;
  lineOpts = CSLAddNameValue(lineOpts, "LEVEL_INTERVAL", interval.constData());
  lineOpts = CSLAddNameValue(lineOpts, "LEVEL_BASE", base.constData());
  lineOpts = CSLAddNameValue(lineOpts, "NODATA", "-9999");
  lineOpts = CSLAddNameValue(lineOpts, "ID_FIELD", QByteArray::number(idField).constData());
  lineOpts = CSLAddNameValue(lineOpts, "ELEV_FIELD", QByteArray::number(elevField).constData());
  step = Tick{&cancel, &progress, 0.45, 0.75};
  CPLErrorReset();
  const bool linesOk = GDALContourGenerateEx(band, lines, lineOpts, tick, &step) == CE_None;
  CSLDestroy(lineOpts);
  if (!linesOk) {
    GDALClose(raster);
    GDALClose(vectors);
    removeOutputs(job.outputDir);
    result.canceled = stopped(cancel);
    result.error = result.canceled ? QStringLiteral("등고선 만들기를 취소했습니다.")
                                   : gdalError(QStringLiteral("등고선을 만들지 못했습니다."));
    return result;
  }

  OGRLayer* bands = nullptr;
  if (job.colorBands && result.bandIntervalCm > 0 && maxCm > minCm) {
    bands = vectors->CreateLayer("contour_bands", &srs, wkbMultiPolygon, nullptr);
    addField(bands, "elev_min", OFTReal);
    addField(bands, "elev_max", OFTReal);
    const int classes = surveyClassCount(minCm, maxCm, result.bandIntervalCm);
    QByteArray levels;
    for (int i = 0; i <= classes; ++i) {
      if (i) levels += ',';
      levels += QByteArray::number(surveyCmToMeters(minCm + i * result.bandIntervalCm), 'f', 2);
    }
    const int minField = bands->GetLayerDefn()->GetFieldIndex("elev_min");
    const int maxField = bands->GetLayerDefn()->GetFieldIndex("elev_max");
    char** bandOpts = nullptr;
    bandOpts = CSLAddNameValue(bandOpts, "FIXED_LEVELS", levels.constData());
    bandOpts = CSLAddNameValue(bandOpts, "POLYGONIZE", "YES");
    bandOpts = CSLAddNameValue(bandOpts, "NODATA", "-9999");
    bandOpts = CSLAddNameValue(bandOpts, "ELEV_FIELD_MIN", QByteArray::number(minField).constData());
    bandOpts = CSLAddNameValue(bandOpts, "ELEV_FIELD_MAX", QByteArray::number(maxField).constData());
    step = Tick{&cancel, &progress, 0.75, 0.92};
    if (GDALContourGenerateEx(band, bands, bandOpts, tick, &step) != CE_None && !stopped(cancel))
      result.warning = gdalError(QStringLiteral("색 구간 면을 만들지 못했습니다."));
    CSLDestroy(bandOpts);
  }
  GDALClose(raster);

  addField(lines, "elev_cm", OFTInteger);
  addField(lines, "elev_label", OFTString);
  addField(lines, "index_line", OFTInteger);
  const int decimals = job.intervalCm < 100 ? 1 : 0;
  lines->ResetReading();
  while (OGRFeature* feature = lines->GetNextFeature()) {
    const int cm = surveySnapCm(feature->GetFieldAsDouble(elevField), job.baseCm, job.intervalCm);
    int mod = job.indexEvery > 0 ? ((cm - job.baseCm) / job.intervalCm) % job.indexEvery : 0;
    if (mod < 0) mod += job.indexEvery;
    feature->SetField(elevField, surveyCmToMeters(cm));
    feature->SetField("elev_cm", cm);
    feature->SetField("elev_label", surveyFormatMeters(cm, decimals).toUtf8().constData());
    feature->SetField("index_line", mod == 0 ? 1 : 0);
    lines->SetFeature(feature);
    OGRFeature::DestroyFeature(feature);
    ++result.lineCount;
  }
  if (bands) {
    addField(bands, "elev_min_cm", OFTInteger);
    addField(bands, "elev_max_cm", OFTInteger);
    addField(bands, "elev_mid", OFTReal);
    addField(bands, "legend", OFTString);
    bands->ResetReading();
    while (OGRFeature* feature = bands->GetNextFeature()) {
      const int lo = surveySnapCm(feature->GetFieldAsDouble(bands->GetLayerDefn()->GetFieldIndex("elev_min")), 0,
                                  result.bandIntervalCm);
      const int hi = surveySnapCm(feature->GetFieldAsDouble(bands->GetLayerDefn()->GetFieldIndex("elev_max")), 0,
                                  result.bandIntervalCm);
      feature->SetField("elev_min", surveyCmToMeters(lo));
      feature->SetField("elev_max", surveyCmToMeters(hi));
      feature->SetField("elev_min_cm", lo);
      feature->SetField("elev_max_cm", hi);
      feature->SetField("elev_mid", surveyCmToMeters(lo + (hi - lo) / 2));
      feature->SetField("legend", surveyBandLabel(lo, hi).toUtf8().constData());
      bands->SetFeature(feature);
      OGRFeature::DestroyFeature(feature);
      ++result.bandCount;
    }
  }
  result.linesBeforeClip = result.lineCount;
  if (!job.clipWkt.isEmpty()) {
    bool linesCut = false, bandsCut = true;
    result.lineCount = clipSurveyLayerToWkt(lines, job.clipWkt, &linesCut);
    if (bands) result.bandCount = clipSurveyLayerToWkt(bands, job.clipWkt, &bandsCut);
    result.clipped = linesCut && bandsCut;
  }
  // SurveyContourStyle::reapply reads these back when a replaced result must be shown again.
  vectors->SetMetadataItem("KA_HGIS_MIN_CM", QByteArray::number(minCm).constData());
  vectors->SetMetadataItem("KA_HGIS_MAX_CM", QByteArray::number(maxCm).constData());
  vectors->SetMetadataItem("KA_HGIS_BAND_CM", QByteArray::number(result.bandIntervalCm).constData());
  GDALClose(vectors);
  result.ok = true;
  if (progress) progress(100);
  return result;
}
