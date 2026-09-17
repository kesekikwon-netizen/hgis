#include "CadastralImport.h"
#include "LayerOps.h"
#include <QCryptographicHash>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QSaveFile>
#include <QSet>
#include <QTemporaryDir>
#include <QRegularExpression>
#include <qgsexpression.h>
#include <qgscoordinatetransform.h>
#include <qgsexception.h>
#include <qgsfeature.h>
#include <qgsfeaturerequest.h>
#include <qgsfillsymbol.h>
#include <qgsgeometryengine.h>
#include <qgslayertree.h>
#include <qgsmapcanvas.h>
#include <qgspallabeling.h>
#include <qgsproject.h>
#include <qgsreadwritecontext.h>
#include <qgssinglesymbolrenderer.h>
#include <qgstextformat.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayerlabeling.h>

namespace {
QByteArray hashFile(const QString& path, const CadastralImport::Cancel& cancel) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) return {};
  QCryptographicHash hash(QCryptographicHash::Sha256);
  while (!f.atEnd()) {
    if (cancel && cancel()) return {};
    const auto bytes = f.read(1024 * 1024);
    if (f.error() != QFile::NoError) return {};
    hash.addData(bytes);
  }
  return hash.result().toHex();
}
}

PreparedReferenceMap CadastralImport::prepare(const QStringList& sources, const QgsGeometry& scope,
    const QgsCoordinateReferenceSystem& crs, const QgsCoordinateTransformContext& context,
    const QString& directory, const Cancel& cancel, const Progress& progress) {
  PreparedReferenceMap result;
  result.tableName = QStringLiteral("cadastral");
  auto stopped = [&]() {
    if (cancel && cancel()) { result.status = PreparedReferenceMap::Status::Cancelled; return true; }
    return false;
  };
  if (stopped()) return result;
  if (sources.isEmpty() || !crs.isValid() || scope.isEmpty() || !scope.isGeosValid()) {
    result.error = QStringLiteral("지적도 원본과 유효한 조사 범위·좌표계가 필요합니다."); return result;
  }
  if (directory.isEmpty() || !QDir().mkpath(directory)) {
    result.error = QStringLiteral("지적도 표시 파일을 저장할 폴더를 만들지 못했습니다."); return result;
  }
  QCryptographicHash identity(QCryptographicHash::Sha256);
  identity.addData("cadastral-scope-v1");
  identity.addData(scope.asWkb()); identity.addData(crs.toWkt().toUtf8());
  QDomDocument transformDocument;
  auto transformElement = transformDocument.createElement(QStringLiteral("transforms"));
  transformDocument.appendChild(transformElement);
  context.writeXml(transformElement, QgsReadWriteContext());
  identity.addData(transformDocument.toByteArray());
  QStringList ordered = sources; ordered.removeDuplicates(); ordered.sort();
  for (const auto& source : ordered) {
    QStringList files{source};
    if (QFileInfo(source).suffix().compare(QStringLiteral("shp"), Qt::CaseInsensitive) == 0) {
      const QString base = QFileInfo(source).absolutePath() + '/' + QFileInfo(source).completeBaseName();
      for (const auto* ext : {"dbf", "shx", "prj", "cpg"}) {
        QString path = base + '.' + QLatin1String(ext);
        if (!QFileInfo::exists(path)) path = base + '.' + QString::fromLatin1(ext).toUpper();
        if (QFileInfo::exists(path)) files.append(path);
      }
    }
    for (const auto& file : files) {
      const auto hash = hashFile(file, cancel);
      if (stopped()) return result;
      if (hash.isEmpty()) { result.error = QStringLiteral("지적도 원본 파일을 읽지 못했습니다."); return result; }
      identity.addData(file.toUtf8()); identity.addData(hash);
    }
  }
  const QString path = QDir(directory).filePath(QString::fromLatin1(identity.result().toHex()) + QStringLiteral(".gpkg"));
  QLockFile lock(path + QStringLiteral(".lock"));
  if (!lock.tryLock(0)) { result.error = QStringLiteral("같은 범위의 지적도를 준비 중입니다."); return result; }
  result.gpkgPath = path;
  if (QFileInfo::exists(path)) {
    QFile digest(path + QStringLiteral(".sha256"));
    if (digest.open(QIODevice::ReadOnly) && digest.readAll().trimmed() == hashFile(path, cancel)) {
      if (stopped()) return result;
      QgsVectorLayer cached(path + QStringLiteral("|layername=cadastral"), QString(), QStringLiteral("ogr"));
      if (cached.isValid() && cached.crs() == crs && cached.fields().indexOf(QStringLiteral("JIBUN")) >= 0 && cached.featureCount() > 0) {
        result.status = PreparedReferenceMap::Status::Ready;
        if (progress) progress(100, QStringLiteral("저장된 조사 주변 지적도를 사용합니다."));
        return result;
      }
    }
  }
  QTemporaryDir staging(QDir(directory).filePath(QStringLiteral(".cadastral-XXXXXX")));
  if (!staging.isValid()) { result.error = QStringLiteral("지적도 임시 파일을 준비하지 못했습니다."); return result; }
  const QString draft = staging.filePath(QStringLiteral("cadastral.gpkg"));
  QgsFields fields;
  fields.append(QgsField(QStringLiteral("PNU"), QMetaType::Type::QString));
  fields.append(QgsField(QStringLiteral("JIBUN"), QMetaType::Type::QString));
  QgsVectorFileWriter::SaveVectorOptions options;
  options.driverName = QStringLiteral("GPKG"); options.layerName = result.tableName;
  options.fileEncoding = QStringLiteral("UTF-8"); options.layerOptions << QStringLiteral("SPATIAL_INDEX=YES");
  std::unique_ptr<QgsVectorFileWriter> writer(QgsVectorFileWriter::create(draft, fields, Qgis::WkbType::MultiPolygon, crs, context, options));
  if (!writer || writer->hasError() != QgsVectorFileWriter::NoError) {
    result.error = QStringLiteral("지적도 표시 파일을 만들지 못했습니다."); return result;
  }
  std::unique_ptr<QgsGeometryEngine> inside(QgsGeometry::createGeometryEngine(scope.constGet()));
  inside->prepareGeometry();
  QSet<QString> seen;
  qint64 written = 0;
  try {
    int sourceIndex = 0;
    for (const auto& source : ordered) {
      if (stopped()) return result;
      if (progress) progress(sourceIndex * 90 / ordered.size(), QStringLiteral("조사 주변 필지와 지번을 정리하고 있습니다…"));
      QgsVectorLayer input(source, QString(), QStringLiteral("ogr"));
      if (!input.isValid() || !input.crs().isValid() || input.geometryType() != Qgis::GeometryType::Polygon) {
        result.error = QStringLiteral("지적도 도형 또는 원본 좌표계를 확인하지 못했습니다."); return result;
      }
      const int jibun = input.fields().lookupField(QStringLiteral("JIBUN"));
      const int pnu = input.fields().lookupField(QStringLiteral("PNU"));
      if (jibun < 0) { result.error = QStringLiteral("지적도 원본에 지번(JIBUN) 항목이 없습니다."); return result; }
      const QgsCoordinateTransform toSource(crs, input.crs(), context);
      const QgsCoordinateTransform toWork(input.crs(), crs, context);
      QgsFeatureRequest request;
      request.setFilterRect(toSource.transformBoundingBox(scope.boundingBox()));
      QgsAttributeList attributes{jibun}; if (pnu >= 0) attributes.append(pnu);
      request.setSubsetOfAttributes(attributes);
      auto features = input.getFeatures(request);
      QgsFeature feature;
      while (features.nextFeature(feature)) {
        if (stopped()) return result;
        QgsGeometry geometry = feature.geometry();
        if (geometry.isEmpty()) continue;
        if (geometry.transform(toWork) != Qgis::GeometryOperationResult::Success) {
          result.error = QStringLiteral("필지 좌표를 조사 좌표계로 변환하지 못했습니다."); return result;
        }
        if (!inside->intersects(geometry.constGet())) continue;
        const QString id = pnu >= 0 ? feature.attribute(pnu).toString().trimmed() : QString();
        // The same parcel ID can legitimately contain distinct polygon parts.
        // Deduplicate only identical geometry, never erase another part by ID.
        QgsGeometry canonical = geometry; canonical.convertToMultiType(); canonical.normalize();
        const QString key = id + ':' + QString::fromLatin1(QCryptographicHash::hash(canonical.asWkb(), QCryptographicHash::Sha256).toHex());
        if (seen.contains(key)) continue;
        seen.insert(key);
        geometry.convertToMultiType();
        QgsFeature output(fields); output.setGeometry(geometry);
        output.setAttribute(0, id); output.setAttribute(1, feature.attribute(jibun).toString());
        if (!writer->addFeature(output)) { result.error = QStringLiteral("지적도 필지를 저장하지 못했습니다."); return result; }
        ++written;
      }
      ++sourceIndex;
    }
  } catch (const QgsCsException&) {
    result.error = QStringLiteral("지적도와 조사 좌표계 사이의 변환에 실패했습니다."); return result;
  }
  if (writer->hasError() != QgsVectorFileWriter::NoError || !writer->flushBuffer()) {
    result.error = QStringLiteral("지적도 저장을 완료하지 못했습니다."); return result;
  }
  writer.reset();
  if (stopped()) return result;
  if (!written) { result.error = QStringLiteral("조사 주변 범위와 겹치는 필지가 없습니다."); return result; }
  {
    QgsVectorLayer check(draft + QStringLiteral("|layername=cadastral"), QString(), QStringLiteral("ogr"));
    if (!check.isValid() || check.featureCount() != written || check.crs() != crs) {
      result.error = QStringLiteral("저장한 지적도 도형 수와 좌표계를 확인하지 못했습니다."); return result;
    }
  }
  QFile input(draft); QSaveFile output(path);
  output.setDirectWriteFallback(false);
  if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly)) {
    result.error = QStringLiteral("완료한 지적도를 저장 위치로 옮기지 못했습니다."); return result;
  }
  while (!input.atEnd()) {
    if (stopped()) return result;
    const auto bytes = input.read(1024 * 1024);
    if (input.error() != QFile::NoError || output.write(bytes) != bytes.size()) {
      result.error = QStringLiteral("지적도 파일 저장 공간을 확인하세요."); return result;
    }
  }
  if (!output.commit()) { result.error = QStringLiteral("지적도 파일을 확정하지 못했습니다."); return result; }
  result.outputCommitted = true;
  QSaveFile digest(path + QStringLiteral(".sha256"));
  const auto hash = hashFile(path, {});
  if (digest.open(QIODevice::WriteOnly)) { digest.write(hash); digest.commit(); }
  result.status = PreparedReferenceMap::Status::Ready;
  if (progress) progress(100, QStringLiteral("주변 필지 %1개를 준비했습니다.").arg(written));
  return result;
}

