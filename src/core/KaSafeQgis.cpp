#include "KaSafeQgis.h"
#include "KaPortableRuntime.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScopeGuard>
#include <QSet>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <exception>

#include <qgis.h>
#include <qgsproject.h>
#include <qgsziputils.h>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

static bool readProjectPlain(QgsProject* project, const QString& path, bool loadLayouts) {
  // A user-composed sheet cannot be rebuilt from the map. Explicit GUI opens
  // restore it; validation/recovery can still omit non-thread-safe layouts.
  return project->read(path, loadLayouts ? Qgis::ProjectReadFlags()
                                        : Qgis::ProjectReadFlags(Qgis::ProjectReadFlag::DontLoadLayouts));
}

static void clearProjectPlain(QgsProject* project) { project->clear(); }

bool kaSafeReadQgisProject(QgsProject* project, const QString& path, bool* crashedOut, bool loadLayouts) {
  if (crashedOut) *crashedOut = false;
  if (!project || path.isEmpty())
    return false;
#ifdef Q_OS_WIN
  bool ok = false;
  __try {
    ok = readProjectPlain(project, path, loadLayouts);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    if (crashedOut) *crashedOut = true;
    return false;
  }
  return ok;
#else
  return readProjectPlain(project, path, loadLayouts);
#endif
}

// Keeps the real suffix last so QGIS still recognises the format:
// 안동시.qgz -> 안동시<tag>.qgz
static QString siblingWithTag(const QFileInfo& fi, const QString& tag) {
  const QString suffix = fi.suffix();
  const QString stem = fi.completeBaseName();
  if (suffix.isEmpty())
    return fi.absolutePath() + QLatin1Char('/') + stem + tag;
  return fi.absolutePath() + QLatin1Char('/') + stem + tag + QLatin1Char('.') + suffix;
}

QString kaProjectBackupPath(const QString& path) {
  if (path.isEmpty()) return {};
  return siblingWithTag(QFileInfo(path), QStringLiteral(".bak"));
}

