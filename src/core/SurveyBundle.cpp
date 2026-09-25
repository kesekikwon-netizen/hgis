#include "SurveyBundle.h"

#include "KaSessionLog.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QStandardPaths>

#include <qgsdataprovider.h>
#include <qgsmaplayer.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

namespace {

const QString kScope = QStringLiteral("ka_hgis");
const QString kSurveyDirKey = QStringLiteral("/survey_dir");

QString normalized(const QString& path) {
  if (path.isEmpty()) return {};
  return QDir::cleanPath(QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath()));
}

bool isUnder(const QString& path, const QString& root) {
  if (path.isEmpty() || root.isEmpty()) return false;
  return path.compare(root, Qt::CaseInsensitive) == 0 ||
         path.startsWith(root + QLatin1Char('/'), Qt::CaseInsensitive);
}

QStringList appRoots() {
  QStringList roots;
  for (const auto location : {QStandardPaths::AppLocalDataLocation, QStandardPaths::AppDataLocation,
                              QStandardPaths::CacheLocation, QStandardPaths::AppConfigLocation,
                              QStandardPaths::TempLocation})
    roots << QStandardPaths::writableLocation(location);
  roots << QDir::tempPath();
  // 포터블은 앱 폴더가 새 버전마다 바뀐다. 그 안에 생긴 자료는 새 버전이 못 본다.
  if (QCoreApplication::instance()) roots << QCoreApplication::applicationDirPath();
  // 설치마다 AppData/Local/ka-hgis 와 AppData/Local/ka-hgis/ka-hgis 가 섞여 있었다.
  roots << QDir::home().filePath(QStringLiteral("AppData/Local/ka-hgis"));
  QStringList out;
  for (const QString& root : roots) {
    const QString clean = normalized(root);
    if (!clean.isEmpty() && !out.contains(clean, Qt::CaseInsensitive)) out << clean;
  }
  return out;
}

// 레이어 원본을 앞(/vsizip/, GPKG:), 파일, 뒤(|layername=, :table, zip 안 경로)로 나눈다.
struct SourceParts {
  QString prefix;
  QString file;
  QString suffix;
};

SourceParts splitSource(const QString& source) {
  SourceParts parts;
  const int pipe = source.indexOf(QLatin1Char('|'));
  QString head = pipe < 0 ? source : source.left(pipe);
  parts.suffix = pipe < 0 ? QString() : source.mid(pipe);
  head = QDir::fromNativeSeparators(head);
  const auto cutAt = [&](const QString& prefix, const QString& extension) {
    if (!head.startsWith(prefix, Qt::CaseInsensitive)) return false;
    const int end = head.indexOf(extension, prefix.size(), Qt::CaseInsensitive);
    if (end < 0) return false;
    parts.prefix = head.left(prefix.size());
    parts.file = head.mid(prefix.size(), end + extension.size() - prefix.size());
    parts.suffix = head.mid(end + extension.size()) + parts.suffix;
    return true;
  };
  if (cutAt(QStringLiteral("/vsizip/"), QStringLiteral(".zip"))) return parts;
  if (cutAt(QStringLiteral("GPKG:"), QStringLiteral(".gpkg"))) return parts;
  if (head.startsWith(QStringLiteral("/vsi"), Qt::CaseInsensitive)) return {};  // 다른 가상 경로는 건드리지 않는다
  parts.file = head;
  return parts;
}

QString joinSource(const SourceParts& parts, const QString& file) {
  return parts.prefix + file + parts.suffix;
}

QString safeFolderName(QString name) {
  static const QString bad = QStringLiteral("<>:\"/\\|?*");
  for (QChar& c : name)
    if (bad.contains(c) || c.unicode() < 32) c = QLatin1Char('_');
  name = name.trimmed();
  while (name.endsWith(QLatin1Char('.'))) name.chop(1);
  if (name.size() > 60) name = name.left(60).trimmed();
  return name.isEmpty() ? QStringLiteral("자료") : name;
}

