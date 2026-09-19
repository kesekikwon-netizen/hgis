#include "KaSessionLog.h"
#include "SurveyStorage.h"
#include "LayerOps.h"

#include <algorithm>
#include <memory>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScopeGuard>
#include <QThread>
#include <QSet>
#include <QTemporaryDir>

#include <qgis.h>
#include <qgscoordinatetransformcontext.h>
#include <qgsproject.h>
#include <qgslayertree.h>
#include <qgsmaplayerstyle.h>
#include <qgsrasterlayer.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectorlayer.h>
#include <qgsogrproviderutils.h>

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

QString gpkgTableName(const QgsVectorLayer* layer) {
  if (!layer) return {};
  const QString src = layer->source();
  const int mark = src.indexOf(QLatin1String("layername="), 0, Qt::CaseInsensitive);
  if (mark >= 0) return src.mid(mark + 10).section(QLatin1Char('|'), 0, 0);
  return sanitizeLayerName(layer->name());
}

bool writeLayerToGpkg(QgsVectorLayer* layer, const QString& gpkgPath, const QString& table,
                      const QgsCoordinateTransformContext& transform, QString* errorOut) {
  QgsVectorFileWriter::SaveVectorOptions opt;
  opt.driverName = QStringLiteral("GPKG");
  opt.layerName = table;
  opt.actionOnExistingFile = QgsVectorFileWriter::CreateOrOverwriteLayer;
  opt.fileEncoding = QStringLiteral("UTF-8");
  QString detail;
  if (QgsVectorFileWriter::writeAsVectorFormatV3(layer, gpkgPath, transform, opt, &detail) !=
      QgsVectorFileWriter::NoError) {
    if (errorOut) *errorOut = detail;
    return false;
  }
  return true;
}

void retargetGpkgLayers(QgsProject* project, const QString& fromPath, const QString& toPath) {
  if (!project || fromPath.isEmpty() || toPath.isEmpty()) return;
  for (QgsMapLayer* ml : project->mapLayers()) {
    auto* vl = qobject_cast<QgsVectorLayer*>(ml);
    if (!vl || !livesInGpkg(vl, fromPath)) continue;
    const QString table = gpkgTableName(vl);
    if (table.isEmpty()) continue;
    const bool editing = vl->isEditable();
    vl->setDataSource(QStringLiteral("%1|layername=%2").arg(toPath, table), vl->name(),
                      QStringLiteral("ogr"));
    if (editing && vl->isValid() && !vl->isEditable()) vl->startEditing();
  }
}

void restoreLayerSources(QgsProject* project, const QHash<QString, QString>& sources) {
  if (!project) return;
  for (QgsMapLayer* ml : project->mapLayers()) {
    auto* vl = qobject_cast<QgsVectorLayer*>(ml);
    if (!vl || !sources.contains(vl->id())) continue;
    if (vl->isValid() && (vl->isModified() || vl->featureCount() > 0)) continue;
    const QString want = sources.value(vl->id());
    if (vl->source() == want) continue;
    const bool editing = vl->isEditable();
    vl->setDataSource(want, vl->name(), vl->providerType());
    if (editing && vl->isValid() && !vl->isEditable()) vl->startEditing();
  }
}

void dropIdleJournals(const QString& gpkgPath) {
  if (gpkgPath.isEmpty()) return;
  for (const QString& suffix : {QStringLiteral("-wal"), QStringLiteral("-shm"),
                                QStringLiteral("-journal")}) {
    const QString side = gpkgPath + suffix;
    if (QFileInfo::exists(side)) QFile::remove(side);
  }
}

