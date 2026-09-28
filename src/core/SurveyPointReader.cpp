#include "SurveyPointReader.h"

#include "SurveyContourMath.h"
#include "SurveyPointSchema.h"
#include "LayerOps.h"

#include <QHash>
#include <QSet>

#include <cmath>

#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <qgsrectangle.h>

namespace {

struct Cell {
  qint64 x = 0;
  qint64 y = 0;
  bool operator==(const Cell& other) const { return x == other.x && y == other.y; }
};
size_t qHash(const Cell& cell, size_t seed = 0) {
  return ::qHash(cell.x, seed) ^ ::qHash(cell.y, seed + 1);
}

OGRPoint* asPoint(OGRGeometry* geometry) {
  if (!geometry) return nullptr;
  if (OGRPoint* point = geometry->toPoint()) return point;
  if (wkbFlatten(geometry->getGeometryType()) == wkbMultiPoint &&
      geometry->toGeometryCollection()->getNumGeometries() > 0)
    return geometry->toGeometryCollection()->getGeometryRef(0)->toPoint();
  return nullptr;
}

}  // namespace

SurveySourceInfo SurveyPointReader::inspect(const QString& path) {
  SurveySourceInfo info;
  SurveyDataset data;
  if (!data.open(path)) {
    info.fatal = QStringLiteral("파일을 열지 못했습니다. DXF·엑셀·CSV인지 확인하세요.");
    return info;
  }
  info.dxf = QString::fromUtf8(data.ds->GetDriver()->GetDescription()) == QLatin1String("DXF");
  if (info.dxf) {
    QSet<QString> layers;
    if (OGRLayer* layer = data.ds->GetLayer(0)) {
      layer->ResetReading();
      while (OGRFeature* feature = layer->GetNextFeature()) {
        if (surveyIsPoint(feature->GetGeometryRef())) {
          const int index = feature->GetFieldIndex("Layer");
          const QString name = index >= 0 ? QString::fromUtf8(feature->GetFieldAsString(index))
                                          : QString::fromUtf8(layer->GetName());
          if (!name.isEmpty()) layers.insert(name);
        }
        OGRFeature::DestroyFeature(feature);
      }
    }
    info.layers = QStringList(layers.begin(), layers.end());
    info.layers.sort();
    return info;
  }
  for (int i = 0; i < data.ds->GetLayerCount(); ++i)
    info.layers << QString::fromUtf8(data.ds->GetLayer(i)->GetName());
  if (OGRLayer* first = data.ds->GetLayer(0)) {
    OGRFeatureDefn* definition = first->GetLayerDefn();
    for (int i = 0; i < definition->GetFieldCount(); ++i)
      info.fields << QString::fromUtf8(definition->GetFieldDefn(i)->GetNameRef());
  }
  return info;
}

