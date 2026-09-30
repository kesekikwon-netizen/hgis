#include "SurveyFileFingerprint.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>

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

QMutex g_mutex;
QHash<QString, SurveyFileFingerprint::Fingerprint> g_remembered;

QString keyFor(const QString& path) {
  if (path.isEmpty()) return {};
  QString clean = QDir::cleanPath(QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath()));
#ifdef Q_OS_WIN
  clean = clean.toCaseFolded();
#endif
  return clean;
}

QString fileIdOf(const QString& path) {
#ifdef Q_OS_WIN
  const QString native = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
  // Attribute access only: never blocks writers, never changes the file.
  HANDLE handle = CreateFileW(reinterpret_cast<LPCWSTR>(native.utf16()), FILE_READ_ATTRIBUTES,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                              OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
  if (handle == INVALID_HANDLE_VALUE) return {};
  BY_HANDLE_FILE_INFORMATION info{};
  const bool ok = GetFileInformationByHandle(handle, &info) != FALSE;
  CloseHandle(handle);
  if (!ok || (info.nFileIndexHigh == 0 && info.nFileIndexLow == 0)) return {};
  const quint64 index = (static_cast<quint64>(info.nFileIndexHigh) << 32) | info.nFileIndexLow;
  return QStringLiteral("%1:%2")
      .arg(static_cast<quint32>(info.dwVolumeSerialNumber), 8, 16, QLatin1Char('0'))
      .arg(index, 16, 16, QLatin1Char('0'));
#else
  Q_UNUSED(path);
  return {};
#endif
}

}  // namespace

namespace SurveyFileFingerprint {

Fingerprint capture(const QString& path) {
  Fingerprint out;
  if (path.isEmpty()) return out;
  const QFileInfo info(path);
  if (!info.isFile()) return out;
  out.exists = true;
  out.size = info.size();
  out.modifiedMs = info.lastModified().toMSecsSinceEpoch();
  out.fileId = fileIdOf(path);
  return out;
}

void remember(const QString& path) {
  const QString key = keyFor(path);
  if (key.isEmpty()) return;
  const Fingerprint now = capture(path);
  QMutexLocker lock(&g_mutex);
  if (now.exists)
    g_remembered.insert(key, now);
  else
    g_remembered.remove(key);
}

void forget(const QString& path) {
  const QString key = keyFor(path);
  QMutexLocker lock(&g_mutex);
  g_remembered.remove(key);
}

bool isRemembered(const QString& path) {
  const QString key = keyFor(path);
  QMutexLocker lock(&g_mutex);
  return g_remembered.contains(key);
}

bool replacedElsewhere(const QString& path, QString* detail) {
  if (detail) detail->clear();
  const QString key = keyFor(path);
  Fingerprint before;
  {
    QMutexLocker lock(&g_mutex);
    const auto it = g_remembered.constFind(key);
    if (it == g_remembered.cend()) return false;
    before = it.value();
  }
  const Fingerprint now = capture(path);
  if (!now.exists) {
    if (detail)
      *detail = QStringLiteral("연 뒤에 조사 파일이 원래 자리에서 없어졌습니다. 다른 곳에서 옮기거나 "
                               "지웠을 수 있습니다.");
    return before.exists;
  }
  if (before.fileId.isEmpty() || now.fileId.isEmpty() || before.fileId == now.fileId) return false;
  if (detail)
    *detail = QStringLiteral("연 뒤에 다른 곳에서 이 조사 파일을 저장했습니다(%1). 같은 이름으로 저장하면 "
                             "그 내용을 덮어씁니다. 「다른 이름으로 저장」을 권합니다.")
                  .arg(QDateTime::fromMSecsSinceEpoch(now.modifiedMs)
                           .toString(QStringLiteral("yyyy-MM-dd HH:mm")));
  return true;
}

}  // namespace SurveyFileFingerprint