QList<QgsVectorLayer*> embeddedReferenceLayers(QgsProject* project, const QString& gpkgPath) {
  QList<QgsVectorLayer*> out;
  if (!project || gpkgPath.isEmpty()) return out;
  const QStringList domain = LayerOps::domainLayerKeys();
  for (QgsMapLayer* ml : project->mapLayers()) {
    auto* vl = qobject_cast<QgsVectorLayer*>(ml);
    if (!vl || !vl->isValid()) continue;
    if (!LayerOps::isReferenceLayer(vl)) continue;
    const QString key = LayerOps::layerKeyOf(vl);
    if (!key.isEmpty() && domain.contains(key)) continue;
    if (vl->providerType().compare(QLatin1String("ogr"), Qt::CaseInsensitive) != 0) continue;
    if (!livesInGpkg(vl, gpkgPath)) continue;
    out << vl;
  }
  return out;
}

bool dropGpkgTables(const QString& gpkgPath, const QStringList& tables, QString* errorOut) {
  if (tables.isEmpty()) return true;
  GDALDatasetH ds = GDALOpenEx(gpkgPath.toUtf8().constData(), GDAL_OF_VECTOR | GDAL_OF_UPDATE,
                               nullptr, nullptr, nullptr);
  if (!ds) {
    if (errorOut) *errorOut = QStringLiteral("조사 파일을 수정용으로 열지 못했습니다.");
    return false;
  }
  for (const QString& table : tables) {
    const int n = GDALDatasetGetLayerCount(ds);
    int index = -1;
    for (int i = 0; i < n; ++i) {
      OGRLayerH layer = GDALDatasetGetLayer(ds, i);
      if (layer && QString::fromUtf8(OGR_L_GetName(layer)).compare(table, Qt::CaseInsensitive) == 0) {
        index = i;
        break;
      }
    }
    if (index < 0) continue;
    if (GDALDatasetDeleteLayer(ds, index) != OGRERR_NONE) {
      if (errorOut)
        *errorOut = QStringLiteral("%1 테이블을 조사 파일에서 빼지 못했습니다.").arg(table);
      GDALClose(ds);
      return false;
    }
  }
  CPLErrorReset();
  OGRLayerH vacuum = GDALDatasetExecuteSQL(ds, "VACUUM", nullptr, nullptr);
  if (vacuum) GDALDatasetReleaseResultSet(ds, vacuum);
  GDALClose(ds);
  return true;
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

// 다 쓴 사본으로 원본을 교체한다. Windows 는 백신·색인이나 방금 닫힌 핸들이 대상
// 파일을 잠깐 잡고 있으면 첫 시도를 "액세스가 거부되었습니다"로 거부한다. 사본은
// 그대로 두고 교체만 짧게 다시 시도한다. 잠금과 무관한 실패는 즉시 알린다.
bool replaceWithStaged(const QString& staged, const QString& target, QString* errorOut) {
  for (int attempt = 0; attempt < 20; ++attempt) {
    if (attempt) QThread::msleep(attempt < 10 ? 100 : 300);
#ifdef Q_OS_WIN
    const QString from = QDir::toNativeSeparators(staged);
    const QString to = QDir::toNativeSeparators(target);
    if (MoveFileExW(reinterpret_cast<LPCWSTR>(from.utf16()), reinterpret_cast<LPCWSTR>(to.utf16()),
                    MOVEFILE_REPLACE_EXISTING))
      return true;
    const DWORD code = GetLastError();
    if (errorOut) {
      wchar_t* text = nullptr;
      FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                         FORMAT_MESSAGE_IGNORE_INSERTS,
                     nullptr, code, 0, reinterpret_cast<LPWSTR>(&text), 0, nullptr);
      *errorOut = text ? QString::fromWCharArray(text).trimmed()
                       : QStringLiteral("교체 오류 %1").arg(static_cast<int>(code));
      if (text) LocalFree(text);
    }
    if (code != ERROR_ACCESS_DENIED && code != ERROR_SHARING_VIOLATION &&
        code != ERROR_LOCK_VIOLATION)
      return false;
#else
    if (QFile::exists(target) && !QFile::remove(target)) {
      if (errorOut) *errorOut = QStringLiteral("기존 파일을 비우지 못했습니다.");
      continue;
    }
    if (QFile::rename(staged, target)) return true;
    if (errorOut) *errorOut = QStringLiteral("이름을 바꾸지 못했습니다.");
#endif
  }
  return false;
}

