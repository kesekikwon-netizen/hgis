#include "HeritageImport.h"

#include "HeritageSiteLegend.h"
#include "LayerOps.h"
#include "TopographicArchive.h"

#include <qgslayertree.h>
#include <qgslayertreegroup.h>
#include <qgslayertreelayer.h>
#include <qgsproject.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorlayer.h>

#include <cpl_conv.h>
#include <cpl_string.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QScopeGuard>
#include <QSet>
#include <QTemporaryDir>
#include <memory>
#include <optional>

namespace {

// 유적명이 들어 있을 만한 컬럼. 앞에 있는 것부터 본다.
// 자료 종류마다 다르다 — 지표조사구역·발굴조사구역은 사업명으로 검색한다.
const char* const kNameCandidates[] = {
    "유적명", "국가유산명", "문화재명", "명칭",   "유적명칭", "사업명",  "조사명",
    "NAME",   "name",       "NM",       "nm",     "SITE_NM",  "site_nm", "HERITAGE_NM",
    "CULTURE_NM", "RELIC_NM", "BURIAL_NM", "TITLE", "title",
};

bool looksLikeText(const QgsVectorLayer* layer, int index) {
  if (!layer || index < 0 || index >= layer->fields().count()) return false;
  const QMetaType::Type type = static_cast<QMetaType::Type>(layer->fields().at(index).type());
  return type == QMetaType::QString;
}

// Decode a disposable SHP copy before writing Unicode storage. Rewriting a SHP
// as UTF-8 would truncate Korean DBF column names (10 bytes) and long values.
QgsVectorLayer* utf8WorkingLayer(const QString& shp, const QString& layerName,
                                const QString& archiveRoot, QgsProject* project, QString& error) {
  const QString workingRoot = QDir(archiveRoot).filePath(QStringLiteral("UTF8"));
  if (!QDir().mkpath(workingRoot)) {
    error = QStringLiteral("UTF-8 작업 자료를 보관할 폴더를 만들지 못했습니다.");
    return nullptr;
  }
  QTemporaryDir working(QDir(workingRoot).filePath(QStringLiteral("heritage-XXXXXX")));
  QTemporaryDir scratch;
  if (!working.isValid() || !scratch.isValid()) {
    error = QStringLiteral("UTF-8 변환용 임시 폴더를 만들지 못했습니다.");
    return nullptr;
  }
  const QFileInfo original(shp);
  const QString base = original.dir().filePath(original.completeBaseName());
  const QString copyBase = scratch.filePath(QStringLiteral("source"));
  for (const QString& suffix : {QStringLiteral(".shp"), QStringLiteral(".shx"), QStringLiteral(".dbf"),
                               QStringLiteral(".prj"), QStringLiteral(".cpg")}) {
    if (!QFileInfo::exists(base + suffix)) continue;
    if (!QFile::copy(base + suffix, copyBase + suffix)) {
      error = QStringLiteral("%1의 인코딩 변환용 사본을 만들지 못했습니다.").arg(original.fileName());
      return nullptr;
    }
  }

  // Existing encoding helpers write a CPG and set process-wide GDAL options.
  // Apply them only to scratch files and restore both global and thread state.
  char** config = CPLGetConfigOptions();
  const char* global = CSLFetchNameValue(config, "SHAPE_ENCODING");
  const auto oldGlobal = global ? std::optional<QByteArray>(global) : std::nullopt;
  CSLDestroy(config);
  const char* local = CPLGetThreadLocalConfigOption("SHAPE_ENCODING", nullptr);
  const auto oldLocal = local ? std::optional<QByteArray>(local) : std::nullopt;
  const auto restore = qScopeGuard([oldGlobal, oldLocal]() {
    CPLSetConfigOption("SHAPE_ENCODING", oldGlobal ? oldGlobal->constData() : nullptr);
    CPLSetThreadLocalConfigOption("SHAPE_ENCODING", oldLocal ? oldLocal->constData() : nullptr);
  });
  const QString copiedShp = copyBase + QStringLiteral(".shp");
  const QString encoding = LayerOps::prepareShapefileEncoding(copiedShp);
  CPLSetThreadLocalConfigOption("SHAPE_ENCODING", encoding.toLatin1().constData());
  QgsVectorLayer source(copiedShp, layerName, QStringLiteral("ogr"));
  if (!source.isValid()) {
    error = QStringLiteral("%1 을(를) 열지 못했습니다.").arg(original.fileName());
    return nullptr;
  }
  const auto releaseScratchConnections = qScopeGuard([&source]() {
    source.dataProvider()->reloadData();
  });
  source.setProviderEncoding(encoding);

  QgsVectorFileWriter::SaveVectorOptions options;
  options.driverName = QStringLiteral("GPKG");
  options.fileEncoding = QStringLiteral("UTF-8");
  options.layerName = QStringLiteral("heritage");
  // Reserve new storage columns without changing an original field named fid/geom.
  const auto storageName = [&source](QString name) {
    while (source.fields().lookupField(name) >= 0) name += QLatin1Char('_');
    return name;
  };
  options.layerOptions = {QStringLiteral("FID=%1").arg(storageName(QStringLiteral("__hgis_fid"))),
                          QStringLiteral("GEOMETRY_NAME=%1").arg(storageName(QStringLiteral("__hgis_geom")))};
  const QString path = working.filePath(QStringLiteral("heritage.gpkg"));
  QString writerError;
  if (QgsVectorFileWriter::writeAsVectorFormatV3(&source, path, project->transformContext(), options,
                                               &writerError) != QgsVectorFileWriter::NoError) {
    error = QStringLiteral("%1의 UTF-8 작업 자료를 만들지 못했습니다: %2")
                .arg(original.fileName(), writerError);
    return nullptr;
  }
  auto result = std::make_unique<QgsVectorLayer>(path + QStringLiteral("|layername=heritage"),
                                                layerName, QStringLiteral("ogr"));
  if (!result->isValid() || result->featureCount() != source.featureCount()) {
    error = QStringLiteral("%1의 UTF-8 작업 자료를 확인하지 못했습니다.").arg(original.fileName());
    return nullptr;
  }
  // Project save/reopen needs this cache after the import call and app exit.
  working.setAutoRemove(false);
  return result.release();
}

}  // namespace

