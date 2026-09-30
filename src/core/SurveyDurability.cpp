#include "SurveyDurability.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(Q_OS_UNIX)
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace SurveyDurability {

bool flushPathToDisk(const QString& path) {
  if (path.isEmpty() || !QFileInfo(path).isFile()) return false;
#ifdef Q_OS_WIN
  // A second handle is enough: the cache manager keeps one cache per file, so
  // FlushFileBuffers on any write handle writes every dirty page of that file.
  const QString native = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
  HANDLE handle = CreateFileW(reinterpret_cast<LPCWSTR>(native.utf16()), GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (handle == INVALID_HANDLE_VALUE) return false;
  const bool flushed = FlushFileBuffers(handle) != FALSE;
  CloseHandle(handle);
  return flushed;
#elif defined(Q_OS_UNIX)
  const int fd = ::open(QFile::encodeName(path).constData(), O_WRONLY);
  if (fd < 0) return false;
  const bool flushed = ::fsync(fd) == 0;
  ::close(fd);
  return flushed;
#else
  return true;
#endif
}

bool flushToDisk(QFile& file) {
  if (!file.isOpen() || !file.flush()) return false;
  return flushPathToDisk(file.fileName());
}

bool isTransientLockError(quint32 nativeError) {
#ifdef Q_OS_WIN
  return nativeError == ERROR_ACCESS_DENIED || nativeError == ERROR_SHARING_VIOLATION ||
         nativeError == ERROR_LOCK_VIOLATION;
#else
  Q_UNUSED(nativeError);
  return false;
#endif
}

bool replaceFileDurably(const QString& from, const QString& to, quint32* nativeError,
                        QString* errorText) {
  if (nativeError) *nativeError = 0;
#ifdef Q_OS_WIN
  const QString source = QDir::toNativeSeparators(from);
  const QString target = QDir::toNativeSeparators(to);
  // WRITE_THROUGH: do not return before the rename itself is on the disk.
  if (MoveFileExW(reinterpret_cast<LPCWSTR>(source.utf16()), reinterpret_cast<LPCWSTR>(target.utf16()),
                  MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    return true;
  const DWORD code = GetLastError();
  if (nativeError) *nativeError = static_cast<quint32>(code);
  if (errorText) {
    wchar_t* text = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, code, 0, reinterpret_cast<LPWSTR>(&text), 0, nullptr);
    *errorText = text ? QString::fromWCharArray(text).trimmed()
                      : QStringLiteral("교체 오류 %1").arg(static_cast<int>(code));
    if (text) LocalFree(text);
  }
  return false;
#elif defined(Q_OS_UNIX)
  if (::rename(QFile::encodeName(from).constData(), QFile::encodeName(to).constData()) == 0) return true;
  if (errorText) *errorText = QStringLiteral("이름을 바꾸지 못했습니다.");
  return false;
#else
  if (QFile::exists(to) && !QFile::remove(to)) {
    if (errorText) *errorText = QStringLiteral("기존 파일을 비우지 못했습니다.");
    return false;
  }
  if (QFile::rename(from, to)) return true;
  if (errorText) *errorText = QStringLiteral("이름을 바꾸지 못했습니다.");
  return false;
#endif
}

bool copyFileDurably(const QString& source, const QString& target, QString* errorOut) {
  const auto fail = [&](const QString& message) {
    if (errorOut) *errorOut = message;
    QFile::remove(target);
    return false;
  };
  QFile input(source);
  if (!input.open(QIODevice::ReadOnly)) return fail(input.errorString());
  QFile output(target);
  if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) return fail(output.errorString());
  while (!input.atEnd()) {
    const QByteArray data = input.read(1024 * 1024);
    if (input.error() != QFileDevice::NoError || output.write(data) != data.size()) {
      output.close();
      return fail(QStringLiteral("사본을 끝까지 기록하지 못했습니다."));
    }
  }
  const bool flushed = flushToDisk(output);
  output.close();
  input.close();
  if (!flushed) return fail(QStringLiteral("사본을 디스크에 끝까지 기록하지 못했습니다."));
  if (QFileInfo(target).size() != QFileInfo(source).size())
    return fail(QStringLiteral("사본 크기가 원본과 다릅니다."));
  return true;
}

}  // namespace SurveyDurability