// 이름 교체를 끝까지 거부당하면(대상 파일을 잡은 쪽이 쓰기는 허용하는 경우) 같은
// 파일에 내용을 그대로 덮어쓴다. 사본은 이 쓰기가 끝날 때까지 남겨 두므로, 중간에
// 끊겨도 옆의 .ka-new 파일에 검증된 새 세대가 온전히 남는다.
bool overwriteInPlace(const QString& staged, const QString& target, QString* errorOut) {
  QFile source(staged);
  QFile destination(target);
  if (!source.open(QIODevice::ReadOnly) ||
      !destination.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    if (errorOut) *errorOut = destination.errorString();
    return false;
  }
  while (!source.atEnd()) {
    const QByteArray data = source.read(1024 * 1024);
    if (source.error() != QFileDevice::NoError || destination.write(data) != data.size()) {
      if (errorOut) *errorOut = destination.errorString();
      return false;
    }
  }
  const bool flushed = destination.flush();
  destination.close();
  source.close();
  if (!flushed) {
    if (errorOut) *errorOut = QStringLiteral("덮어쓴 내용을 끝까지 기록하지 못했습니다.");
    return false;
  }
  return QFileInfo(target).size() == QFileInfo(staged).size();
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
  // 대상 옆에 새 세대를 먼저 다 쓴다. 다 쓰기 전에는 원본을 건드리지 않는다.
  const QString staged = target.absoluteFilePath() + QStringLiteral(".ka-new");
  QFile::remove(staged);
  const auto dropStaged = [&staged] { QFile::remove(staged); };
  QFile input(snapshot);
  QFile output(staged);
  if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly)) {
    dropStaged();
    return fail(QStringLiteral("저장 대상에 쓸 수 없습니다: %1").arg(output.errorString()));
  }
  while (!input.atEnd()) {
    const QByteArray data = input.read(1024 * 1024);
    if (input.error() != QFileDevice::NoError || output.write(data) != data.size()) {
      output.close();
      dropStaged();
      return fail(QStringLiteral("조사 파일 사본을 기록하지 못했습니다."));
    }
  }
  const bool flushed = output.flush();
  output.close();
  input.close();
  if (!flushed) {
    dropStaged();
    return fail(QStringLiteral("조사 파일 사본을 끝까지 기록하지 못했습니다."));
  }
  if (targetHasJournal()) {
    dropStaged();
    return fail(QStringLiteral("저장 도중 대상 조사 파일이 열려 저장을 멈췄습니다."));
  }
  QString replaceError;
  if (!replaceWithStaged(staged, target.absoluteFilePath(), &replaceError)) {
    KaSessionLog::line(QStringLiteral("[save] 이름 교체 거부 — 같은 파일에 덮어쓴다: %1")
                           .arg(replaceError));
    QString overwriteError;
    if (!overwriteInPlace(staged, target.absoluteFilePath(), &overwriteError)) {
      return fail(QStringLiteral("저장 파일을 교체하지 못했습니다: %1 · 새로 만든 조사 파일은 "
                                 "%2 에 남겨 두었습니다.")
                      .arg(overwriteError.isEmpty() ? replaceError : overwriteError,
                           QDir::toNativeSeparators(staged)));
    }
  }
  dropStaged();
  return true;
}

bool publishSurveyGeneration(const QString& generationGpkg, const QString& targetGpkg,
                             QString* errorOut) {
  if (!validateForOpen(generationGpkg, errorOut)) return false;
  return copySurvey(generationGpkg, targetGpkg, errorOut);
}

