#include "CadDrawingStore.h"

#include "SurveyBundle.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtMath>

#include <cmath>
#include <memory>
#include <optional>

#include <gdal.h>

#include <qgscoordinatetransform.h>
#include <qgsexception.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsfields.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectorlayer.h>

namespace CadDrawingStore {
namespace {

const QString kWriteFailed = QStringLiteral("변환본을 쓰지 못했습니다.");
const QString kTransformFailed = QStringLiteral("도면 좌표를 작업 좌표계로 바꾸지 못했습니다.");
const QString kCanceled = QStringLiteral("도면 변환을 취소했습니다.");
const QString kInfoTable = QStringLiteral("ka_cad_drawing");
constexpr int kCancelEvery = 1000;  // 도형 이만큼마다 취소를 확인한다

struct Table {
  CadKind kind;
  const char* name;
  Qgis::WkbType type;
};

// 표 순서는 tableNames 가 돌려주는 순서다.
constexpr Table kTables[] = {{CadKind::Line, "lines", Qgis::WkbType::LineString},
                             {CadKind::Fill, "fills", Qgis::WkbType::Polygon},
                             {CadKind::Point, "points", Qgis::WkbType::Point},
                             {CadKind::Text, "texts", Qgis::WkbType::Point}};

QgsFields entityFields() {
  QgsFields fields;
  fields.append(QgsField(QStringLiteral("cad_layer"), QMetaType::Type::QString));
  fields.append(QgsField(QStringLiteral("color"), QMetaType::Type::QString));
  fields.append(QgsField(QStringLiteral("text"), QMetaType::Type::QString));
  fields.append(QgsField(QStringLiteral("text_height"), QMetaType::Type::Double));
  fields.append(QgsField(QStringLiteral("text_angle"), QMetaType::Type::Double));
  fields.append(QgsField(QStringLiteral("text_anchor"), QMetaType::Type::Int));
  return fields;
}

QgsFields infoFields() {
  QgsFields fields;
  for (const char* name : {"source_path", "source_sha256", "source_crs", "converter", "created_at"})
    fields.append(QgsField(QString::fromLatin1(name), QMetaType::Type::QString));
  return fields;
}

// 첫 표는 파일을 새로 만들고, 다음 표부터는 같은 파일에 표를 더한다.
std::unique_ptr<QgsVectorFileWriter> openTable(const QString& path, const QString& name, const QgsFields& fields,
                                               Qgis::WkbType type, const QgsCoordinateReferenceSystem& crs,
                                               const QgsCoordinateTransformContext& context, bool newFile) {
  QgsVectorFileWriter::SaveVectorOptions options;
  options.driverName = QStringLiteral("GPKG");
  options.layerName = name;
  options.fileEncoding = QStringLiteral("UTF-8");
  options.actionOnExistingFile =
      newFile ? QgsVectorFileWriter::CreateOrOverwriteFile : QgsVectorFileWriter::CreateOrOverwriteLayer;
  std::unique_ptr<QgsVectorFileWriter> writer(QgsVectorFileWriter::create(path, fields, type, crs, context, options));
  if (!writer || writer->hasError() != QgsVectorFileWriter::NoError) return nullptr;
  return writer;
}

}  // namespace

QString outputPathFor(const QString& sourcePath, const QString& surveyDir) {
  const QDir folder(surveyDir.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
                                              QStringLiteral("/cad-drawings")
                                        : QDir(surveyDir).filePath(SurveyBundle::collectedFolderName() +
                                                                   QStringLiteral("/도면")));
  const QString base = QFileInfo(sourcePath).completeBaseName();
  QString path = folder.filePath(base + QStringLiteral(".gpkg"));
  for (int n = 2; QFileInfo::exists(path); ++n)
    path = folder.filePath(QStringLiteral("%1 (%2).gpkg").arg(base, QString::number(n)));
  return path;
}

bool write(const CadDrawing& drawing, const CadStoreInfo& info, const QgsCoordinateReferenceSystem& workCrs,
           const QgsCoordinateTransformContext& context, const QString& outPath, QString* error,
           const std::function<bool()>& canceled) {
  const auto fail = [error](const QString& message) {
    if (error) *error = message;
    return false;
  };
  const auto stop = [&canceled] { return canceled && canceled(); };
  if (QFileInfo::exists(outPath)) return fail(QStringLiteral("같은 이름의 변환본이 이미 있습니다."));
  const QFileInfo target(outPath);
  if (!QDir().mkpath(target.absolutePath())) return fail(kWriteFailed);
  // 같은 폴더의 임시 폴더에 쓴다: 이름 바꾸기가 같은 디스크 안에서 끝나고, 실패·취소면 폴더째 지워진다.
  QTemporaryDir staging(target.absoluteDir().filePath(QStringLiteral(".cad-XXXXXX")));
  if (!staging.isValid()) return fail(kWriteFailed);
  const QString draft = staging.filePath(QStringLiteral("drawing.gpkg"));

  std::optional<QgsCoordinateTransform> toWork;
  if (!info.sourceCrs.isEmpty()) {
    const QgsCoordinateReferenceSystem source(info.sourceCrs);
    if (!source.isValid() || !workCrs.isValid()) return fail(kTransformFailed);
    toWork = QgsCoordinateTransform(source, workCrs, context);
  }

  const QgsFields fields = entityFields();
  bool fileMade = false;
  for (const Table& table : kTables) {
    QVector<const CadEntity*> rows;
    for (const CadEntity& entity : drawing.entities)
      if (entity.kind == table.kind) rows << &entity;
    if (rows.isEmpty()) continue;
    if (stop()) return fail(kCanceled);
    const std::unique_ptr<QgsVectorFileWriter> writer =
        openTable(draft, QString::fromLatin1(table.name), fields, table.type, workCrs, context, !fileMade);
    if (!writer) return fail(kWriteFailed);
    fileMade = true;
    int count = 0;
    for (const CadEntity* entity : rows) {
      if (++count % kCancelEvery == 0 && stop()) return fail(kCanceled);
      QgsGeometry geometry = entity->geometry;
      if (toWork) {
        try {
          if (geometry.transform(*toWork) != Qgis::GeometryOperationResult::Success) return fail(kTransformFailed);
        } catch (const QgsCsException&) {
          return fail(kTransformFailed);
        }
      }
      QgsFeature feature(fields);
      feature.setGeometry(geometry);
      feature.setAttributes(QgsAttributes(QVector<QVariant>{entity->cadLayer, entity->color.name(), entity->text,
                                                            entity->textHeight, entity->textAngle,
                                                            entity->textAnchor}));
      if (!writer->addFeature(feature)) return fail(kWriteFailed);
    }
  }
  if (stop()) return fail(kCanceled);
  {
    const QgsFields meta = infoFields();
    const std::unique_ptr<QgsVectorFileWriter> writer = openTable(
        draft, kInfoTable, meta, Qgis::WkbType::NoGeometry, QgsCoordinateReferenceSystem(), context, !fileMade);
    if (!writer) return fail(kWriteFailed);
    QgsFeature feature(meta);
    feature.setAttributes(QgsAttributes(QVector<QVariant>{info.sourcePath, info.sourceSha256, info.sourceCrs,
                                                          info.converter,
                                                          QDateTime::currentDateTime().toString(Qt::ISODate)}));
    if (!writer->addFeature(feature)) return fail(kWriteFailed);
  }
  if (!QFile::rename(draft, outPath)) return fail(kWriteFailed);
  return true;
}

CadStoreInfo readInfo(const QString& gpkgPath) {
  CadStoreInfo info;
  QgsVectorLayer::LayerOptions options;
  options.loadDefaultStyle = false;
  QgsVectorLayer layer(gpkgPath + QStringLiteral("|layername=") + kInfoTable, QString(), QStringLiteral("ogr"),
                       options);
  QgsFeature feature;
  if (!layer.isValid() || !layer.getFeatures().nextFeature(feature)) return info;
  info.sourcePath = feature.attribute(QStringLiteral("source_path")).toString();
  info.sourceSha256 = feature.attribute(QStringLiteral("source_sha256")).toString();
  info.sourceCrs = feature.attribute(QStringLiteral("source_crs")).toString();
  info.converter = feature.attribute(QStringLiteral("converter")).toString();
  return info;
}

QStringList tableNames(const QString& gpkgPath) {
  QStringList names;
  GDALAllRegister();
  const char* const drivers[] = {"GPKG", nullptr};
  GDALDatasetH dataset =
      GDALOpenEx(gpkgPath.toUtf8().constData(), GDAL_OF_VECTOR | GDAL_OF_READONLY, drivers, nullptr, nullptr);
  if (!dataset) return names;
  for (const Table& table : kTables)
    if (GDALDatasetGetLayerByName(dataset, table.name)) names << QString::fromLatin1(table.name);
  GDALClose(dataset);
  return names;
}

bool applyAffine(const QList<QgsVectorLayer*>& layers, const GeorefService::Affine& a, QString* error) {
  struct Before {
    QgsVectorLayer* layer = nullptr;
    QgsGeometryMap geometries;
    QgsChangedAttributesMap attributes;
  };
  QVector<Before> touched;
  const double turn = qRadiansToDegrees(std::atan2(a.d, a.a));
  const double scale = std::hypot(a.a, a.d);
  for (QgsVectorLayer* layer : layers) {
    QgsVectorDataProvider* provider = layer && layer->isValid() ? layer->dataProvider() : nullptr;
    bool ok = provider != nullptr;
    Before before;
    QgsGeometryMap moved;
    QgsChangedAttributesMap turned;
    if (ok) {
      const QgsFields fields = provider->fields();
      const int text = fields.lookupField(QStringLiteral("text"));
      const int angle = fields.lookupField(QStringLiteral("text_angle"));
      const int height = fields.lookupField(QStringLiteral("text_height"));
      QgsFeatureIterator features = provider->getFeatures();
      QgsFeature feature;
      while (ok && features.nextFeature(feature)) {
        QgsGeometry geometry = feature.geometry();
        before.geometries.insert(feature.id(), geometry);
        ok = GeorefService::transformGeometry(&geometry, a);
        moved.insert(feature.id(), geometry);
        // 글자만 돈다: 각도에 회전각을 더하고 높이에 배율을 곱한다.
        if (text < 0 || angle < 0 || height < 0 || feature.attribute(text).toString().isEmpty()) continue;
        before.attributes.insert(feature.id(), {{angle, feature.attribute(angle)}, {height, feature.attribute(height)}});
        turned.insert(feature.id(), {{angle, feature.attribute(angle).toDouble() + turn},
                                     {height, feature.attribute(height).toDouble() * scale}});
      }
    }
    if (ok) {
      before.layer = layer;
      touched << before;
      ok = provider->changeGeometryValues(moved) && (turned.isEmpty() || provider->changeAttributeValues(turned));
    }
    if (!ok) {
      for (const Before& done : touched) {
        done.layer->dataProvider()->changeGeometryValues(done.geometries);
        if (!done.attributes.isEmpty()) done.layer->dataProvider()->changeAttributeValues(done.attributes);
        done.layer->updateExtents();
      }
      if (error) *error = QStringLiteral("맞춘 결과를 도면 전체에 적용하지 못했습니다.");
      return false;
    }
    layer->updateExtents();
  }
  return true;
}

}  // namespace CadDrawingStore
