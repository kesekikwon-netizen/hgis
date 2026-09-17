#include "SurveyStorage.h"
#include "LayerOps.h"

#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScopeGuard>
#include <QSet>
#include <QTemporaryDir>
#include <memory>

#include <qgis.h>
#include <qgscoordinatetransformcontext.h>
#include <qgsproject.h>
#include <qgslayertree.h>
#include <qgsmaplayerstyle.h>
#include <qgsrasterlayer.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectorlayer.h>

#include <gdal.h>
#include <cpl_error.h>
#include <ogr_api.h>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

// GeoPackage 테이블 이름으로 안전한 형태. 한글은 그대로 둔다(GPKG는 UTF-8 식별자 허용).
QString sanitizeLayerName(const QString& name) {
  QString out = name;
  out.replace(QRegularExpression(QStringLiteral("[^\\w\\s가-힣._-]")), QStringLiteral("_"));
  out.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral("_"));
  out = out.trimmed();
  if (out.isEmpty()) out = QStringLiteral("layer");
  // 숫자로 시작하는 테이블 이름은 일부 SQL 경로에서 다루기 번거롭다.
  if (out.at(0).isDigit()) out.prepend(QLatin1Char('L'));
  return out.left(60);
}

// 이 레이어가 이미 조사 .gpkg 안에 있는가. source는 "경로|layername=..." 형태다.
bool livesInGpkg(const QgsVectorLayer* layer, const QString& gpkgPath) {
  if (!layer) return false;
  const QString src = layer->source();
  const QString file = src.section(QLatin1Char('|'), 0, 0);
  if (file.isEmpty()) return false;
  return QFileInfo(file).absoluteFilePath().compare(QFileInfo(gpkgPath).absoluteFilePath(),
                                                    Qt::CaseInsensitive) == 0;
}

QSet<QString> existingGpkgLayerNames(const QString& gpkgPath) {
  QSet<QString> names;
  GDALDatasetH ds = GDALOpenEx(gpkgPath.toUtf8().constData(), GDAL_OF_VECTOR, nullptr, nullptr,
                               nullptr);
  if (!ds) return names;
  const int n = GDALDatasetGetLayerCount(ds);
  for (int i = 0; i < n; ++i) {
    OGRLayerH l = GDALDatasetGetLayer(ds, i);
    if (l) names.insert(QString::fromUtf8(OGR_L_GetName(l)));
  }
  GDALClose(ds);
  return names;
}

bool readProjectPlain(QgsProject* project, const QString& uri, bool loadLayouts) {
  return project->read(uri, loadLayouts ? Qgis::ProjectReadFlags()
                                       : Qgis::ProjectReadFlags(Qgis::ProjectReadFlag::DontLoadLayouts));
}

// SEH를 쓰는 함수에는 소멸자가 필요한 지역 객체를 둘 수 없다(C2712). 그래서 감시만
// 하는 얇은 함수로 분리한다.
bool readGuarded(QgsProject* project, const QString& uri, bool* crashed, bool loadLayouts) {
#ifdef Q_OS_WIN
  __try {
    return readProjectPlain(project, uri, loadLayouts);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    *crashed = true;
    return false;
  }
#else
  return readProjectPlain(project, uri, loadLayouts);
#endif
}

}  // namespace