// file 과 같은 이름으로 시작하는 곁 파일(.shx .dbf .prj .cpg .aux.xml -wal ...).
QFileInfoList companions(const QFileInfo& file) {
  QFileInfoList out;
  const QDir dir = file.dir();
  const QString base = file.completeBaseName();
  const QString name = file.fileName();
  for (const QFileInfo& entry : dir.entryInfoList(QDir::Files | QDir::Hidden)) {
    const QString other = entry.fileName();
    if (other.compare(name, Qt::CaseInsensitive) == 0) continue;
    if (other.endsWith(QLatin1String("-shm"), Qt::CaseInsensitive)) continue;
    if (other.startsWith(name, Qt::CaseInsensitive) ||
        other.startsWith(base + QLatin1Char('.'), Qt::CaseInsensitive))
      out << entry;
  }
  return out;
}

bool sameFile(const QString& a, const QString& b) {
  const QFileInfo x(a);
  const QFileInfo y(b);
  return x.isFile() && y.isFile() && x.size() == y.size();
}

// file 과 곁 파일을 targetDir 에 복사한다. 이미 같은 파일이 있으면 그대로 쓰고,
// 이름만 같은 다른 파일이 있으면 번호 붙인 폴더를 쓴다. 복사한 주 파일 경로를 돌려준다.
QString copyWithCompanions(const QString& file, const QString& targetDir, QString* error) {
  const QFileInfo source(file);
  QString dir = targetDir;
  for (int n = 2; n < 100; ++n) {
    const QString candidate = QDir(dir).filePath(source.fileName());
    if (!QFileInfo::exists(candidate) || sameFile(file, candidate)) break;
    dir = targetDir + QStringLiteral("_%1").arg(n);
  }
  if (!QDir().mkpath(dir)) {
    if (error) *error = QStringLiteral("폴더를 만들지 못했습니다: %1").arg(QDir::toNativeSeparators(dir));
    return {};
  }
  QFileInfoList all = companions(source);
  all.prepend(source);
  for (const QFileInfo& part : all) {
    const QString target = QDir(dir).filePath(part.fileName());
    if (sameFile(part.absoluteFilePath(), target)) continue;
    if (QFileInfo::exists(target)) QFile::remove(target);
    if (!QFile::copy(part.absoluteFilePath(), target)) {
      if (error) *error = QStringLiteral("복사하지 못했습니다: %1").arg(QDir::toNativeSeparators(part.absoluteFilePath()));
      return {};
    }
  }
  return normalized(QDir(dir).filePath(source.fileName()));
}

bool fileBacked(const QgsMapLayer* layer) {
  const QString provider = layer->providerType().toLower();
  return provider == QLatin1String("ogr") || provider == QLatin1String("gdal");
}

}  // namespace