bool kaWriteQgisProjectAtomic(QgsProject* project, const QString& path, QString* errorOut) {
  if (errorOut) errorOut->clear();
  auto fail = [&](const QString& m) {
    if (errorOut) *errorOut = m;
    return false;
  };
  if (!project || path.isEmpty()) return fail(QStringLiteral("저장 경로가 없습니다."));
  const QString originalFileName = project->fileName();
  const bool originalDirty = project->isDirty();
  bool completed = false;
  const auto restoreProjectOnFailure = qScopeGuard([&] {
    if (!completed) {
      project->setFileName(originalFileName);
      project->setDirty(originalDirty);
    }
  });
  try {
    const QFileInfo fi(path);
    if (!QDir().mkpath(fi.absolutePath()))
      return fail(QStringLiteral("작업 화면의 저장 폴더를 만들 수 없습니다. 쓰기 가능한 다른 폴더를 선택해 주세요."));
    const QString finalPath = fi.absoluteFilePath();
    // Same folder, suffix preserved. Same folder because QgsPathResolver writes relative
    // datasources against the project file's directory and rename must stay on one volume;
    // suffix preserved because QgsZipUtils::isZipFile keys on ".qgz" — a temp named
    // "x.qgz.writing" is written as plain XML and renaming it to .qgz yields a file that
    // QgsProject::read can no longer open.
    QTemporaryFile temporary(siblingWithTag(fi, QStringLiteral(".ka-writing-XXXXXX")));
    if (!temporary.open())
      return fail(
          QStringLiteral("작업 화면의 임시 파일을 만들 수 없습니다. 남은 공간과 폴더의 쓰기 권한을 확인해 주세요."));
    const QString tmpPath = temporary.fileName();
    temporary.close();
    const auto removeTemporaryFiles = qScopeGuard([&] {
      QFile::remove(tmpPath);
      QFile::remove(tmpPath + QLatin1Char('~'));
    });
    // Reserve a unique sibling name, but leave no empty archive for QGIS/libzip
    // to interpret as an existing corrupt QGZ. The scope guard owns the file
    // subsequently created by project->write(), including its failure paths.
    if (!temporary.remove())
      return fail(QStringLiteral("작업 화면의 임시 파일을 준비하지 못했습니다. 저장 폴더의 쓰기 권한을 확인한 뒤 다시 저장해 주세요."));
    temporary.setAutoRemove(false);
    const QString bakPath = kaProjectBackupPath(finalPath);

    project->setFileName(tmpPath);
    const bool wrote = project->write();
    if (!wrote) {
      return fail(
          QStringLiteral(
              "작업 화면을 저장하지 못했습니다. 기존 파일은 그대로입니다. 남은 공간과 쓰기 권한을 확인해 주세요.\n%1")
              .arg(project->error()));
    }
    // A zero-byte or missing temp file means the write claimed success but produced
    // nothing. Renaming that over a good project is how the file got destroyed.
    const QFileInfo tmpFi(tmpPath);
    if (!tmpFi.exists() || tmpFi.size() <= 0) {
      return fail(QStringLiteral("저장 결과가 비어 있어 기존 파일을 그대로 두었습니다."));
    }
    // Prove it opens before it replaces a good file. Layers are not resolved, so this
    // only checks that the container and XML are intact — which is what silently broke.
    {
      QgsProject probe;
      if (!probe.read(tmpPath, Qgis::ProjectReadFlag::DontResolveLayers | Qgis::ProjectReadFlag::DontLoadLayouts)) {
        return fail(QStringLiteral("저장한 파일을 다시 열 수 없어 기존 파일을 그대로 두었습니다."));
      }
    }

    const auto atomicCopy = [](const QString& source, const QString& target, QString* detail) {
      QFile input(source);
      QSaveFile output(target);
      output.setDirectWriteFallback(false);
      if (!input.open(QIODevice::ReadOnly)) {
        *detail = input.errorString();
        return false;
      }
      if (!output.open(QIODevice::WriteOnly)) {
        *detail = output.errorString();
        return false;
      }
      while (!input.atEnd()) {
        const QByteArray bytes = input.read(1024 * 1024);
        if (input.error() != QFileDevice::NoError) {
          *detail = input.errorString();
          return false;
        }
        if (output.write(bytes) != bytes.size()) {
          *detail = output.errorString();
          return false;
        }
      }
      if (!output.commit()) {
        *detail = output.errorString();
        return false;
      }
      return true;
    };
    QString detail;
    if (QFile::exists(finalPath)) {
      // Preserve a complete previous generation without moving the live file away.
      if (!atomicCopy(finalPath, bakPath, &detail)) {
        return fail(QStringLiteral("이전 작업 화면의 백업을 만들지 못해 저장을 멈췄습니다. 기존 파일은 그대로입니다. "
                                   "저장 폴더의 공간과 권한을 확인해 주세요.\n%1")
                        .arg(detail));
      }
    }
    project->setFileName(finalPath);
    if (!atomicCopy(tmpPath, finalPath, &detail)) {
      return fail(QStringLiteral("작업 화면 파일을 교체하지 못했습니다. 기존 파일은 그대로입니다. 파일을 사용하는 다른 "
                                 "프로그램을 닫고 다시 저장해 주세요.\n%1")
                      .arg(detail));
    }
    completed = true;
    return true;
  } catch (const std::exception& ex) {
    return fail(
        QStringLiteral(
            "작업 화면 저장 중 오류가 발생했습니다. 작업을 유지한 채 다시 저장하거나 다른 이름으로 저장해 주세요.\n%1")
            .arg(QString::fromUtf8(ex.what())));
  } catch (...) {
    return fail(QStringLiteral(
        "작업 화면 저장 중 오류가 발생했습니다. 작업을 유지한 채 다시 저장하거나 다른 이름으로 저장해 주세요."));
  }
}

