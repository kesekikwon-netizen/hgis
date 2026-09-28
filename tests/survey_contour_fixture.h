#pragma once

#include "core/SurveyContourBuilder.h"
#include "core/SurveyPointReader.h"

#include <functional>

#include <QCryptographicHash>
#include <QFile>
#include <QPointF>
#include <QVector>

#include <gdal_priv.h>
#include <ogr_geometry.h>
#include <ogrsf_frmts.h>
#include <cpl_error.h>

inline QByteArray surveySha256(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return {};
  return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256);
}

inline QString surveyGdalError(const QString& fallback) {
  const char* message = CPLGetLastErrorMsg();
  return (message && message[0]) ? fallback + QLatin1Char(' ') + QString::fromUtf8(message) : fallback;
}

inline QString writeSurveyTable(const QString& path, const QList<QStringList>& rows) {
  GDALAllRegister();
  CPLErrorReset();
  GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("XLSX");
  if (!driver) return surveyGdalError(QStringLiteral("XLSX 드라이버 없음"));
  GDALDataset* dataset = driver->Create(path.toUtf8().constData(), 0, 0, 0, GDT_Unknown, nullptr);
  if (!dataset) return surveyGdalError(QStringLiteral("XLSX 생성 실패"));
  OGRLayer* layer = dataset->CreateLayer("survey", nullptr, wkbNone, nullptr);
  if (!layer) {
    GDALClose(dataset);
    return surveyGdalError(QStringLiteral("XLSX 시트 실패"));
  }
  for (const QString& name : rows.first()) {
    OGRFieldDefn field(name.toUtf8().constData(), OFTString);
    if (layer->CreateField(&field) != OGRERR_NONE) {
      GDALClose(dataset);
      return surveyGdalError(QStringLiteral("칸 생성 실패 %1").arg(name));
    }
  }
  for (int r = 1; r < rows.size(); ++r) {
    OGRFeature* feature = OGRFeature::CreateFeature(layer->GetLayerDefn());
    for (int c = 0; c < rows.at(r).size(); ++c) feature->SetField(c, rows.at(r).at(c).toUtf8().constData());
    const OGRErr err = layer->CreateFeature(feature);
    OGRFeature::DestroyFeature(feature);
    if (err != OGRERR_NONE) {
      GDALClose(dataset);
      return surveyGdalError(QStringLiteral("행 기록 실패"));
    }
  }
  GDALClose(dataset);
  return {};
}

inline QString writeSurveyDxf(const QString& path) {
  GDALAllRegister();
  CPLErrorReset();
  GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("DXF");
  if (!driver) return surveyGdalError(QStringLiteral("DXF 드라이버 없음"));
  GDALDataset* dataset = driver->Create(path.toUtf8().constData(), 0, 0, 0, GDT_Unknown, nullptr);
  if (!dataset) return surveyGdalError(QStringLiteral("DXF 생성 실패"));
  OGRLayer* layer = dataset->CreateLayer("entities", nullptr, wkbPoint25D, nullptr);
  if (!layer) {
    GDALClose(dataset);
    return surveyGdalError(QStringLiteral("DXF 레이어 실패"));
  }
  if (layer->GetLayerDefn()->GetFieldIndex("Layer") < 0) {
    OGRFieldDefn field("Layer", OFTString);
    if (layer->CreateField(&field) != OGRERR_NONE) {
      GDALClose(dataset);
      return surveyGdalError(QStringLiteral("DXF Layer 칸 실패"));
    }
  }
  for (int i = 0; i < 20; ++i) {
    OGRFeature* feature = OGRFeature::CreateFeature(layer->GetLayerDefn());
    OGRPoint point(i, i < 10 ? 0 : 5, i < 10 ? 10.0 + i * 0.1 : 30.0);
    feature->SetGeometry(&point);
    feature->SetField("Layer", i < 10 ? "A" : "B");
    const OGRErr err = layer->CreateFeature(feature);
    OGRFeature::DestroyFeature(feature);
    if (err != OGRERR_NONE) {
      GDALClose(dataset);
      return surveyGdalError(QStringLiteral("DXF 점 기록 실패"));
    }
  }
  GDALClose(dataset);
  return {};
}

inline QVector<SurveyPoint> surveyGrid(int xCount, int yCount, double step, double zAt) {
  QVector<SurveyPoint> points;
  for (int ix = 0; ix < xCount; ++ix) {
    for (int iy = 0; iy < yCount; ++iy) {
      SurveyPoint point;
      point.x = ix * step;
      point.y = iy * step;
      point.z = zAt + 0.01 * point.x;
      points.push_back(point);
    }
  }
  return points;
}

inline SurveyContourResult buildSurvey(const QVector<SurveyPoint>& points, const QString& dir, int intervalCm,
                                       int bandCm, double cell) {
  SurveyContourJob job;
  job.outputDir = dir;
  job.intervalCm = intervalCm;
  job.bandIntervalCm = bandCm;
  job.cellSizeM = cell;
  job.colorBands = bandCm > 0;
  job.read.crsAuthId = QStringLiteral("EPSG:5186");
  return SurveyContourBuilder::build(points, job);
}

inline void eachSurveyGeometry(const QString& gpkg, const char* table, const std::function<void(OGRFeature*)>& visit) {
  GDALDataset* dataset = static_cast<GDALDataset*>(
      GDALOpenEx(gpkg.toUtf8().constData(), GDAL_OF_VECTOR | GDAL_OF_READONLY, nullptr, nullptr, nullptr));
  if (!dataset) return;
  OGRLayer* layer = dataset->GetLayerByName(table);
  if (!layer) {
    GDALClose(dataset);
    return;
  }
  layer->ResetReading();
  while (OGRFeature* feature = layer->GetNextFeature()) {
    visit(feature);
    OGRFeature::DestroyFeature(feature);
  }
  GDALClose(dataset);
}

inline void collectSurveyVertices(OGRGeometry* geometry, QVector<QPointF>* vertices) {
  if (!geometry || !vertices) return;
  const OGRwkbGeometryType kind = wkbFlatten(geometry->getGeometryType());
  if (kind == wkbLineString || kind == wkbLinearRing) {
    OGRLineString* line = geometry->toLineString();
    if (!line) return;
    for (int i = 0; i < line->getNumPoints(); ++i) vertices->push_back(QPointF(line->getX(i), line->getY(i)));
    return;
  }
  if (kind == wkbPolygon) {
    OGRPolygon* polygon = geometry->toPolygon();
    if (polygon && polygon->getExteriorRing()) collectSurveyVertices(polygon->getExteriorRing(), vertices);
    return;
  }
  if (kind == wkbMultiPolygon || kind == wkbMultiLineString || kind == wkbGeometryCollection) {
    OGRGeometryCollection* multi = geometry->toGeometryCollection();
    if (!multi) return;
    for (int i = 0; i < multi->getNumGeometries(); ++i) collectSurveyVertices(multi->getGeometryRef(i), vertices);
  }
}