namespace SurveyStorage {

QString projectUri(const QString& gpkgPath) {
  if (gpkgPath.isEmpty()) return {};
  return QStringLiteral("geopackage:%1?projectName=survey")
      .arg(QFileInfo(gpkgPath).absoluteFilePath());
}

bool hasEmbeddedProject(const QString& gpkgPath) {
  if (gpkgPath.isEmpty() || !QFileInfo::exists(gpkgPath)) return false;
  GDALDatasetH ds = GDALOpenEx(gpkgPath.toUtf8().constData(), GDAL_OF_VECTOR, nullptr, nullptr,
                               nullptr);
  if (!ds) return false;
  // QGIS는 내장 프로젝트를 qgis_projects 테이블에 넣는다. 지오메트리가 없는 테이블이라
  // 레이어 목록에는 안 잡히므로 SQL로 직접 확인한다.
  OGRLayerH res = GDALDatasetExecuteSQL(
      ds, "SELECT name FROM sqlite_master WHERE type='table' AND name='qgis_projects'", nullptr,
      nullptr);
  bool found = false;
  if (res) {
    found = OGR_L_GetFeatureCount(res, 1) > 0;
    GDALDatasetReleaseResultSet(ds, res);
  }
  GDALClose(ds);
  return found;
}

bool validateForOpen(const QString& gpkgPath, QString* errorOut) {
  if (errorOut) errorOut->clear();
  const char* drivers[] = {"GPKG", nullptr};
  GDALDatasetH dataset = GDALOpenEx(gpkgPath.toUtf8().constData(),
      GDAL_OF_VECTOR | GDAL_OF_READONLY, drivers, nullptr, nullptr);
  if (!dataset) {
    if (errorOut) *errorOut = QStringLiteral("읽을 수 있는 GeoPackage 조사 파일이 아닙니다.");
    return false;
  }
  OGRLayerH result = GDALDatasetExecuteSQL(dataset, "PRAGMA quick_check", nullptr, nullptr);
  OGRFeatureH check = result ? OGR_L_GetNextFeature(result) : nullptr;
  const bool valid = check && QString::fromUtf8(OGR_F_GetFieldAsString(check, 0)) == QLatin1String("ok");
  if (check) OGR_F_Destroy(check);
  if (result) GDALDatasetReleaseResultSet(dataset, result);
  GDALClose(dataset);
  if (!valid && errorOut)
    *errorOut = QStringLiteral("조사 파일의 무결성을 확인하지 못했습니다. 현재 작업은 유지됩니다.");
  return valid;
}

bool copySurvey(const QString& sourceGpkg, const QString& targetGpkg, QString* errorOut) {
  if (errorOut) errorOut->clear();
  const auto fail = [errorOut](const QString& message) {
    if (errorOut) *errorOut = message;
    return false;
  };
  const QFileInfo source(sourceGpkg), target(targetGpkg);
  if (sourceGpkg.isEmpty() || targetGpkg.isEmpty() || !source.isFile())
    return fail(QStringLiteral("복사할 조사 파일 또는 저장 경로가 없습니다."));
  if (source.canonicalFilePath().compare(target.canonicalFilePath(), Qt::CaseInsensitive) == 0 ||
      source.absoluteFilePath().compare(target.absoluteFilePath(), Qt::CaseInsensitive) == 0)
    return true;
  const auto targetHasJournal = [&target]() {
    for (const QString& suffix : {QStringLiteral("-wal"), QStringLiteral("-shm"), QStringLiteral("-journal")})
      if (QFileInfo::exists(target.absoluteFilePath() + suffix)) return true;
    return false;
  };
  if (targetHasJournal())
    return fail(QStringLiteral("저장 대상 조사 파일이 사용 중입니다. 해당 파일을 닫고 다시 저장하세요."));
  QTemporaryDir temporary(target.dir().filePath(QStringLiteral(".ka-survey-copy-XXXXXX")));
  if (!temporary.isValid()) return fail(QStringLiteral("저장 폴더에 임시 사본을 만들 수 없습니다."));
  const QString snapshot = temporary.filePath(QStringLiteral("survey.gpkg"));
  GDALDatasetH dataset = GDALOpenEx(source.absoluteFilePath().toUtf8().constData(), GDAL_OF_VECTOR,
                                    nullptr, nullptr, nullptr);
  if (!dataset) return fail(QStringLiteral("원본 조사 파일을 읽을 수 없습니다."));
  // SQLite VACUUM INTO takes a consistent read snapshot, including committed WAL,
  // without modifying the source or relying on a byte copy of its main file.
  QString escaped = snapshot;
  escaped.replace(QLatin1Char('\''), QStringLiteral("''"));
  const QByteArray sql = QStringLiteral("VACUUM INTO '%1'").arg(escaped).toUtf8();
  CPLErrorReset();
  OGRLayerH result = GDALDatasetExecuteSQL(dataset, sql.constData(), nullptr, nullptr);
  const bool copied = CPLGetLastErrorType() < CE_Failure;
  const QString copyError = QString::fromUtf8(CPLGetLastErrorMsg());
  if (result) GDALDatasetReleaseResultSet(dataset, result);
  GDALClose(dataset);
  if (!copied || QFileInfo(snapshot).size() <= 0)
    return fail(QStringLiteral("조사 파일 사본을 만들지 못했습니다: %1").arg(copyError));
  QFile input(snapshot);
  QSaveFile output(target.absoluteFilePath());
  if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly))
    return fail(QStringLiteral("저장 대상에 쓸 수 없습니다: %1").arg(output.errorString()));
  while (!input.atEnd()) {
    const QByteArray data = input.read(1024 * 1024);
    if (input.error() != QFileDevice::NoError || output.write(data) != data.size())
      return fail(QStringLiteral("조사 파일 사본을 기록하지 못했습니다."));
  }
  if (targetHasJournal())
    return fail(QStringLiteral("저장 도중 대상 조사 파일이 열려 저장을 멈췄습니다."));
  if (!output.commit()) return fail(QStringLiteral("저장 파일을 교체하지 못했습니다: %1").arg(output.errorString()));
  return true;
}

