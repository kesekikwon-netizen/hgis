#include "SurveySchema.h"

#include <QByteArray>

#include <gdal.h>
#include <ogr_api.h>

#include <qgsfield.h>
#include <qgsfields.h>

namespace SurveySchema {
namespace {
constexpr QMetaType::Type kText = QMetaType::Type::QString;
constexpr QMetaType::Type kReal = QMetaType::Type::Double;

// Field names stay within 10 characters: the submit package is a shapefile (DBF).
const QList<FieldDef> kAudit = {
    {"uid", kText, "고유번호"},
    {"created_at", kText, "만든 시각"},
    {"updated_at", kText, "고친 시각"},
};

QList<FieldDef> withAudit(QList<FieldDef> fields) {
  fields.append(kAudit);
  return fields;
}

QList<LayerDef> buildLayers() {
  QList<LayerDef> out;
  out.append({"survey_area", "Polygon",
              withAudit({{"survey_name", kText, "조사명"},
                         {"site_name", kText, "유적명"},
                         {"note", kText, "비고"}})});
  out.append({"feature_poly", "Polygon",
              withAudit({{"kind", kText, "유구종류"},
                         {"period", kText, "시대"},
                         {"feature_no", kText, "유구번호"},
                         {"note", kText, "비고"},
                         {"layer_no", kText, "층위·문맥 번호"},
                         {"depth_m", kReal, "깊이(m)"},
                         {"top_el", kReal, "상면 표고(m)"},
                         {"bottom_el", kReal, "바닥 표고(m)"},
                         {"relation", kText, "선후관계"},
                         {"status", kText, "조사 상태"},
                         {"photo", kText, "사진"},
                         {"surveyor", kText, "조사자"},
                         {"surv_date", kText, "조사일"}})});
  out.append({"feature_line", "LineString",
              withAudit({{"kind", kText, "유구종류"},
                         {"period", kText, "시대"},
                         {"note", kText, "비고"},
                         {"feature_no", kText, "유구번호"},
                         {"layer_no", kText, "층위·문맥 번호"},
                         {"relation", kText, "선후관계"},
                         {"status", kText, "조사 상태"},
                         {"photo", kText, "사진"},
                         {"surveyor", kText, "조사자"},
                         {"surv_date", kText, "조사일"}})});
  out.append({"section_line", "LineString",
              withAudit({{"section_id", kText, "단면번호"},
                         {"note", kText, "비고"},
                         {"surveyor", kText, "조사자"},
                         {"surv_date", kText, "조사일"}})});
  out.append({"control_points", "Point",
              withAudit({{"point_id", kText, "점ID"},
                         {"x", kReal, "X"},
                         {"y", kReal, "Y"},
                         {"z", kReal, "표고 Z"},
                         {"datum", kText, "측지기준계"},
                         {"ellipsoid", kText, "타원체"},
                         {"projection", kText, "투영"},
                         {"origin", kText, "원점"},
                         {"accuracy", kText, "정확도 메모"},
                         {"accuracy_m", kReal, "정확도(m)"},
                         {"pdop", kReal, "PDOP"},
                         {"fix_type", kText, "수신상태"},
                         {"pixel_x", kReal, "픽셀 X"},
                         {"pixel_y", kReal, "픽셀 Y"}})});
  out.append({"artifact_point", "Point",
              withAudit({{"kind", kText, "유물종류"},
                         {"period", kText, "시대"},
                         {"artifact_no", kText, "유물번호"},
                         {"note", kText, "비고"},
                         {"in_feature", kText, "소속 유구"},
                         {"z", kReal, "표고 Z"},
                         {"layer_no", kText, "층위·문맥 번호"},
                         {"photo", kText, "사진"},
                         {"surveyor", kText, "조사자"},
                         {"surv_date", kText, "조사일"}})});
  // The trench grid is generated, not recorded by hand: no record or audit columns.
  out.append({"trial_trench", "Polygon",
              {{"name", kText, "이름"}, {"width", kReal, "폭"}, {"length", kReal, "길이"}}});
  return out;
}

QByteArray utf8(const QString& text) { return text.toUtf8(); }
}  // namespace

const QList<LayerDef>& layers() {
  static const QList<LayerDef> table = buildLayers();
  return table;
}

const LayerDef* layer(const QString& layerKey) {
  for (const LayerDef& def : layers()) {
    if (layerKey == QLatin1String(def.name)) return &def;
  }
  return nullptr;
}

QgsFields fieldsFor(const QString& layerKey) {
  QgsFields fields;
  if (const LayerDef* def = layer(layerKey)) {
    for (const FieldDef& field : def->fields) fields.append(QgsField(QString::fromUtf8(field.name), field.type));
  }
  return fields;
}

bool isAutoField(const QString& fieldName) {
  for (const FieldDef& field : kAudit) {
    if (fieldName.compare(QLatin1String(field.name), Qt::CaseInsensitive) == 0) return true;
  }
  return false;
}

QString labelKo(const QString& fieldName) {
  for (const LayerDef& def : layers()) {
    for (const FieldDef& field : def.fields) {
      if (fieldName == QLatin1String(field.name)) return QString::fromUtf8(field.labelKo);
    }
  }
  return fieldName;
}

int readVersion(const QString& gpkgPath) {
  const char* drivers[] = {"GPKG", nullptr};
  GDALDatasetH ds = GDALOpenEx(utf8(gpkgPath).constData(), GDAL_OF_VECTOR | GDAL_OF_READONLY, drivers, nullptr,
                               nullptr);
  if (!ds) return 0;
  const char* value = GDALGetMetadataItem(ds, kVersionMetadataItem, nullptr);
  const int version = value ? QByteArray(value).toInt() : 0;
  GDALClose(ds);
  return version;
}

bool writeVersion(const QString& gpkgPath, int version, QString* errorOut) {
  const char* drivers[] = {"GPKG", nullptr};
  GDALDatasetH ds =
      GDALOpenEx(utf8(gpkgPath).constData(), GDAL_OF_VECTOR | GDAL_OF_UPDATE, drivers, nullptr, nullptr);
  if (!ds) {
    if (errorOut) *errorOut = QStringLiteral("조사 파일을 열지 못해 판 번호를 적지 못했습니다.");
    return false;
  }
  const QByteArray text = QByteArray::number(version);
  const CPLErr err = GDALSetMetadataItem(ds, kVersionMetadataItem, text.constData(), nullptr);
  GDALClose(ds);  // the GPKG driver writes gpkg_metadata on close
  if (err != CE_None) {
    if (errorOut) *errorOut = QStringLiteral("조사 파일에 판 번호를 적지 못했습니다.");
    return false;
  }
  return true;
}

MigrationResult migrateGenerationCopy(const QString& gpkgPath) {
  MigrationResult result;
  const char* drivers[] = {"GPKG", nullptr};
  GDALDatasetH ds =
      GDALOpenEx(utf8(gpkgPath).constData(), GDAL_OF_VECTOR | GDAL_OF_UPDATE, drivers, nullptr, nullptr);
  if (!ds) {
    result.error = QStringLiteral("조사 파일 사본을 열지 못해 새 기록 항목을 더하지 못했습니다.");
    return result;
  }
  const char* stamp = GDALGetMetadataItem(ds, kVersionMetadataItem, nullptr);
  result.fromVersion = stamp ? QByteArray(stamp).toInt() : 0;
  // Columns are checked on every call, not only below the current stamp: a table that
  // another save path rewrote from an old layer gets its optional columns back.
  struct Missing {
    OGRLayerH layer;
    const LayerDef* def;
    const FieldDef* field;
  };
  QList<Missing> missing;
  for (const LayerDef& def : layers()) {
    OGRLayerH ogrLayer = GDALDatasetGetLayerByName(ds, def.name);
    if (!ogrLayer) continue;  // tables made on first use (artifact_point, trial_trench) stay absent
    for (const FieldDef& field : def.fields) {
      if (OGR_FD_GetFieldIndex(OGR_L_GetLayerDefn(ogrLayer), field.name) < 0) missing.append({ogrLayer, &def, &field});
    }
  }
  bool failed = false;
  if (!missing.isEmpty()) {
    // All columns or none (GPKG runs ALTER TABLE inside the transaction).
    const bool transaction = GDALDatasetStartTransaction(ds, FALSE) == OGRERR_NONE;
    for (const Missing& m : std::as_const(missing)) {
      OGRFieldDefnH defn = OGR_Fld_Create(m.field->name, m.field->type == kReal ? OFTReal : OFTString);
      const OGRErr err = OGR_L_CreateField(m.layer, defn, TRUE);
      OGR_Fld_Destroy(defn);
      if (err != OGRERR_NONE) {
        failed = true;
        result.error = QStringLiteral("%1 표에 %2 항목을 더하지 못했습니다.")
                           .arg(QString::fromUtf8(m.def->name), QString::fromUtf8(m.field->name));
        break;
      }
      result.addedFields << QStringLiteral("%1.%2").arg(QString::fromUtf8(m.def->name), QString::fromUtf8(m.field->name));
    }
    if (transaction && failed) {
      GDALDatasetRollbackTransaction(ds);
    } else if (transaction && GDALDatasetCommitTransaction(ds) != OGRERR_NONE) {
      failed = true;
      result.error = QStringLiteral("새 기록 항목을 조사 파일 사본에 확정하지 못했습니다.");
    }
  }
  // Never lowers the stamp of a file from a newer build.
  if (!failed && result.fromVersion < kCurrentVersion) {
    const QByteArray text = QByteArray::number(kCurrentVersion);
    if (GDALSetMetadataItem(ds, kVersionMetadataItem, text.constData(), nullptr) != CE_None) {
      failed = true;
      result.error = QStringLiteral("조사 파일 사본에 판 번호를 적지 못했습니다.");
    }
  }
  GDALClose(ds);  // the GPKG driver writes gpkg_metadata on close
  if (failed) result.addedFields.clear();
  result.ok = !failed;
  return result;
}

}  // namespace SurveySchema
