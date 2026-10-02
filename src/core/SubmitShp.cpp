#include "SubmitShp.h"

#include <QFileInfo>

#include <gdal.h>
#include <ogr_api.h>
#include <ogr_srs_api.h>

#include <qgscoordinatetransform.h>
#include <qgsexception.h>
#include <qgsfeatureiterator.h>
#include <qgsfield.h>
#include <qgsfields.h>
#include <qgsgeometry.h>
#include <qgsogrutils.h>
#include <qgsproject.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorlayer.h>
#include <qgswkbtypes.h>

namespace SubmitShp {
namespace {
std::unique_ptr<QgsVectorLayer> emptyMemoryCopy(QgsVectorLayer* like, const QString& key,
                                                const QgsFields& fields, QString* errorOut) {
  // The CRS is copied as an object: a CRS without an EPSG code must not turn into a guess.
  auto copy = std::make_unique<QgsVectorLayer>(QgsWkbTypes::displayString(like->wkbType()), key,
                                               QStringLiteral("memory"));
  if (!copy->isValid() || !copy->dataProvider()->addAttributes(fields.toList())) {
    if (errorOut) *errorOut = QStringLiteral("%1 레이어의 속성 구성을 만들지 못했습니다.").arg(key);
    return nullptr;
  }
  copy->setCrs(like->crs());
  copy->updateFields();
  return copy;
}

// Copies one value by field name; a value the target type cannot hold becomes NULL
// and is reported instead of making the memory provider reject the whole batch.
void copyValue(QgsFeature& to, int toIndex, const QgsField& field, const QVariant& value,
               const QString& key, QStringList* notes) {
  QVariant converted = value;
  if (!field.convertCompatible(converted)) {
    if (notes) notes->append(QStringLiteral("%1 %2: 형식이 달라 값 하나를 비웠습니다").arg(key, field.name()));
    converted = QVariant();
  }
  to.setAttribute(toIndex, converted);
}
}  // namespace

QString leftUtf8(const QString& text, int maxBytes) {
  QString out;
  int bytes = 0;
  for (int i = 0; i < text.size();) {
    const int width = text.at(i).isHighSurrogate() && i + 1 < text.size() ? 2 : 1;
    const QString unit = text.mid(i, width);
    const int size = int(unit.toUtf8().size());
    if (bytes + size > maxBytes) break;
    out += unit;
    bytes += size;
    i += width;
  }
  return out;
}

QString fieldNameFor(const QString& name, QSet<QString>& used) {
  QString mapped = name;
  if (name.toUtf8().size() > 10) {
    if (name == QLatin1String("survey_name")) mapped = QStringLiteral("surv_name");
    else if (name == QLatin1String("artifact_no")) mapped = QStringLiteral("artif_no");
    else mapped = leftUtf8(name, 10);
  }
  QString candidate = mapped;
  int serial = 2;
  while ((used.contains(candidate) || candidate.isEmpty()) && serial <= 99) {
    const QString suffix = QString::number(serial++);
    candidate = leftUtf8(mapped, 10 - int(suffix.size())) + suffix;
  }
  used.insert(candidate);
  return candidate;
}

std::unique_ptr<QgsVectorLayer> mergeLayers(const QList<QgsVectorLayer*>& sources, const QString& key,
                                            QgsProject* project, QStringList* notes, QString* errorOut) {
  if (sources.isEmpty()) return nullptr;
  QgsVectorLayer* primary = sources.first();
  // Union of fields: a field that only a later layer has is kept, not dropped.
  QgsFields fields;
  for (QgsVectorLayer* layer : sources) {
    for (const QgsField& field : layer->fields()) {
      if (fields.lookupField(field.name()) < 0) fields.append(field);
    }
  }
  auto merged = emptyMemoryCopy(primary, key, fields, errorOut);
  if (!merged) return nullptr;
  for (QgsVectorLayer* layer : sources) {
    QgsCoordinateTransform toPrimary;
    if (layer->crs().isValid() && primary->crs().isValid() && layer->crs() != primary->crs())
      toPrimary = QgsCoordinateTransform(layer->crs(), primary->crs(), project->transformContext());
    QgsFeatureList batch;
    QgsFeatureIterator it = layer->getFeatures();
    QgsFeature source;
    while (it.nextFeature(source)) {
      QgsFeature copy(merged->fields());
      // 필드는 이름으로 맞춘다. 레이어마다 속성 순서가 다를 수 있다.
      for (int i = 0; i < merged->fields().count(); ++i) {
        const int index = source.fields().indexOf(merged->fields().at(i).name());
        if (index >= 0) copyValue(copy, i, merged->fields().at(i), source.attribute(index), key, notes);
      }
      QgsGeometry geometry = source.geometry();
      if (toPrimary.isValid() && !geometry.isNull()) {
        bool moved = false;
        try {
          moved = geometry.transform(toPrimary) == Qgis::GeometryOperationResult::Success;
        } catch (const QgsCsException&) {
          moved = false;
        }
        if (!moved) {
          if (errorOut) *errorOut = QStringLiteral("%1 레이어 「%2」의 좌표를 합치다 변환에 실패했습니다.").arg(key, layer->name());
          return nullptr;
        }
      }
      copy.setGeometry(geometry);
      batch.append(copy);
    }
    if (!batch.isEmpty() && !merged->dataProvider()->addFeatures(batch)) {
      if (errorOut) *errorOut = QStringLiteral("%1 레이어 「%2」의 도형을 합치지 못했습니다.").arg(key, layer->name());
      return nullptr;
    }
  }
  merged->updateExtents();
  return merged;
}

std::unique_ptr<QgsVectorLayer> withShapefileFieldNames(QgsVectorLayer* source, const QString& key,
                                                        QStringList* notes, QString* errorOut) {
  if (!source) return nullptr;
  bool needsAlias = false;
  for (const QgsField& field : source->fields()) {
    if (field.name().toUtf8().size() > 10) needsAlias = true;
  }
  if (!needsAlias) return nullptr;
  QSet<QString> used;
  QgsFields fields;
  QStringList fromNames;
  for (const QgsField& field : source->fields()) {
    const QString mapped = fieldNameFor(field.name(), used);
    if (mapped != field.name() && notes) notes->append(QStringLiteral("%1 %2=%3").arg(key, field.name(), mapped));
    QgsField out(mapped, field.type());
    out.setLength(field.length());
    out.setPrecision(field.precision());
    fields.append(out);
    fromNames.append(field.name());
  }
  auto copy = emptyMemoryCopy(source, key, fields, errorOut);
  if (!copy) return nullptr;
  QgsFeatureIterator it = source->getFeatures();
  QgsFeature sourceFeature;
  QgsFeatureList batch;
  while (it.nextFeature(sourceFeature)) {
    QgsFeature feature(copy->fields());
    for (int i = 0; i < fromNames.size(); ++i) {
      const int index = sourceFeature.fields().indexOf(fromNames.at(i));
      if (index >= 0) feature.setAttribute(i, sourceFeature.attribute(index));
    }
    feature.setGeometry(sourceFeature.geometry());
    batch.append(feature);
  }
  if (!batch.isEmpty() && !copy->dataProvider()->addFeatures(batch)) {
    if (errorOut) *errorOut = QStringLiteral("%1 SHP 속성을 복사하지 못했습니다.").arg(key);
    return nullptr;
  }
  copy->updateExtents();
  return copy;
}

bool verifyWritten(const QString& shpPath, long long expectedFeatures, QString* errorOut) {
  const QString name = QFileInfo(shpPath).fileName();
  GDALDatasetH ds = GDALOpenEx(shpPath.toUtf8().constData(), GDAL_OF_VECTOR | GDAL_OF_READONLY,
                               nullptr, nullptr, nullptr);
  if (!ds) {
    if (errorOut) *errorOut = QStringLiteral("방금 쓴 %1을(를) 다시 열지 못했습니다.").arg(name);
    return false;
  }
  OGRLayerH layer = GDALDatasetGetLayer(ds, 0);
  const long long count = layer ? static_cast<long long>(OGR_L_GetFeatureCount(layer, TRUE)) : -1;
  bool is5179 = false;
  if (OGRSpatialReferenceH srs = layer ? OGR_L_GetSpatialRef(layer) : nullptr) {
    const char* auth = OSRGetAuthorityName(srs, nullptr);
    const char* code = OSRGetAuthorityCode(srs, nullptr);
    is5179 = auth && code && QString::fromLatin1(auth) == QLatin1String("EPSG") &&
             QString::fromLatin1(code) == QLatin1String("5179");
    // Same identification the QGIS OGR provider uses for an ESRI .prj.
    if (!is5179) is5179 = QgsOgrUtils::OGRSpatialReferenceToCrs(srs).authid() == QLatin1String("EPSG:5179");
  }
  GDALClose(ds);
  if (!is5179) {
    if (errorOut) *errorOut = QStringLiteral("%1의 좌표계가 EPSG:5179로 기록되지 않았습니다.").arg(name);
    return false;
  }
  if (count != expectedFeatures) {
    if (errorOut)
      *errorOut = QStringLiteral("%1에 도형이 %2개 기록돼야 하는데 %3개입니다.").arg(name).arg(expectedFeatures).arg(count);
    return false;
  }
  return true;
}

}  // namespace SubmitShp
