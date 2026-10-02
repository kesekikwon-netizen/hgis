#include "KaSessionLog.h"
#include "SecretMask.h"
#include "app/KaHgisVersion.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QRecursiveMutex>
#include <QTextStream>

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

// The crash guard asks for the log folder before QApplication exists, so the
// executable folder comes from the module path, not QCoreApplication.
QString moduleExeDir() {
#ifdef Q_OS_WIN
  wchar_t buf[4096];
  const DWORD n = GetModuleFileNameW(nullptr, buf, 4096);
  if (n > 0 && n < 4096) return QFileInfo(QString::fromWCharArray(buf, int(n))).absolutePath();
#endif
  if (QCoreApplication::instance()) return QCoreApplication::applicationDirPath();
  return {};
}

QString localAppDataLogDir() {
  const QByteArray localAppData = qgetenv("LOCALAPPDATA");
  const QString base = localAppData.isEmpty() ? QDir::tempPath()
                                              : QString::fromLocal8Bit(localAppData);
  return base + QStringLiteral("/ka-hgis/logs");
}

// Resolved once per process: the executable does not move while running.
const QString& defaultLogDir() {
  static const QString resolved = [] {
    const QString portable = KaSessionLog::portableLogDirFor(moduleExeDir());
    return portable.isEmpty() ? localAppDataLogDir() : portable;
  }();
  return resolved;
}

int envInt(const char* name, int fallback, int lo, int hi) {
  const QString env = qEnvironmentVariable(name);
  if (env.isEmpty()) return fallback;
  bool ok = false;
  const int v = env.toInt(&ok);
  return ok ? qBound(lo, v, hi) : fallback;
}

}  // namespace

QString KaSessionLog::portableLogDirFor(const QString& exeDir) {
  if (exeDir.trimmed().isEmpty()) return {};
  // Same rule as KaPortableRuntime::discover(exeDir).looksBundled(): a portable bundle
  // carries its own QGIS under apps/qgis-dev. Checked here directly because discover()
  // may create an ASCII junction (a child process) and needs QGIS/GDAL libraries, while
  // the crash guard asks for this folder before QApplication exists and
  // ka_catch_log_tests links Qt Core only.
  const QDir exe(QDir(exeDir).absolutePath());
  if (!QFileInfo(exe.filePath(QStringLiteral("apps/qgis-dev"))).isDir()) return {};
  return exe.filePath(QStringLiteral("config/logs"));
}

QString KaSessionLog::dir() {
  const QString overridden = qEnvironmentVariable("KA_HGIS_LOG_DIR");
  if (!overridden.isEmpty()) return QDir::fromNativeSeparators(overridden);
  return defaultLogDir();
}

qint64 KaSessionLog::maxBytes() {
  const QString env = qEnvironmentVariable("KA_HGIS_LOG_MAX_BYTES");
  if (!env.isEmpty()) {
    bool ok = false;
    const qint64 v = env.toLongLong(&ok);
    if (ok && v > 0) return v;
  }
  return kDefaultMaxBytes;
}

int KaSessionLog::keepOldFiles() {
  return envInt("KA_HGIS_LOG_KEEP", kDefaultKeepOldFiles, 1, 20);
}

QString KaSessionLog::rotatedFileName(int index) {
  return index <= 1 ? QStringLiteral("session.old.log")
                    : QStringLiteral("session.old.%1.log").arg(index);
}

QString KaSessionLog::dumpHint() {
  return QStringLiteral(
             "세션 로그(session.log, %1MB 초과 시 session.old.log로 회전, 이전 기록 %2개 보관)와 "
             "크래시 로그(crash-날짜.log)·미니덤프(crash-날짜.dmp): %3")
      .arg(kDefaultMaxBytes / (1024 * 1024))
      .arg(keepOldFiles())
      .arg(QDir::toNativeSeparators(dir()));
}