bool CadastralImport::applyStyle(QgsVectorLayer* layer, const QColor& color, bool labels) {
  if (!layer || !color.isValid() || layer->fields().lookupField(QStringLiteral("JIBUN")) < 0) return false;
  auto symbol = QgsFillSymbol::createSimple({{QStringLiteral("style"), QStringLiteral("no")},
      {QStringLiteral("outline_color"), color.name(QColor::HexArgb)}, {QStringLiteral("outline_width"), QStringLiteral("0.2")},
      {QStringLiteral("outline_width_unit"), QStringLiteral("MM")}});
  layer->setRenderer(new QgsSingleSymbolRenderer(symbol.release()));
  QgsPalLayerSettings settings;
  settings.fieldName = QStringLiteral("JIBUN"); settings.isExpression = false;
  QgsTextFormat format; format.setSize(8); format.setSizeUnit(Qgis::RenderUnit::Points); format.setColor(color);
  settings.setFormat(format); settings.placement = Qgis::LabelPlacement::OverPoint;
  // At small scales only boundaries draw; dense labels needlessly dominate PAL.
  settings.scaleVisibility = true; settings.minimumScale = 10000.; settings.maximumScale = 0.;
  layer->setLabeling(new QgsVectorLayerSimpleLabeling(settings)); layer->setLabelsEnabled(labels);
  layer->setCustomProperty(QStringLiteral("ka_hgis/cadastral"), true);
  layer->setCustomProperty(QStringLiteral("ka_hgis/cadastral_color"), color.name(QColor::HexArgb));
  layer->triggerRepaint(); return true;
}