bool noteRecoveryPending(const QString& recoveryDirectory, const QString& snapshotPath, QString* errorOut) {
  if (errorOut) errorOut->clear();
  const auto fail = [errorOut](const QString& message) {
    if (errorOut) *errorOut = message;
    return false;
  };
  if (recoveryDirectory.isEmpty() || snapshotPath.isEmpty() || !QFileInfo::exists(snapshotPath))
    return fail(QStringLiteral("복구 사본 경로가 없습니다."));
  const QString root = QDir::cleanPath(QFileInfo(recoveryDirectory).absoluteFilePath());
  const QString file = QDir::cleanPath(QFileInfo(snapshotPath).absoluteFilePath());
  if (!file.startsWith(root + QLatin1Char('/'), Qt::CaseInsensitive))
    return fail(QStringLiteral("복구 사본이 복구 폴더 밖에 있습니다."));
  if (!QDir().mkpath(root))
    return fail(QStringLiteral("복구 사본 폴더를 만들 수 없습니다."));
  QSaveFile output(QDir(root).filePath(QStringLiteral("pending.txt")));
  if (!output.open(QIODevice::WriteOnly) || output.write(file.toUtf8()) != file.toUtf8().size() || !output.commit())
    return fail(QStringLiteral("복구 사본 표시를 기록하지 못했습니다."));
  return true;
}

QString pendingRecoverySnapshot(const QString& recoveryDirectory) {
  if (recoveryDirectory.isEmpty()) return {};
  QFile file(QDir(recoveryDirectory).filePath(QStringLiteral("pending.txt")));
  if (!file.open(QIODevice::ReadOnly)) return {};
  const QString path = QDir::cleanPath(QString::fromUtf8(file.readAll()).trimmed());
  return !path.isEmpty() && QFileInfo::exists(path) ? path : QString();
}

void clearRecoveryPending(const QString& recoveryDirectory) {
  if (!recoveryDirectory.isEmpty())
    QFile::remove(QDir(recoveryDirectory).filePath(QStringLiteral("pending.txt")));
}

int pruneRecoverySnapshots(const QString& recoveryDirectory, int keep, const QString& protectPath) {
  if (recoveryDirectory.isEmpty() || keep < 1) return 0;
  QDir dir(recoveryDirectory);
  if (!dir.exists()) return 0;
  const QString protect = QDir::cleanPath(QFileInfo(protectPath).absolutePath());
  struct Item {
    QString path;
    QDateTime modified;
  };
  QList<Item> items;
  const QFileInfoList infos = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
  for (const QFileInfo& info : infos) {
    if (!info.fileName().startsWith(QStringLiteral("조사복구_"))) continue;
    const QFileInfo gpkg(QDir(info.absoluteFilePath()).filePath(QStringLiteral("복구조사.gpkg")));
    if (!gpkg.exists()) continue;
    items.append({QDir::cleanPath(info.absoluteFilePath()), gpkg.lastModified()});
  }
  std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.modified > b.modified; });
  int kept = 0;
  int removed = 0;
  for (const Item& item : items) {
    if (item.path.compare(protect, Qt::CaseInsensitive) == 0 || kept < keep) {
      ++kept;
      continue;
    }
    if (QDir(item.path).removeRecursively()) ++removed;
  }
  return removed;
}

QString writeRecoverySnapshot(QgsProject* project, const QString& recoveryDirectory, QString* errorOut,
                              const QStringList& onlyLayerIds) {
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
        if (!onlyLayerIds.isEmpty() && !onlyLayerIds.contains(source->id())) continue;
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
    KaSessionLog::line(QStringLiteral("[except] core/SurveyStorage.cpp:494"));
    return fail(QStringLiteral("복구 사본을 만드는 중 오류가 발생했습니다. 현재 창을 닫지 말고 다시 저장하세요."));
  }
}

