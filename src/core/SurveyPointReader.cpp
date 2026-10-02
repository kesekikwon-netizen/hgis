#include "SurveyPointReader.h"

#include "SurveyPointArrange.h"
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

// OGRGeometry::toPoint() is an unchecked cast, so the type is tested first.
OGRPoint* asPoint(OGRGeometry* geometry) {
  if (!geometry) return nullptr;
  const OGRwkbGeometryType kind = wkbFlatten(geometry->getGeometryType());
  if (kind == wkbPoint) return geometry->toPoint();
  if (kind == wkbMultiPoint && geometry->toGeometryCollection()->getNumGeometries() > 0) {
    OGRGeometry* first = geometry->toGeometryCollection()->getGeometryRef(0);
    return first && wkbFlatten(first->getGeometryType()) == wkbPoint ? first->toPoint() : nullptr;
  }
  return nullptr;
}

QString cadLayer(OGRFeature* feature) {
  const int index = feature->GetFieldIndex("Layer");
  return index >= 0 ? QString::fromUtf8(feature->GetFieldAsString(index)) : QString();
}

// Keeps lines whose every vertex has a height; those can steer the surface as breaklines.
bool collectLine(OGRGeometry* geometry, SurveyReadReport* report) {
  if (!geometry) return false;
  const OGRwkbGeometryType kind = wkbFlatten(geometry->getGeometryType());
  if (kind == wkbMultiLineString) {
    OGRGeometryCollection* parts = geometry->toGeometryCollection();
    for (int i = 0; i < parts->getNumGeometries(); ++i) collectLine(parts->getGeometryRef(i), report);
    return true;
  }
  if (kind != wkbLineString) return false;
  OGRLineString* line = geometry->toLineString();
  SurveyPolyline vertices;
  bool allElevated = line->getNumPoints() >= 2;
  for (int i = 0; i < line->getNumPoints(); ++i) {
    SurveyPoint vertex;
    vertex.x = line->getX(i);
    vertex.y = line->getY(i);
    vertex.z = line->getZ(i);
    vertex.row = -1;
    if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) || !std::isfinite(vertex.z) || vertex.z == 0.0)
      allElevated = false;
    vertices.push_back(vertex);
  }
  if (allElevated) report->breaklines.push_back(vertices);
  else ++report->flatLineCount;
  return true;
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
          const QString name = feature->GetFieldIndex("Layer") >= 0 ? cadLayer(feature)
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
  report.crsAuthId = options.crsAuthId;
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
  if (!dxf && surveyAxesNamedNorthEast(definition, columns)) {
    report.swapSuggested = true;
    report.swapReason = QStringLiteral("열 이름에 북·동 표시가 있음");
  }
  auto note = [&report](const QString& text) {
    if (report.issues.size() < kMaxIssues) report.issues << text;
    else ++report.issuesOmitted;
  };

  // Only point entities decide which CAD layers carry heights; line-only layers are not "dropped".
  QHash<QString, bool> cadHasElevation;
  if (dxf && options.layer.isEmpty()) {
    layer->ResetReading();
    while (OGRFeature* feature = layer->GetNextFeature()) {
      if (OGRPoint* geometry = asPoint(feature->GetGeometryRef())) {
        const bool elevated = std::isfinite(geometry->getZ()) && geometry->getZ() != 0.0;
        const QString cad = cadLayer(feature);
        cadHasElevation.insert(cad, cadHasElevation.value(cad) || elevated);
      }
      OGRFeature::DestroyFeature(feature);
    }
  }
  bool someLayerHasElevation = false;
  for (bool elevated : cadHasElevation) someLayerHasElevation = someLayerHasElevation || elevated;
  const QgsRectangle fileKorea = LayerOps::koreaExtentForCrs(
      options.sourceCrsAuthId.isEmpty() ? options.crsAuthId : options.sourceCrsAuthId);

  struct Bucket { double zSum = 0; int count = 0; SurveyPoint point; bool zDiffers = false; };
  QHash<Cell, Bucket> buckets;
  layer->ResetReading();
  for (int index = 1; OGRFeature* feature = layer->GetNextFeature(); ++index) {
    const int row = data.headerless || dxf ? index : index + 1;
    if (dxf && !surveyIsPoint(feature->GetGeometryRef())) {
      // Lines are kept from every CAD layer: breaklines usually sit on a layer of their own.
      if (!collectLine(feature->GetGeometryRef(), &report)) ++report.nonPointCount;
      OGRFeature::DestroyFeature(feature);
      continue;
    }
    const QString cad = dxf ? cadLayer(feature) : QString();
    if ((dxf && !options.layer.isEmpty() && cad != options.layer) ||
        (dxf && options.layer.isEmpty() && someLayerHasElevation && !cadHasElevation.value(cad))) {
      OGRFeature::DestroyFeature(feature);
      continue;
    }
    double fileX = 0, fileY = 0, z = 0;
    QString name;
    if (dxf) {
      OGRPoint* geometry = asPoint(feature->GetGeometryRef());
      if (!geometry || !std::isfinite(geometry->getX()) || !std::isfinite(geometry->getZ())) {
        ++report.skipped;
        note(QStringLiteral("%1행: 점 좌표가 없습니다.").arg(row));
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
      QString problem;
      if (xs.isEmpty() && ys.isEmpty() && zs.isEmpty()) problem = QStringLiteral("%1행: 빈 행");
      else if (!surveyParseNumber(xs, &fileX) || !surveyParseNumber(ys, &fileY))
        problem = QStringLiteral("%1행: X·Y가 숫자가 아닙니다.");
      else if (!surveyParseNumber(zs, &z)) problem = QStringLiteral("%1행: 표고가 숫자가 아닙니다.");
      if (!problem.isEmpty()) {
        ++report.skipped;
        note(problem.arg(row));
        OGRFeature::DestroyFeature(feature);
        continue;
      }
      name = surveyFieldText(feature, columns.id);
    }
    OGRFeature::DestroyFeature(feature);
    // File order: x is the first coordinate column. SurveyPointArrange puts it in map order.
    SurveyPoint point;
    point.x = fileX;
    point.y = fileY;
    point.z = z;
    point.name = name;
    point.row = row;
    point.suspicious = z == 0.0 || std::abs(z) >= 999.0;
    if (report.rawCount == 0 && !report.swapSuggested && !fileKorea.isEmpty() && fileKorea.isFinite()) {
      const bool swapped = fileKorea.contains(QgsPointXY(fileY, fileX));
      const bool xIsNorthing = std::abs(fileX - 200000.0) > std::abs(fileY - 200000.0) + 50000.0;
      if (swapped && xIsNorthing) {
        report.swapSuggested = true;
        report.swapReason = QStringLiteral("첫 행을 바꿔 읽으면 한국 범위에 맞음");
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
  if (report.nonPointCount > 0)
    report.issues << QStringLiteral("점이 아닌 도형 %1개는 건너뛰었습니다.").arg(report.nonPointCount);
  if (someLayerHasElevation) {
    QStringList dropped;
    for (auto it = cadHasElevation.cbegin(); it != cadHasElevation.cend(); ++it)
      if (!it.value() && !it.key().isEmpty()) dropped << it.key();
    report.droppedLayers = dropped;
    if (!dropped.isEmpty())
      report.issues << QStringLiteral("높이가 없는 점 레이어는 뺐습니다: %1").arg(dropped.join(QStringLiteral(", ")));
  }
  for (auto it = buckets.cbegin(); it != buckets.cend(); ++it) {
    SurveyPoint point = it.value().point;
    point.z = it.value().zSum / it.value().count;
    if (it.value().count > 1 && it.value().zDiffers) {
      ++report.duplicateGroups;
      note(QStringLiteral("같은 좌표(%1, %2)의 표고 %3개를 평균했습니다.")
               .arg(point.x, 0, 'f', 3)
               .arg(point.y, 0, 'f', 3)
               .arg(it.value().count));
    }
    report.points.push_back(point);
  }
  SurveyArrangeOptions arrange;
  arrange.swapAxes = options.swapAxes;
  arrange.sourceCrsAuthId = options.sourceCrsAuthId;
  arrange.targetCrsAuthId = options.crsAuthId;
  return SurveyPointArrange::arrange(report, arrange);
}