QgsVectorLayer* CadastralImport::addPrepared(QgsProject* project, QgsMapCanvas* canvas,
    const PreparedReferenceMap& prepared, QString* error) {
  if (!project || !prepared.isReady()) { if (error) *error = prepared.error; return nullptr; }
  auto layer = std::make_unique<QgsVectorLayer>(prepared.gpkgPath + QStringLiteral("|layername=cadastral"),
      QStringLiteral("지적도 · 조사 주변 5km"), QStringLiteral("ogr"));
  if (!layer->isValid() || !applyStyle(layer.get())) {
    if (error) *error = QStringLiteral("준비한 지적도를 표시하지 못했습니다."); return nullptr;
  }
  QStringList previous;
  QColor retainedColor(Qt::black); bool retainedLabels = true;
  for (auto* existing : project->mapLayers()) {
    if (existing->customProperty(QStringLiteral("ka_hgis/cadastral")).toBool()) {
      previous.append(existing->id());
      retainedColor = QColor(existing->customProperty(QStringLiteral("ka_hgis/cadastral_color"), QStringLiteral("#000000")).toString());
      if (auto* vector = qobject_cast<QgsVectorLayer*>(existing)) retainedLabels = vector->labelsEnabled();
    }
    // Keep the old online layer, but don't draw its baked-in labels twice.
    if (existing->providerType() == QLatin1String("wms") && existing->source().contains(QLatin1String("lp_pa_cbnd"), Qt::CaseInsensitive))
      if (auto* node = project->layerTreeRoot()->findLayer(existing->id())) node->setItemVisibilityChecked(false);
  }
  applyStyle(layer.get(), retainedColor, retainedLabels);
  LayerOps::markReferenceLayer(layer.get());
  auto* added = layer.release(); project->addMapLayer(added, false);
  auto* references = project->layerTreeRoot()->findGroup(QStringLiteral("참조 지도"));
  if (!references) references = project->layerTreeRoot()->addGroup(QStringLiteral("참조 지도"));
  references->addLayer(added); references->setItemVisibilityChecked(true);
  LayerOps::placeInLegendGroup(project, added, QStringLiteral("참조 지도"));
  prepared.retainFiles(); project->removeMapLayers(previous);
  if (canvas) { LayerOps::syncMapCanvas(project, canvas, false); LayerOps::refreshCanvasIfIdle(canvas); }
  return added;
}