namespace {

// Returns true when the file was rotated and reopened empty.
bool rotateIfNeeded(QFile& logFile, const QString& path) {
  if (!logFile.isOpen() || logFile.size() <= KaSessionLog::maxBytes()) return false;
  logFile.close();
  const QDir folder = QFileInfo(path).absoluteDir();
  const int keep = KaSessionLog::keepOldFiles();
  // QFile::rename does not overwrite an existing file.
  // https://doc.qt.io/qt-6/qfile.html
  QFile::remove(folder.filePath(KaSessionLog::rotatedFileName(keep)));
  for (int i = keep - 1; i >= 1; --i)
    QFile::rename(folder.filePath(KaSessionLog::rotatedFileName(i)),
                  folder.filePath(KaSessionLog::rotatedFileName(i + 1)));
  QFile::rename(path, folder.filePath(KaSessionLog::rotatedFileName(1)));
  logFile.setFileName(path);
  return logFile.open(QIODevice::Append | QIODevice::Text);
}

QString g_qgisVersion = QStringLiteral("없음");

QString nowStamp() {
  return QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
}

// First line of every file: build, QGIS and process id, so lines from two runs
// (or two copies of a portable folder) are never mistaken for one session.
void writeHeader(QTextStream& ts, const QString& note) {
  ts << nowStamp() << ' ' << KaSessionLog::buildLabel()
     << QStringLiteral(" · pid %1").arg(QCoreApplication::applicationPid());
  if (!note.isEmpty()) ts << QStringLiteral(" · ") << note;
  ts << '\n';
}

}  // namespace

QString KaSessionLog::buildLabel() {
  return QStringLiteral("ka-hgis %1 · 커밋 %2 · QGIS %3")
      .arg(QLatin1String(KA_HGIS_VERSION), QLatin1String(KA_HGIS_GIT_HASH), g_qgisVersion);
}

void KaSessionLog::setQgisVersion(const QString& version) {
  const QString trimmed = version.trimmed();
  if (!trimmed.isEmpty()) g_qgisVersion = trimmed;
}

void KaSessionLog::line(const QString& rawText) {
  thread_local bool inLogLine = false;
  if (inLogLine) return;
  struct InLogLineGuard {
    bool& flag;
    explicit InLogLineGuard(bool& f) : flag(f) { flag = true; }
    ~InLogLineGuard() { flag = false; }
  } inLogLineGuard(inLogLine);

  // Keys and account values are removed at the single entry point (F029):
  // QGIS provider warnings and caught errors may carry a request URL.
  const QString text = SecretMask::mask(rawText);

  static QRecursiveMutex mtx;
  static QString lastLine;
  static int repeat = 0;
  QMutexLocker lock(&mtx);

  bool foldedNote = false;
  if (text == lastLine) {
    ++repeat;
    if (repeat % 50 != 0) return;
    foldedNote = true;
  }

  static QFile logFile;
  const QString path = dir() + QStringLiteral("/session.log");
  if (logFile.isOpen() && QDir::fromNativeSeparators(logFile.fileName()) != path) {
    logFile.close();
  }
  bool openedNow = false;
  if (!logFile.isOpen()) {
    QDir().mkpath(dir());
    logFile.setFileName(path);
    if (!logFile.open(QIODevice::Append | QIODevice::Text)) return;
    openedNow = true;
  }
  const bool rotatedBefore = rotateIfNeeded(logFile, path);
  if (!logFile.isOpen()) return;

  QTextStream ts(&logFile);
  if (openedNow || rotatedBefore)
    writeHeader(ts, rotatedBefore ? QStringLiteral("이전 기록은 session.old.log") : QString());
  const QString stamp = nowStamp();
  if (foldedNote) {
    ts << stamp << QStringLiteral(" (직전 메시지 %1회 반복)\n").arg(repeat);
  } else {
    if (repeat > 0) {
      ts << QStringLiteral("  (직전 메시지 총 %1회 반복)\n").arg(repeat + 1);
      repeat = 0;
    }
    lastLine = text;
    ts << stamp << ' ' << text << '\n';
  }
  ts.flush();
  logFile.flush();
  if (rotateIfNeeded(logFile, path) && logFile.isOpen()) {
    QTextStream next(&logFile);
    writeHeader(next, QStringLiteral("이전 기록은 session.old.log"));
    next.flush();
    logFile.flush();
  }
}