bool kaSafeClearQgisProject(QgsProject* project) {
  if (!project)
    return false;
#ifdef Q_OS_WIN
  __try {
    clearProjectPlain(project);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
  return true;
#else
  clearProjectPlain(project);
  return true;
#endif
}

static bool nameLooksSatellite(const QString& name) {
  return name.contains(QStringLiteral("위성")) ||
         name.contains(QStringLiteral("Satellite"), Qt::CaseInsensitive);
}

static int countSatelliteNamesInXml(const QString& xml) {
  int fromLayerName = 0;
  int fromTree = 0;
  const QRegularExpression layerRe(QStringLiteral("<layername>([^<]*)</layername>"),
                                   QRegularExpression::CaseInsensitiveOption);
  auto it = layerRe.globalMatch(xml);
  while (it.hasNext()) {
    if (nameLooksSatellite(it.next().captured(1).trimmed()))
      ++fromLayerName;
  }
  const QRegularExpression treeRe(
      QStringLiteral("layer-tree-layer[^>]*\\bname=\"([^\"]+)\""),
      QRegularExpression::CaseInsensitiveOption);
  auto treeIt = treeRe.globalMatch(xml);
  while (treeIt.hasNext()) {
    if (nameLooksSatellite(treeIt.next().captured(1).trimmed()))
      ++fromTree;
  }
  return qMax(fromLayerName, fromTree);
}

static QString readQgsXml(const QString& qgsPath) {
  QFile f(qgsPath);
  if (!f.open(QIODevice::ReadOnly))
    return {};
  return QString::fromUtf8(f.readAll());
}

int kaCountSatelliteLayersInQgisProjectFile(const QString& path) {
  if (path.isEmpty() || !QFileInfo::exists(path))
    return 0;
  const QString suffix = QFileInfo(path).suffix().toLower();
  if (suffix == QLatin1String("qgs")) {
    const QString xml = readQgsXml(path);
    if (xml.isEmpty())
      return -1;
    return countSatelliteNamesInXml(xml);
  }
  if (suffix != QLatin1String("qgz") && suffix != QLatin1String("zip"))
    return 0;

  QTemporaryDir dir;
  if (!dir.isValid())
    return -1;
  QStringList files;
  if (!QgsZipUtils::unzip(path, dir.path(), files, false))
    return -1;
  QString xml;
  for (const QString& f : files) {
    if (f.endsWith(QLatin1String(".qgs"), Qt::CaseInsensitive)) {
      xml = readQgsXml(f);
      if (!xml.isEmpty())
        break;
    }
  }
  if (xml.isEmpty()) {
    const QStringList found =
        QDir(dir.path()).entryList(QStringList{QStringLiteral("*.qgs")}, QDir::Files);
    if (!found.isEmpty())
      xml = readQgsXml(dir.filePath(found.first()));
  }
  if (xml.isEmpty())
    return -1;
  return countSatelliteNamesInXml(xml);
}

bool kaQgisProjectFileHasDuplicateSatellites(const QString& path) {
  if (path.isEmpty() || !QFileInfo::exists(path))
    return false;
  const int n = kaCountSatelliteLayersInQgisProjectFile(path);
  return n >= 2;
}

static QSet<QString> g_unsafeQgisProjects;
static bool g_unsafeLoaded = false;

static QString unsafeListPath() {
  if (KaPortableRuntime::discover(KaPortableRuntime::resolvedExeDir()).looksBundled())
    return QDir(KaPortableRuntime::userConfigDir()).filePath(QStringLiteral("unsafe-projects.txt"));
  const QByteArray localAppData = qgetenv("LOCALAPPDATA");
  const QString base = localAppData.isEmpty() ? QDir::tempPath()
                                              : QString::fromLocal8Bit(localAppData);
  return base + QStringLiteral("/ka-hgis/unsafe-projects.txt");
}

// The mark has to outlive the process. A crashed read leaves QGIS unusable, so the
// app restarts — and an in-memory set would let the next launch walk into the same AV.
static void loadUnsafeList() {
  if (g_unsafeLoaded) return;
  g_unsafeLoaded = true;
  QFile f(unsafeListPath());
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
  const QStringList lines = QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'));
  for (const QString& line : lines) {
    const QString p = line.trimmed();
    if (!p.isEmpty()) g_unsafeQgisProjects.insert(p);
  }
}

static void saveUnsafeList() {
  const QString path = unsafeListPath();
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile f(path);
  if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return;
  QStringList out(g_unsafeQgisProjects.begin(), g_unsafeQgisProjects.end());
  out.sort();
  f.write(out.join(QLatin1Char('\n')).toUtf8());
}

void kaMarkQgisProjectUnsafeToRead(const QString& path) {
  if (path.isEmpty())
    return;
  loadUnsafeList();
  g_unsafeQgisProjects.insert(QFileInfo(path).absoluteFilePath());
  saveUnsafeList();
}

void kaClearQgisProjectUnsafeMark(const QString& path) {
  if (path.isEmpty())
    return;
  loadUnsafeList();
  if (g_unsafeQgisProjects.remove(QFileInfo(path).absoluteFilePath()))
    saveUnsafeList();
}

bool kaQgisProjectFileIsUnsafeToRead(const QString& path) {
  if (path.isEmpty() || !QFileInfo::exists(path))
    return false;
  // Duplicate satellites are pruned after a successful read. Skipping the
  // whole .qgz here dropped every imported SHP (안동시 15장).
  loadUnsafeList();
  return g_unsafeQgisProjects.contains(QFileInfo(path).absoluteFilePath());
}