QString writeRecoverySnapshot(QgsProject* project, const QString& recoveryDirectory, QString* errorOut) {
  if (errorOut) errorOut->clear();
  const auto fail = [errorOut](const QString& message) -> QString {
    if (errorOut) *errorOut = message;
    return {};
  };
  if (!project || recoveryDirectory.isEmpty())
    return fail(QStringLiteral("복구 사본을 보관할 프로젝트 또는 폴더가 없습니다."));
  if (!QDir().mkpath(recoveryDirectory))
    return fail(QStringLiteral("복구 사본 폴더를 만들 수 없습니다. 다른 저장 위치를 선택하세요."));
  // Keep the unique directory at its original location: embedded relative sources and home paths
  // must remain valid. It becomes a retained recovery result only after all writes are verified.
  QTemporaryDir output(QDir(recoveryDirectory).filePath(
      QStringLiteral("조사복구_%1_XXXXXX").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")))));
  if (!output.isValid()) return fail(QStringLiteral("복구 사본을 만들 공간이나 쓰기 권한이 없습니다."));
  const QString path = output.filePath(QStringLiteral("복구조사.gpkg"));
  try {
    {
      QgsProject recovered;
      recovered.setTitle(project->title());
      recovered.setCrs(project->crs());
      recovered.setTransformContext(project->transformContext());
      QSet<QString> tables;
      QStringList externalReferences;
      bool firstVector = true;
      for (QgsMapLayer* source : project->mapLayers()) {
        if (!source) continue;
        std::unique_ptr<QgsMapLayer> copied;
        if (auto* vector = qobject_cast<QgsVectorLayer*>(source)) {
          if (!vector->isValid())
            return fail(QStringLiteral("%1을 읽을 수 없어 편집 도형 복구 사본을 완성하지 못했습니다.")
                            .arg(source->name()));
          const QString key = LayerOps::layerKeyOf(source);
          const QString base = sanitizeLayerName(key.isEmpty() ? source->name() : key);
          QString table = base;
          for (int suffix = 2; tables.contains(table.toCaseFolded()); ++suffix)
            table = base + QStringLiteral("_%1").arg(suffix);
          tables.insert(table.toCaseFolded());
          QgsVectorFileWriter::SaveVectorOptions options;
          options.driverName = QStringLiteral("GPKG");
          options.fileEncoding = QStringLiteral("UTF-8");
          options.layerName = table;
          options.actionOnExistingFile = firstVector ? QgsVectorFileWriter::CreateOrOverwriteFile
                                                     : QgsVectorFileWriter::CreateOrOverwriteLayer;
          const auto storageName = [vector](QString name) {
            while (vector->fields().lookupField(name) >= 0) name += QLatin1Char('_');
            return name;
          };
          // Pending added features can have a NULL fid while existing features already use fid=1.
          // Preserve the original fid as an attribute and reserve a different storage primary key.
          options.layerOptions = {QStringLiteral("FID=%1").arg(storageName(QStringLiteral("__hgis_recovery_fid"))),
                                  QStringLiteral("GEOMETRY_NAME=%1").arg(storageName(QStringLiteral("__hgis_recovery_geom")))};
          QString detail;
          if (QgsVectorFileWriter::writeAsVectorFormatV3(vector, path, project->transformContext(), options,
                                                        &detail) != QgsVectorFileWriter::NoError)
            return fail(QStringLiteral("%1의 편집을 복구 사본에 기록하지 못했습니다: %2").arg(source->name(), detail));
          firstVector = false;
          auto snapshot = std::make_unique<QgsVectorLayer>(
              path + QStringLiteral("|layername=%1").arg(table), source->name(), QStringLiteral("ogr"));
          if (!snapshot->isValid() || snapshot->featureCount() != vector->featureCount() ||
              snapshot->crs() != vector->crs())
            return fail(QStringLiteral("%1의 복구 피처 수 또는 좌표계를 확인하지 못했습니다.").arg(source->name()));
          for (const QgsField& field : vector->fields())
            if (snapshot->fields().lookupField(field.name()) < 0)
              return fail(QStringLiteral("%1의 복구 속성이 누락되었습니다: %2").arg(source->name(), field.name()));
          QgsMapLayerStyle style;
          style.readFromLayer(vector);
          style.writeToLayer(snapshot.get());
          copied = std::move(snapshot);
        } else {
          copied.reset(source->clone()); // Raster/reference sources stay external; never rewrite them.
          externalReferences << source->name() + (source->isValid()
              ? QStringLiteral(" (외부 원본 참조)") : QStringLiteral(" (외부 원본 확인 필요)"));
        }
        if (!copied || !copied->setId(source->id()))
          return fail(QStringLiteral("%1의 복구 레이어 구성을 만들지 못했습니다.").arg(source->name()));
        for (const QString& key : source->customPropertyKeys())
          copied->setCustomProperty(key, source->customProperty(key));
        recovered.addMapLayer(copied.release(), false);
      }
      if (firstVector) return fail(QStringLiteral("복구 사본에 보관할 벡터 레이어가 없습니다."));
      const auto copyTree = [&](auto&& self, QgsLayerTreeGroup* from, QgsLayerTreeGroup* to) -> void {
        to->setItemVisibilityChecked(from->itemVisibilityChecked());
        to->setExpanded(from->isExpanded());
        for (QgsLayerTreeNode* node : from->children()) {
          if (auto* group = qobject_cast<QgsLayerTreeGroup*>(node)) {
            self(self, group, to->addGroup(group->name()));
          } else if (auto* layerNode = qobject_cast<QgsLayerTreeLayer*>(node)) {
            if (QgsMapLayer* layer = recovered.mapLayer(layerNode->layerId())) {
              auto* added = to->addLayer(layer);
              added->setItemVisibilityChecked(node->itemVisibilityChecked());
              added->setExpanded(node->isExpanded());
            }
          }
        }
      };
      copyTree(copyTree, project->layerTreeRoot(), recovered.layerTreeRoot());
      for (QgsMapLayer* layer : recovered.mapLayers())
        if (!recovered.layerTreeRoot()->findLayer(layer->id())) recovered.layerTreeRoot()->addLayer(layer);
      recovered.writeEntry(QStringLiteral("ka_hgis"), QStringLiteral("recovery_external_references"),
                            externalReferences);
      QString detail;
      if (!writeEmbedded(&recovered, path, &detail))
        return fail(QStringLiteral("복구 사본에 레이어 구성을 기록하지 못했습니다: %1").arg(detail));
    }
    QString detail;
    if (!validateForOpen(path, &detail))
      return fail(QStringLiteral("복구 사본 파일을 다시 확인하지 못했습니다: %1").arg(detail));
    output.setAutoRemove(false);
    return path;
  } catch (...) {
    return fail(QStringLiteral("복구 사본을 만드는 중 오류가 발생했습니다. 현재 창을 닫지 말고 다시 저장하세요."));
  }
}

AbsorbResult absorbExternalVectors(QgsProject* project, const QString& gpkgPath) {
  AbsorbResult r;
  if (!project || gpkgPath.isEmpty() || !QFileInfo::exists(gpkgPath)) return r;

  QSet<QString> used = existingGpkgLayerNames(gpkgPath);
  const QList<QgsMapLayer*> layers = project->mapLayers().values();
  for (QgsMapLayer* ml : layers) {
    if (!ml || !ml->isValid()) continue;
    if (qobject_cast<QgsRasterLayer*>(ml)) {
      // 래스터(스크린샷·항공사진)는 아직 바깥에 둔다. .gpkg 타일로 굽는 것은 되돌릴 수
      // 없는 변환이라 사용자가 원할 때만 해야 한다.
      const QString src = ml->source();
      if (QFileInfo::exists(src.section(QLatin1Char('|'), 0, 0)))
        r.skippedRaster << ml->name();
      continue;
    }
    auto* vl = qobject_cast<QgsVectorLayer*>(ml);
    if (!vl) continue;
    const QString provider = vl->providerType().toLower();
    // 파일에서 온 것(ogr)과 메모리 레이어만 대상. xyz/wms 배경지도는 파일이 아니다.
    if (provider != QLatin1String("ogr") && provider != QLatin1String("memory")) continue;
    if (provider == QLatin1String("ogr") && livesInGpkg(vl, gpkgPath)) continue;

    QString target = sanitizeLayerName(vl->name());
    int suffix = 2;
    while (used.contains(target)) target = sanitizeLayerName(vl->name()) + QStringLiteral("_%1").arg(suffix++);

    QgsVectorFileWriter::SaveVectorOptions opt;
    opt.driverName = QStringLiteral("GPKG");
    opt.layerName = target;
    opt.actionOnExistingFile = QgsVectorFileWriter::CreateOrOverwriteLayer;
    opt.fileEncoding = QStringLiteral("UTF-8");
    QString err;
    QString newFile;
    QString newLayer;
    const QgsVectorFileWriter::WriterError code = QgsVectorFileWriter::writeAsVectorFormatV3(
        vl, gpkgPath, project->transformContext(), opt, &err, &newFile, &newLayer);
    if (code != QgsVectorFileWriter::NoError) {
      r.failed << vl->name();
      continue;
    }
    used.insert(target);
    const QString storedSource = QStringLiteral("%1|layername=%2").arg(gpkgPath, target);
    QgsVectorLayer probe(storedSource, vl->name(), QStringLiteral("ogr"));
    if (!probe.isValid() || probe.featureCount() != vl->featureCount()) {
      r.failed << vl->name();
      continue;
    }
    // 스타일·라벨은 레이어 객체에 남아 있으므로 데이터소스만 사본으로 돌린다.
    const QString name = vl->name();
    vl->setDataSource(storedSource, name,
                      QStringLiteral("ogr"));
    if (vl->isValid())
      r.imported << name;
    else
      r.failed << name;
  }
  return r;
}

bool writeEmbedded(QgsProject* project, const QString& gpkgPath, QString* errorOut) {
  if (!project || gpkgPath.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("저장 경로가 없습니다.");
    return false;
  }
  if (!QFileInfo::exists(gpkgPath)) {
    if (errorOut) *errorOut = QStringLiteral("조사 파일이 없습니다: %1").arg(gpkgPath);
    return false;
  }
  const QString abs = QFileInfo(gpkgPath).absoluteFilePath();
  const QString previousFileName = project->fileName();
  const QString previousHome = project->presetHomePath();
  const bool wasDirty = project->isDirty();
  bool written = false;
  const auto restoreOnFailure = qScopeGuard([&] {
    if (!written) {
      project->setFileName(previousFileName);
      project->setPresetHomePath(previousHome);
      project->setDirty(wasDirty);
    }
  });
  project->setFileName(abs);
  project->setPresetHomePath(QFileInfo(abs).absolutePath());
  const QString uri = projectUri(gpkgPath);
  if (!project->write(uri)) {
    if (errorOut) *errorOut = project->error();
    return false;
  }
  LayerOps::saveGpkgDefaultStyles(project, abs);
  written = true;
  return true;
}

bool readEmbedded(QgsProject* project, const QString& gpkgPath, bool* crashedOut,
                  QString* errorOut, bool loadLayouts) {
  if (crashedOut) *crashedOut = false;
  if (!project || gpkgPath.isEmpty() || !QFileInfo::exists(gpkgPath)) {
    if (errorOut) *errorOut = QStringLiteral("조사 파일이 없습니다.");
    return false;
  }
  const QString abs = QFileInfo(gpkgPath).absoluteFilePath();
  project->setFileName(abs);
  project->setPresetHomePath(QFileInfo(abs).absolutePath());
  const QString uri = projectUri(gpkgPath);
  bool crashed = false;
  const bool ok = readGuarded(project, uri, &crashed, loadLayouts);
  if (crashedOut) *crashedOut = crashed;
  if (!ok && errorOut)
    *errorOut = crashed ? QStringLiteral("조사 파일 안의 작업공간을 읽다 오류가 났습니다.")
                        : project->error();
  return ok;
}

}  // namespace SurveyStorage