QString HeritageImport::referenceGroupName() { return QStringLiteral("참조 지도"); }

QString HeritageImport::chooseNameField(const QgsVectorLayer* layer) {
  if (!layer || !layer->isValid()) return {};
  const QgsFields fields = layer->fields();

  for (const char* candidate : kNameCandidates) {
    const int i = fields.indexOf(QString::fromUtf8(candidate));
    if (i >= 0 && looksLikeText(layer, i)) return fields.at(i).name();
  }
  // 대소문자·공백이 다를 수 있다. 느슨하게 한 번 더 본다.
  for (const char* candidate : kNameCandidates) {
    const QString want = QString::fromUtf8(candidate).toLower();
    for (int i = 0; i < fields.count(); ++i) {
      const QString have = fields.at(i).name().trimmed().toLower();
      if (have == want && looksLikeText(layer, i)) return fields.at(i).name();
    }
  }
  // 이름에 「명」이 들어간 글자 필드.
  for (int i = 0; i < fields.count(); ++i) {
    if (looksLikeText(layer, i) && fields.at(i).name().contains(QStringLiteral("명")))
      return fields.at(i).name();
  }
  // 마지막 수단: 첫 글자 필드. 그래도 못 찾으면 빈 값을 돌려주고 호출자가 알린다.
  for (int i = 0; i < fields.count(); ++i) {
    if (looksLikeText(layer, i)) return fields.at(i).name();
  }
  return {};
}

