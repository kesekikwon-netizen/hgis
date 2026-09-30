#pragma once

#include <QString>
#include <cstdint>
#include <vector>

// GDAL DEM hillshade / slope. Overlay only — not survey domain, not export 5179.
namespace DemAnalyzer {

enum class HillshadeMode { Single, Multi };

struct Options {
  double azimuthDeg = 315.0;    // light, clockwise from north (GDAL gdaldem)
  double altitudeDeg = 45.0;
  double zFactor = 1.0;
  HillshadeMode hillshade = HillshadeMode::Multi;
};

struct RasterInfo {
  int width = 0;
  int height = 0;
  double geotransform[6] = {0, 1, 0, 0, 0, -1};
  QString projectionWkt;
  float noData = -9999.0f;
  bool hasNoData = false;
};

bool readFloatBand(const QString& tifPath, std::vector<float>* out, RasterInfo* info,
                   QString* errorOut);
// 세계좌표 상자만 읽는다. maxEdge>1이면 긴 변을 그 픽셀 이하로 줄인다.
bool readFloatWindow(const QString& tifPath, double xMin, double yMin, double xMax, double yMax,
                     int maxEdge, std::vector<float>* out, RasterInfo* info, QString* errorOut);

void hillshadeHorn(const std::vector<float>& z, const RasterInfo& info, const Options& opt,
                   std::vector<std::uint8_t>* outGray);

void slopeDegrees(const std::vector<float>& z, const RasterInfo& info, double zFactor,
                  std::vector<float>* outDeg);

// Writes a Byte GeoTIFF (same grid as source) for QgsRasterLayer overlay. 0 is NoData.
bool writeByteGeoTiff(const QString& outPath, const RasterInfo& info,
                      const std::vector<std::uint8_t>& gray, QString* errorOut);

// Hillshade of band 1 as a Byte GeoTIFF. Like gdaldem, 0 is NoData only (edges, source NoData
// cells and their neighbours); a fully shaded cell is written as 1 so it stays visible. The DEM is processed
// in strips of rows, never loaded whole. stripRows <= 0 picks a strip of about 4 million cells.
bool runHillshadeFile(const QString& demPath, const QString& outPath, const Options& opt,
                      QString* errorOut, int stripRows = 0);

// Where a new hillshade goes: the survey's 지형분석 folder when a survey is open, else next to
// the DEM. Never an existing file; a numbered name is chosen instead of overwriting.
QString hillshadeOutputPath(const QString& demPath, const QString& surveyPath);

}  // namespace DemAnalyzer