namespace SurveyBundle {

QString collectedFolderName() { return QStringLiteral("가져온자료"); }

bool isAppManagedPath(const QString& path) {
  const QString clean = normalized(path);
  if (clean.isEmpty()) return false;
  for (const QString& part : clean.split(QLatin1Char('/')))
    if (part.startsWith(QLatin1String(".ka-survey-gen-"))) return true;
  for (const QString& root : appRoots())
    if (isUnder(clean, root)) return true;
  return false;
}

bool isRegeneratedCache(const QString& path) {
  // DEM 음영은 원본 DEM 경로를 적은 .vrt 캐시다. 옮겨도 쓸모없고 없으면 다시 만든다.
  return normalized(path).contains(QLatin1String("/dem-relief/"), Qt::CaseInsensitive);
}

CollectResult collectIntoSurvey(QgsProject* project, const QString& surveyDir,
                                const QString& previousSurveyDir) {
  CollectResult result;
  if (!project || surveyDir.isEmpty() || !QFileInfo(surveyDir).isDir()) return result;
  const QString survey = normalized(surveyDir);
  const QString previous = normalized(previousSurveyDir);
  const bool moving = !previous.isEmpty() && previous.compare(survey, Qt::CaseInsensitive) != 0;
  const QString collected = QDir(survey).filePath(collectedFolderName());
  QHash<QString, QString> copied;  // 원래 파일(소문자) → 조사 폴더 사본
  for (QgsMapLayer* layer : project->mapLayers()) {
    if (!layer || !fileBacked(layer)) continue;
    const SourceParts parts = splitSource(layer->source());
    if (parts.file.isEmpty()) continue;
    const QString file = normalized(parts.file);
    if (!QFileInfo(file).isFile()) continue;  // 못 찾는 파일은 열 때 알린다
    if (isUnder(file, survey)) continue;
    const bool fromPrevious = moving && isUnder(file, previous);
    if (!fromPrevious && (!isAppManagedPath(file) || isRegeneratedCache(file))) continue;
    // 편집 중인 레이어는 원본을 바꾸면 편집 버퍼를 잃는다. 다음 저장에서 모은다.
    if (auto* vector = qobject_cast<QgsVectorLayer*>(layer); vector && vector->isEditable()) continue;
    QString target = copied.value(file.toLower());
    if (target.isEmpty()) {
      // 이전 조사 폴더에 있던 파일은 같은 하위 폴더 구조로, 앱 자료는 「가져온자료/레이어 이름」에 둔다.
      const QString targetDir = fromPrevious
          ? QFileInfo(QDir(survey).filePath(QDir(previous).relativeFilePath(file))).absolutePath()
          : QDir(collected).filePath(safeFolderName(layer->name()));
      QString error;
      target = copyWithCompanions(file, targetDir, &error);
      if (target.isEmpty()) {
        KaSessionLog::line(QStringLiteral("[bundle] %1 — %2").arg(layer->name(), error));
        result.failed << layer->name();
        continue;
      }
      copied.insert(file.toLower(), target);
    }
    const QString original = layer->source();
    QgsDataProvider::ProviderOptions options;
    options.transformContext = project->transformContext();
    layer->setDataSource(joinSource(parts, target), layer->name(), layer->providerType(), options);
    if (!layer->isValid()) {
      layer->setDataSource(original, layer->name(), layer->providerType(), options);
      KaSessionLog::line(QStringLiteral("[bundle] %1 — 사본을 열지 못해 원래 경로를 유지").arg(layer->name()));
      result.failed << layer->name();
      continue;
    }
    result.copied << layer->name();
  }
  return result;
}

QString savedSurveyDir(const QgsProject* project) {
  if (!project) return {};
  return project->readEntry(kScope, kSurveyDirKey);
}

void rememberSurveyDir(QgsProject* project, const QString& surveyDir) {
  if (!project || surveyDir.isEmpty()) return;
  project->writeEntry(kScope, kSurveyDirKey, normalized(surveyDir));
}

QString rebaseIntoSurvey(const QString& path, const QString& savedDir, const QString& currentDir) {
  const QString saved = normalized(savedDir);
  const QString current = normalized(currentDir);
  if (saved.isEmpty() || current.isEmpty() || saved.compare(current, Qt::CaseInsensitive) == 0) return {};
  const SourceParts parts = splitSource(path);
  if (parts.file.isEmpty()) return {};
  const QString file = QDir::cleanPath(QDir::fromNativeSeparators(parts.file));
  if (!isUnder(file, saved)) return {};
  return joinSource(parts, current + file.mid(saved.size()));
}

QString onSurveyDrive(const QString& source, const QString& surveyDir) {
  const SourceParts parts = splitSource(source);
  const QString file = QDir::fromNativeSeparators(parts.file);
  const QString dir = normalized(surveyDir);
  const auto hasDrive = [](const QString& p) {
    return p.size() >= 3 && p.at(1) == QLatin1Char(':') && p.at(2) == QLatin1Char('/');
  };
  if (!hasDrive(file) || !hasDrive(dir) || file.at(0).toUpper() == dir.at(0).toUpper()) return {};
  return joinSource(parts, dir.left(2) + file.mid(2));
}

QString sourceFile(const QString& source) { return splitSource(source).file; }

QStringList missingFileLayers(QgsProject* project) {
  QStringList names;
  if (!project) return names;
  for (QgsMapLayer* layer : project->mapLayers()) {
    if (!layer || layer->isValid() || !fileBacked(layer)) continue;
    const SourceParts parts = splitSource(layer->source());
    if (parts.file.isEmpty() || QFileInfo::exists(parts.file) || isRegeneratedCache(parts.file)) continue;
    names << layer->name();
  }
  names.sort(Qt::CaseInsensitive);
  return names;
}

}  // namespace SurveyBundle