SurveyReadReport SurveyPointReader::read(const SurveyReadOptions& options) {
  SurveyReadReport report;
  SurveyDataset data;
  if (!data.open(options.path)) {
    report.fatal = QStringLiteral("파일을 열지 못했습니다. 다른 프로그램이 파일을 열고 있는지 확인하세요.");
    return report;
  }
  const bool dxf = QString::fromUtf8(data.ds->GetDriver()->GetDescription()) == QLatin1String("DXF");
  OGRLayer* layer = dxf ? data.ds->GetLayer(0) : nullptr;
  if (!dxf && !options.layer.isEmpty())
    layer = data.ds->GetLayerByName(options.layer.toUtf8().constData());
  if (!layer) {
    for (int i = 0; i < data.ds->GetLayerCount(); ++i) {
      OGRLayer* candidate = data.ds->GetLayer(i);
      if (candidate && candidate->GetLayerDefn()->GetFieldCount() >= 3) {
        layer = candidate;
        break;
      }
    }
  }
  if (!layer) layer = data.ds->GetLayer(0);
  if (!layer) {
    report.fatal = QStringLiteral("시트나 CAD 레이어가 없습니다.");
    return report;
  }
  OGRFeatureDefn* definition = layer->GetLayerDefn();
  SurveyColumns columns = guessSurveyColumns(definition);
  if (options.idColumn >= 0) columns.id = options.idColumn;
  if (options.xColumn >= 0) columns.x = options.xColumn;
  if (options.yColumn >= 0) columns.y = options.yColumn;
  if (options.zColumn >= 0) columns.z = options.zColumn;
  report.idColumn = columns.id;
  report.xColumn = columns.x;
  report.yColumn = columns.y;
  report.zColumn = columns.z;
  for (int i = 0; i < definition->GetFieldCount(); ++i)
    report.fields << QString::fromUtf8(definition->GetFieldDefn(i)->GetNameRef());
  if (!dxf && (columns.x < 0 || columns.y < 0 || columns.z < 0)) {
    report.fatal = QStringLiteral("X·Y·표고 칸을 찾지 못했습니다. 칸을 직접 지정하세요.");
    return report;
  }
  if (!dxf && surveyAxesNamedNorthEast(definition, columns)) report.swapSuggested = true;

  QHash<QString, bool> cadHasElevation;
  if (dxf && options.layer.isEmpty()) {
    layer->ResetReading();
    while (OGRFeature* feature = layer->GetNextFeature()) {
      const int layerField = feature->GetFieldIndex("Layer");
      const QString cad = layerField >= 0 ? QString::fromUtf8(feature->GetFieldAsString(layerField)) : QString();
      OGRPoint* geometry = asPoint(feature->GetGeometryRef());
      const bool elevated = geometry && std::isfinite(geometry->getZ()) && geometry->getZ() != 0.0;
      cadHasElevation.insert(cad, cadHasElevation.value(cad) || elevated);
      OGRFeature::DestroyFeature(feature);
    }
  }
  bool someLayerHasElevation = false;
  for (bool elevated : cadHasElevation) someLayerHasElevation = someLayerHasElevation || elevated;

  struct Bucket { double zSum = 0; int count = 0; SurveyPoint point; bool zDiffers = false; };
  QHash<Cell, Bucket> buckets;
  int nonPoints = 0;
  layer->ResetReading();
  for (int index = 1; OGRFeature* feature = layer->GetNextFeature(); ++index) {
    const int row = data.headerless || dxf ? index : index + 1;
    if (dxf && !options.layer.isEmpty()) {
      const int layerField = feature->GetFieldIndex("Layer");
      const QString cad = layerField >= 0 ? QString::fromUtf8(feature->GetFieldAsString(layerField)) : QString();
      if (cad != options.layer) {
        OGRFeature::DestroyFeature(feature);
        continue;
      }
    }
    if (dxf && options.layer.isEmpty() && someLayerHasElevation) {
      const int layerField = feature->GetFieldIndex("Layer");
      const QString cad = layerField >= 0 ? QString::fromUtf8(feature->GetFieldAsString(layerField)) : QString();
      if (!cadHasElevation.value(cad)) {
        OGRFeature::DestroyFeature(feature);
        continue;
      }
    }
    double fileX = 0, fileY = 0, z = 0;
    QString name;
    if (dxf) {
      if (!surveyIsPoint(feature->GetGeometryRef())) {
        ++nonPoints;
        OGRFeature::DestroyFeature(feature);
        continue;
      }
      OGRPoint* geometry = asPoint(feature->GetGeometryRef());
      if (!geometry || !std::isfinite(geometry->getX()) || !std::isfinite(geometry->getZ())) {
        ++report.skipped;
        report.issues << QStringLiteral("%1행: 점 좌표가 없습니다.").arg(row);
        OGRFeature::DestroyFeature(feature);
        continue;
      }
      fileX = geometry->getX();
      fileY = geometry->getY();
      z = geometry->getZ();
      name = QString::number(index);
    } else {
      const QString xs = surveyFieldText(feature, columns.x);
      const QString ys = surveyFieldText(feature, columns.y);
      const QString zs = surveyFieldText(feature, columns.z);
      if (xs.isEmpty() && ys.isEmpty() && zs.isEmpty()) {
        ++report.skipped;
        report.issues << QStringLiteral("%1행: 빈 행").arg(row);
        OGRFeature::DestroyFeature(feature);
        continue;
      }
      if (!surveyParseNumber(xs, &fileX) || !surveyParseNumber(ys, &fileY)) {
        ++report.skipped;
        report.issues << QStringLiteral("%1행: X·Y가 숫자가 아닙니다.").arg(row);
        OGRFeature::DestroyFeature(feature);
        continue;
      }
      if (!surveyParseNumber(zs, &z)) {
        ++report.skipped;
        report.issues << QStringLiteral("%1행: 표고가 숫자가 아닙니다.").arg(row);
        OGRFeature::DestroyFeature(feature);
        continue;
      }
      name = surveyFieldText(feature, columns.id);
    }
    OGRFeature::DestroyFeature(feature);
    SurveyPoint point;
    point.x = options.swapAxes ? fileY : fileX;
    point.y = options.swapAxes ? fileX : fileY;
    point.z = z;
    point.name = name;
    point.row = row;
    point.suspicious = z == 0.0 || std::abs(z) >= 999.0;
    if (report.rawCount == 0) {
      const QgsRectangle korea = LayerOps::koreaExtentForCrs(options.crsAuthId);
      if (!korea.isEmpty() && korea.isFinite()) {
        const bool swapped = korea.contains(QgsPointXY(fileY, fileX));
        const bool xIsNorthing = std::abs(fileX - 200000.0) > std::abs(fileY - 200000.0) + 50000.0;
        if (swapped && xIsNorthing) report.swapSuggested = true;
      }
    }
    ++report.rawCount;
    Bucket& bucket = buckets[Cell{qRound64(point.x * 1000.0), qRound64(point.y * 1000.0)}];
    if (bucket.count > 0 && std::abs(bucket.point.z - z) > 0.001) bucket.zDiffers = true;
    if (bucket.count == 0) bucket.point = point;
    bucket.point.suspicious = bucket.point.suspicious || point.suspicious;
    bucket.zSum += z;
    ++bucket.count;
  }
  if (nonPoints > 0)
    report.issues << QStringLiteral("점이 아닌 도형 %1개는 건너뛰었습니다.").arg(nonPoints);
  if (someLayerHasElevation) {
    QStringList dropped;
    for (auto it = cadHasElevation.cbegin(); it != cadHasElevation.cend(); ++it)
      if (!it.value() && !it.key().isEmpty()) dropped << it.key();
    if (!dropped.isEmpty())
      report.issues << QStringLiteral("높이가 없는 점 이름 레이어는 빼었습니다: %1").arg(dropped.join(QStringLiteral(", ")));
  }
  for (auto it = buckets.cbegin(); it != buckets.cend(); ++it) {
    SurveyPoint point = it.value().point;
    point.z = it.value().zSum / it.value().count;
    if (it.value().count > 1 && it.value().zDiffers) {
      ++report.duplicateGroups;
      report.issues << QStringLiteral("같은 좌표(%1, %2)의 표고 %3개를 평균했습니다.")
                           .arg(point.x, 0, 'f', 3)
                           .arg(point.y, 0, 'f', 3)
                           .arg(it.value().count);
    }
    report.points.push_back(point);
  }
  if (!report.points.isEmpty()) {
    report.minCm = surveyMetersToCm(report.points.first().z);
    report.maxCm = report.minCm;
    const QgsRectangle korea = LayerOps::koreaExtentForCrs(options.crsAuthId);
    for (const SurveyPoint& point : report.points) {
      const int cm = surveyMetersToCm(point.z);
      report.minCm = std::min(report.minCm, cm);
      report.maxCm = std::max(report.maxCm, cm);
      if (!korea.isEmpty() && !korea.contains(QgsPointXY(point.x, point.y)))
        report.outsideKorea = true;
    }
  }
  return report;
}