AbsorbResult absorbExternalVectors(QgsProject* project, const QString& gpkgPath,
                                   const QString& alsoSurveyGpkg) {
  AbsorbResult r;
  if (!project || gpkgPath.isEmpty() || !QFileInfo::exists(gpkgPath)) return r;

  QSet<QString> used = existingGpkgLayerNames(gpkgPath);
  const QList<QgsMapLayer*> layers = project->mapLayers().values();
  for (QgsMapLayer* ml : layers) {
    if (!ml) continue;
    if (auto* early = qobject_cast<QgsVectorLayer*>(ml)) {
      const QString provider = early->providerType().toLower();
      if (provider == QLatin1String("ogr") && !livesInGpkg(early, gpkgPath) &&
          (alsoSurveyGpkg.isEmpty() || !livesInGpkg(early, alsoSurveyGpkg)) &&
          !LayerOps::isReferenceLayer(early)) {
        const QString file = early->source().section(QLatin1Char('|'), 0, 0);
        if (file.isEmpty() || !QFileInfo::exists(file)) {
          r.failed << early->name();
          continue;
        }
      }
    }
    if (!ml->isValid()) continue;
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
    if (provider == QLatin1String("ogr") && !alsoSurveyGpkg.isEmpty() &&
        livesInGpkg(vl, alsoSurveyGpkg))
      continue;
    if (LayerOps::isReferenceLayer(vl) && provider != QLatin1String("memory")) {
      // 바깥 파일의 참조 지도는 조사 파일에 복사하지 않는다.
      // 메모리 레이어는 닫으면 사라진다. persistSurveyWork는 커밋을 먼저 하므로
      // isModified()만 보면 닫기 저장에서 빠진다.
      r.skippedReference << vl->name();
      continue;
    }

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

PersistAttempt persistWorkspace(QgsProject* project, const QString& gpkgPath,
                                const QString& recoveryDirectory) {
  PersistAttempt attempt;
  if (!project || gpkgPath.isEmpty()) {
    attempt.error = QStringLiteral("저장 경로가 없습니다.");
    return attempt;
  }

  QHash<QString, QString> sources;
  for (QgsMapLayer* layer : project->mapLayers()) {
    if (auto* vector = qobject_cast<QgsVectorLayer*>(layer))
      sources.insert(vector->id(), vector->source());
  }

  QTemporaryDir generationDir(QFileInfo(gpkgPath).dir().filePath(QStringLiteral(".ka-survey-gen-XXXXXX")));
  const QString generation = generationDir.isValid()
      ? generationDir.filePath(QStringLiteral("survey.gpkg"))
      : QString();

  const auto recover = [&](const QString& message) {
    attempt.saved = false;
    if (attempt.error.isEmpty()) attempt.error = message;
    QStringList recoverable;
    for (QgsMapLayer* layer : project->mapLayers()) {
      auto* vector = qobject_cast<QgsVectorLayer*>(layer);
      if (vector && vector->isValid()) recoverable << vector->id();
    }
    QString recoveryError;
    attempt.recoveryPath = writeRecoverySnapshot(project, recoveryDirectory, &recoveryError,
                                                 recoverable);
    if (attempt.recoveryPath.isEmpty() && !recoveryError.isEmpty())
      attempt.error += QLatin1Char('\n') + recoveryError;
    restoreLayerSources(project, sources);
    project->setDirty(true);
    if (!generation.isEmpty()) {
      for (QgsMapLayer* layer : project->mapLayers()) {
        auto* vector = qobject_cast<QgsVectorLayer*>(layer);
        if (vector && livesInGpkg(vector, generation)) {
          generationDir.setAutoRemove(false);
          break;
        }
      }
    }
  };

  QList<QgsVectorLayer*> ordered;
  QSet<QString> seen;
  const auto enqueue = [&](QgsVectorLayer* vector) {
    if (!vector || seen.contains(vector->id())) return;
    seen.insert(vector->id());
    ordered << vector;
  };
  const auto walk = [&](auto&& self, QgsLayerTreeGroup* group) -> void {
    if (!group) return;
    for (QgsLayerTreeNode* node : group->children()) {
      if (auto* child = qobject_cast<QgsLayerTreeGroup*>(node))
        self(self, child);
      else if (auto* layerNode = qobject_cast<QgsLayerTreeLayer*>(node))
        enqueue(qobject_cast<QgsVectorLayer*>(layerNode->layer()));
    }
  };
  walk(walk, project->layerTreeRoot());
  for (QgsMapLayer* layer : project->mapLayers())
    enqueue(qobject_cast<QgsVectorLayer*>(layer));

  if (!generationDir.isValid() || generation.isEmpty()) {
    recover(QStringLiteral("다음 세대 조사 파일을 만들 공간이 없습니다."));
    return attempt;
  }
  QString copyError;
  if (!copySurvey(gpkgPath, generation, &copyError)) {
    recover(copyError.isEmpty() ? QStringLiteral("다음 세대 조사 파일을 만들지 못했습니다.")
                                : copyError);
    return attempt;
  }

  for (QgsVectorLayer* vector : ordered) {
    if (!vector->isValid() || !vector->isEditable() || !vector->isModified())
      continue;
    if (!vector->allowCommit()) {
      attempt.failedLayers << vector->name();
      recover(QStringLiteral("%1의 편집을 저장하지 못해 전체 저장을 마치지 못했습니다. "
                             "미저장 편집은 유지됩니다.")
                  .arg(vector->name()));
      return attempt;
    }
    if (livesInGpkg(vector, gpkgPath)) {
      QString writeLayerError;
      if (!writeLayerToGpkg(vector, generation, gpkgTableName(vector), project->transformContext(),
                            &writeLayerError)) {
        attempt.failedLayers << vector->name();
        recover(writeLayerError.isEmpty()
                    ? QStringLiteral("%1의 편집을 다음 세대 파일에 쓰지 못했습니다.").arg(vector->name())
                    : writeLayerError);
        return attempt;
      }
      attempt.committedLayers << vector->name();
      continue;
    }
    if (!vector->commitChanges(false)) {
      attempt.failedLayers << vector->name();
      recover(QStringLiteral("%1의 편집을 저장하지 못해 전체 저장을 마치지 못했습니다. "
                             "미저장 편집은 유지됩니다.")
                  .arg(vector->name()));
      return attempt;
    }
    attempt.committedLayers << vector->name();
  }

  const AbsorbResult absorbed = absorbExternalVectors(project, generation, gpkgPath);
  attempt.skippedRaster = absorbed.skippedRaster;
  attempt.skippedReference = absorbed.skippedReference;
  if (!absorbed.failed.isEmpty()) {
    attempt.failedLayers << absorbed.failed;
    recover(QStringLiteral("%1을 조사 파일에 보관하지 못했습니다.")
                .arg(absorbed.failed.join(QStringLiteral(", "))));
    return attempt;
  }

  QString writeError;
  try {
    if (!writeEmbedded(project, generation, &writeError)) {
      recover(writeError.isEmpty() ? QStringLiteral("조사 파일에 작업 구성을 저장하지 못했습니다.")
                                   : writeError);
      return attempt;
    }
  } catch (...) {
    KaSessionLog::line(QStringLiteral("[except] core/SurveyStorage.cpp:709"));
    recover(QStringLiteral("저장 중 오류가 발생했습니다. 원본 조사 파일은 그대로입니다. "
                           "창을 닫지 말고 다시 저장하세요."));
    return attempt;
  }

  for (QgsVectorLayer* vector : ordered) {
    if (vector && livesInGpkg(vector, gpkgPath) && vector->isModified())
      vector->rollBack(false);
  }
  retargetGpkgLayers(project, gpkgPath, generation);
  QgsOgrProviderUtils::invalidateCachedDatasets(QFileInfo(gpkgPath).absoluteFilePath());
  dropIdleJournals(gpkgPath);
  QString publishError;
  if (!publishSurveyGeneration(generation, gpkgPath, &publishError)) {
    recover(publishError.isEmpty() ? QStringLiteral("검증된 다음 세대로 원본을 교체하지 못했습니다.")
                                   : publishError);
    return attempt;
  }

  const QString originalAbs = QFileInfo(gpkgPath).absoluteFilePath();
  project->setFileName(originalAbs);
  project->setPresetHomePath(QFileInfo(originalAbs).absolutePath());
  for (QgsVectorLayer* vector : ordered) {
    if (vector && livesInGpkg(vector, gpkgPath) && vector->isModified())
      vector->rollBack(false);
  }
  retargetGpkgLayers(project, generation, gpkgPath);
  LayerOps::reloadSurveyGpkgReaders(project, gpkgPath);
  attempt.saved = true;
  return attempt;
}

QStringList embeddedReferenceVectorNames(QgsProject* project, const QString& gpkgPath) {
  QStringList names;
  for (QgsVectorLayer* layer : embeddedReferenceLayers(project, gpkgPath))
    names << layer->name();
  return names;
}

ExtractAttempt extractEmbeddedReferenceVectors(QgsProject* project, const QString& gpkgPath,
                                               const QString& outputDirectory) {
  ExtractAttempt attempt;
  attempt.outputDirectory = outputDirectory;
  attempt.bytesBefore = QFileInfo(gpkgPath).size();
  if (!project || gpkgPath.isEmpty() || !QFileInfo::exists(gpkgPath)) {
    attempt.error = QStringLiteral("조사 파일이 없습니다.");
    return attempt;
  }
  if (outputDirectory.isEmpty() || !QDir().mkpath(outputDirectory)) {
    attempt.error = QStringLiteral("참조 벡터를 둘 폴더를 만들지 못했습니다.");
    return attempt;
  }
  const QList<QgsVectorLayer*> targets = embeddedReferenceLayers(project, gpkgPath);
  if (targets.isEmpty()) {
    attempt.error = QStringLiteral("조사 파일 안에 있는 참조 벡터가 없습니다.");
    return attempt;
  }

  QStringList tables;
  for (QgsVectorLayer* vl : targets) {
    const QString table = gpkgTableName(vl);
    QString dest = QDir(outputDirectory).filePath(sanitizeLayerName(vl->name()) + QStringLiteral(".gpkg"));
    int suffix = 2;
    while (QFileInfo::exists(dest)) {
      dest = QDir(outputDirectory).filePath(sanitizeLayerName(vl->name()) +
                                            QStringLiteral("_%1.gpkg").arg(suffix++));
    }
    QgsVectorFileWriter::SaveVectorOptions opt;
    opt.driverName = QStringLiteral("GPKG");
    opt.layerName = table;
    opt.fileEncoding = QStringLiteral("UTF-8");
    QString writeError;
    if (QgsVectorFileWriter::writeAsVectorFormatV3(vl, dest, project->transformContext(), opt,
                                                   &writeError) != QgsVectorFileWriter::NoError) {
      attempt.failed << vl->name();
      attempt.error = writeError;
      return attempt;
    }
    const QString stored = QStringLiteral("%1|layername=%2").arg(dest, table);
    QgsVectorLayer probe(stored, vl->name(), QStringLiteral("ogr"));
    if (!probe.isValid() || probe.featureCount() != vl->featureCount()) {
      attempt.failed << vl->name();
      attempt.error = QStringLiteral("%1의 바깥 사본을 확인하지 못했습니다.").arg(vl->name());
      return attempt;
    }
    const QString name = vl->name();
    vl->setDataSource(stored, name, QStringLiteral("ogr"));
    LayerOps::markReferenceLayer(vl);
    if (!vl->isValid()) {
      attempt.failed << name;
      attempt.error = QStringLiteral("%1을 바깥 파일로 돌리지 못했습니다.").arg(name);
      return attempt;
    }
    tables << table;
    attempt.moved << name;
  }

  QTemporaryDir generationDir(QFileInfo(gpkgPath).dir().filePath(QStringLiteral(".ka-survey-gen-XXXXXX")));
  if (!generationDir.isValid()) {
    attempt.error = QStringLiteral("다음 세대 조사 파일을 만들 공간이 없습니다.");
    return attempt;
  }
  const QString generation = generationDir.filePath(QStringLiteral("survey.gpkg"));
  QString copyError;
  if (!copySurvey(gpkgPath, generation, &copyError) ||
      !dropGpkgTables(generation, tables, &copyError)) {
    attempt.error = copyError.isEmpty() ? QStringLiteral("참조 테이블을 조사 파일에서 빼지 못했습니다.")
                                        : copyError;
    return attempt;
  }
  QString writeError;
  try {
    if (!writeEmbedded(project, generation, &writeError)) {
      attempt.error = writeError.isEmpty() ? QStringLiteral("작업 구성을 저장하지 못했습니다.")
                                           : writeError;
      return attempt;
    }
  } catch (...) {
    KaSessionLog::line(QStringLiteral("[except] core/SurveyStorage.cpp:826"));
    attempt.error = QStringLiteral("작업 구성을 저장하는 중 오류가 났습니다. 조사 파일은 그대로 둡니다.");
    return attempt;
  }
  for (QgsMapLayer* ml : project->mapLayers()) {
    auto* vl = qobject_cast<QgsVectorLayer*>(ml);
    if (vl && livesInGpkg(vl, gpkgPath) && vl->isModified()) vl->rollBack(false);
  }
  retargetGpkgLayers(project, gpkgPath, generation);
  QgsOgrProviderUtils::invalidateCachedDatasets(QFileInfo(gpkgPath).absoluteFilePath());
  dropIdleJournals(gpkgPath);
  QString publishError;
  if (!publishSurveyGeneration(generation, gpkgPath, &publishError)) {
    attempt.error = publishError.isEmpty() ? QStringLiteral("줄어든 조사 파일로 교체하지 못했습니다.")
                                           : publishError;
    return attempt;
  }
  const QString originalAbs = QFileInfo(gpkgPath).absoluteFilePath();
  project->setFileName(originalAbs);
  project->setPresetHomePath(QFileInfo(originalAbs).absolutePath());
  retargetGpkgLayers(project, generation, gpkgPath);
  LayerOps::reloadSurveyGpkgReaders(project, gpkgPath);
  attempt.bytesAfter = QFileInfo(gpkgPath).size();
  attempt.extracted = attempt.failed.isEmpty() && !attempt.moved.isEmpty();
  return attempt;
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
  QString home = QFileInfo(abs).absolutePath();
  // 다음 세대는 surveyDir/.ka-survey-gen-*/survey.gpkg 다. 여기서 상대 경로를
  // 계산하면 ../ 가 하나 더 붙어, 다시 열 때 C:/Users/AppData/... 로 풀린다.
  // https://docs.qgis.org/3.44/en/docs/user_manual/introduction/qgis_configuration.html
  if (QDir(home).dirName().startsWith(QLatin1String(".ka-survey-gen-")))
    home = QFileInfo(home).absolutePath();
  project->setPresetHomePath(home);
  const Qgis::FilePathType previousPathType = project->filePathStorage();
  project->setFilePathStorage(Qgis::FilePathType::Absolute);
  const QString uri = projectUri(gpkgPath);
  if (!project->write(uri)) {
    project->setFilePathStorage(previousPathType);
    if (errorOut) *errorOut = project->error();
    return false;
  }
  LayerOps::saveGpkgDefaultStyles(project, abs);
  // 프로젝트·스타일 쓰기가 같은 GPKG의 OGR 연결을 끊는다. 피처 수는 캐시에 남아
  // 있어도 이터레이터는 unable to open database file 로 실패하고 도형이 안 그려진다.
  LayerOps::reloadSurveyGpkgReaders(project, abs);
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