bool CadastralImport::focusParcel(QgsProject* project, QgsMapCanvas* canvas,
                                  const QString& pnu, QgsPointXY* position) {
  static const QRegularExpression identifier(QStringLiteral("^[0-9]{19}$"));
  if (!project || !canvas || !identifier.match(pnu).hasMatch()) return false;
  for (auto* mapLayer : project->mapLayers()) {
    auto* layer = qobject_cast<QgsVectorLayer*>(mapLayer);
    if (!layer || !layer->isValid() || !layer->crs().isValid()
        || !layer->customProperty(QStringLiteral("ka_hgis/cadastral")).toBool()) continue;
    const int field = layer->fields().lookupField(QStringLiteral("PNU"));
    if (field < 0) continue;
    QgsFeatureRequest request;
    request.setFilterExpression(QgsExpression::quotedColumnRef(layer->fields().at(field).name())
                                + QStringLiteral(" = ") + QgsExpression::quotedString(pnu));
    request.setSubsetOfAttributes(QgsAttributeList{field});
    QgsFeatureIds ids;
    QgsRectangle bounds;
    QgsPointXY marker;
    try {
      QgsCoordinateTransform transform(layer->crs(), canvas->mapSettings().destinationCrs(),
                                       project->transformContext());
      auto features = layer->getFeatures(request);
      QgsFeature feature;
      while (features.nextFeature(feature)) {
        if (feature.attribute(field).toString() != pnu || !feature.hasGeometry()) continue;
        QgsGeometry geometry = feature.geometry();
        if (geometry.transform(transform) != Qgis::GeometryOperationResult::Success || geometry.isEmpty()) continue;
        const auto surface = geometry.pointOnSurface();
        if (surface.isEmpty()) continue;
        if (ids.isEmpty()) { bounds = geometry.boundingBox(); marker = surface.asPoint(); }
        else bounds.combineExtentWith(geometry.boundingBox());
        ids.insert(feature.id());
      }
    } catch (const QgsCsException&) { continue; }
    if (ids.isEmpty() || bounds.isEmpty()) continue;
    if (auto* node = project->layerTreeRoot()->findLayer(layer->id()))
      node->setItemVisibilityCheckedParentRecursive(true);
    layer->selectByIds(ids);
    bounds.scale(1.4);
    LayerOps::syncMapCanvas(project, canvas, false);
    canvas->setExtent(bounds);
    if (position) *position = marker;
    return true;
  }
  return false;
}