HeritageImport::Result HeritageImport::loadDataset(QgsProject* project, HeritageDataset dataset,
                                                   const QStringList& downloadedFiles,
                                                   const QString& archiveRoot) {
  Result out;
  if (!project) {
    out.error = QStringLiteral("프로젝트가 없어 자료를 올리지 못했습니다.");
    return out;
  }
  if (downloadedFiles.isEmpty()) {
    out.error = QStringLiteral("받은 파일이 없습니다.");
    return out;
  }

  // 받은 것을 보관함에 풀어 둔다. 원본은 바꾸지 않는다.
  //
  // ZIP 안의 파일 이름이 **CP949** 다(국가유산 인트라넷이 주는 ZIP, 2026-09-13 확인).
  // GDAL 의 /vsizip/ 은 기본으로 UTF-8 로 읽어 「국가지정유산」이 「?├??┴÷┴n└≫?Ω」가 된다.
  // 레이어 이름이 그대로 깨지므로 푸는 동안만 인코딩을 알려 준다.
  const char* previousZipEncoding = CPLGetThreadLocalConfigOption("CPL_ZIP_ENCODING", nullptr);
  struct ZipEncodingGuard {
    std::optional<QByteArray> previous;
    ~ZipEncodingGuard() {
      CPLSetThreadLocalConfigOption("CPL_ZIP_ENCODING", previous ? previous->constData() : nullptr);
    }
  } zipEncodingGuard{previousZipEncoding ? std::optional<QByteArray>(previousZipEncoding) : std::nullopt};
  CPLSetThreadLocalConfigOption("CPL_ZIP_ENCODING", "CP949");

  QStringList shapefiles;
  QSet<QString> archivedShapefiles;
  for (const QString& file : downloadedFiles) {
    const bool isArchive = QFileInfo(file).suffix().compare(QLatin1String("zip"), Qt::CaseInsensitive) == 0;
    const TopographicArchive::Result prepared = TopographicArchive::prepare(file, archiveRoot);
    if (!prepared.error.isEmpty()) {
      out.error = QStringLiteral("%1: %2").arg(QFileInfo(file).fileName(), prepared.error);
      out.retryableDownload = prepared.invalidArchive;
      return out;
    }
    bool hasShapefile = false;
    for (const QString& inner : prepared.files) {
      if (inner.endsWith(QStringLiteral(".shp"), Qt::CaseInsensitive)) {
        shapefiles << inner;
        hasShapefile = true;
        if (isArchive) archivedShapefiles.insert(inner);
      }
    }
    if (isArchive && !hasShapefile) {
      out.error = QStringLiteral("%1에서 SHP를 찾지 못했습니다. 자료를 다시 받아야 합니다.")
                      .arg(QFileInfo(file).fileName());
      out.retryableDownload = true;
      return out;
    }
  }
  if (shapefiles.isEmpty()) {
    out.error = QStringLiteral("받은 파일에서 SHP를 찾지 못했습니다. 파일 형식을 확인해야 합니다.");
    return out;
  }

  const QString datasetName = HeritageStyle::layerName(dataset);
  QList<QgsVectorLayer*> loaded;
  for (const QString& shp : shapefiles) {
    const QFileInfo shapeInfo(shp);
    const QString base = shapeInfo.dir().filePath(shapeInfo.completeBaseName());
    for (const QString& suffix : {QStringLiteral(".shx"), QStringLiteral(".dbf"), QStringLiteral(".prj")}) {
      if (!QFileInfo::exists(base + suffix)) {
        out.error = QStringLiteral("%1의 필수 파일 %2가 없습니다. 적재를 중단합니다.")
                        .arg(shapeInfo.fileName(), suffix);
        out.retryableDownload = archivedShapefiles.contains(shp);
        qDeleteAll(loaded);
        return out;
      }
    }
    // 한 ZIP 에 여러 SHP 가 들어온다(예: 지정유산 →
    // 국가지정유산 · 시도지정유산 · 국가등록문화유산 · 시도등록문화유산 ·
    // 국가지정유산보호구역 · 시도지정유산보호구역, 2026-09-12 실제 파일로 확인).
    // 전부 한 이름으로 올리면 레이어창에서 구분이 안 된다.
    // **이름은 파일 이름 그대로, 색과 범례는 그 종류의 것**을 쓴다.
    const QString baseName = QFileInfo(shp).completeBaseName();
    const QString layerName = baseName.isEmpty() ? datasetName : baseName;
    auto* layer = utf8WorkingLayer(shp, layerName, archiveRoot, project, out.error);
    if (!layer) {
      qDeleteAll(loaded);
      return out;
    }

    // 유적명은 실제 필드에서 고른다. 없으면 그렇다고 말한다.
    const QString nameField = chooseNameField(layer);
    if (nameField.isEmpty()) {
      out.messages << QStringLiteral("%1: 유적명 컬럼을 찾지 못해 범례에 이름을 넣지 못했습니다.")
                          .arg(layerName);
    }
    // 한 종류는 한 색이다. 지정유산 안의 6종도 모두 지정유산 색을 쓴다
    // (레이어창에서는 「지정유산」 그룹으로 묶어 구분한다).
    const HeritageStyleResult styled = HeritageStyle::apply(layer, dataset, nameField);
    if (!styled.message.isEmpty()) out.messages << styled.message;
    // 레이어창에는 종류 이름만, 도면 범례에는 유적명 한 줄씩.
    HeritageSiteLegend::install(layer);

    // 참조 자료다. 조사 데이터와 섞이지 않게 한다.
    layer->setProperty("readOnly", true);
    LayerOps::markReferenceLayer(layer);
    out.featureCount += static_cast<int>(layer->featureCount());
    loaded.append(layer);
  }

  if (loaded.isEmpty()) {
    out.error = QStringLiteral("%1 자료를 지도에 올리지 못했습니다.").arg(datasetName);
    return out;
  }

  // 한 번에 등록한다. 하나씩 넣으면 그때마다 화면이 다시 그려진다.
  // **범례에 자동으로 넣지 않는다**(addToLegend=false). 「참조 지도」 그룹에 직접 넣기 위해서다.
  QList<QgsMapLayer*> asMapLayers;
  for (QgsVectorLayer* layer : loaded) asMapLayers.append(layer);
  project->addMapLayers(asMapLayers, false);

  // 조사 데이터와 섞이지 않게 「참조 지도」 그룹에 모은다.
  // LayerOps::placeInLegendGroup 은 그룹 이름을 쓰지 않으므로(Q_UNUSED) 여기서 직접 만든다.
  QgsLayerTree* root = project->layerTreeRoot();
  if (root) {
    QgsLayerTreeGroup* reference = root->findGroup(referenceGroupName());
    if (!reference) reference = root->addGroup(referenceGroupName());
    // 한 ZIP 에 그 종류의 여러 갈래가 들어온다(지정유산 → 국가지정유산·시도지정유산·…).
    // 레이어창에서 종류별로 묶어 준다: 참조 지도 › 지정유산 › 국가지정유산 …
    QgsLayerTreeGroup* group = reference;
    if (reference) {
      const QString kind = HeritageStyle::layerName(dataset);
      QgsLayerTreeGroup* kindGroup = reference->findGroup(kind);
      if (!kindGroup) kindGroup = reference->addGroup(kind);
      if (kindGroup) group = kindGroup;
    }
    for (QgsVectorLayer* layer : loaded) {
      if (!group) break;
      QgsLayerTreeLayer* node = group->addLayer(layer);
      if (!node) continue;
      node->setItemVisibilityChecked(true);
      // 유적명이 수백 줄이면 레이어창을 덮는다. 접은 채로 올린다.
      node->setExpanded(false);
    }
  }

  LayerOps::ensureSatelliteAtBottom(project);
  out.layers = loaded;
  return out;
}
