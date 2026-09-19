#include "KaSessionLog.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QRecursiveMutex>
#include <QTextStream>

QString KaSessionLog::dir() {
  const QString overridden = qEnvironmentVariable("KA_HGIS_LOG_DIR");
  if (!overridden.isEmpty()) return QDir::fromNativeSeparators(overridden);
  const QByteArray localAppData = qgetenv("LOCALAPPDATA");
  const QString base = localAppData.isEmpty() ? QDir::tempPath()
                                              : QString::fromLocal8Bit(localAppData);
  return base + QStringLiteral("/ka-hgis/logs");
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

QString KaSessionLog::dumpHint() {
  return QStringLiteral(
             "세션 로그(session.log, %1MB 초과 시 session.old.log로 회전)와 "
             "크래시 로그(crash-날짜.log)·미니덤프(crash-날짜.dmp): %2")
      .arg(kDefaultMaxBytes / (1024 * 1024))
      .arg(QDir::toNativeSeparators(dir()));
}

namespace {

void rotateIfNeeded(QFile& logFile, const QString& path) {
  if (!logFile.isOpen() || logFile.size() <= KaSessionLog::maxBytes()) return;
  logFile.close();
  const QString oldPath = KaSessionLog::dir() + QStringLiteral("/session.old.log");
  // QFile::rename does not overwrite an existing file.
  // https://doc.qt.io/qt-6/qfile.html
  QFile::remove(oldPath);
  QFile::rename(path, oldPath);
  logFile.setFileName(path);
  if (!logFile.open(QIODevice::Append | QIODevice::Text)) return;
}

}  // namespace

void KaSessionLog::line(const QString& text) {
  thread_local bool inLogLine = false;
  if (inLogLine) return;
  struct InLogLineGuard {
    bool& flag;
    explicit InLogLineGuard(bool& f) : flag(f) { flag = true; }
    ~InLogLineGuard() { flag = false; }
  } inLogLineGuard(inLogLine);

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
  if (!logFile.isOpen()) {
    QDir().mkpath(dir());
    logFile.setFileName(path);
    if (!logFile.open(QIODevice::Append | QIODevice::Text)) return;
  }
  rotateIfNeeded(logFile, path);
  if (!logFile.isOpen()) return;

  QTextStream ts(&logFile);
  const QString stamp =
      QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
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
  rotateIfNeeded(logFile, path);
}
