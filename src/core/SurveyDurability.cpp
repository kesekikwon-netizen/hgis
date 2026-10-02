#include "SurveyDurability.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <vector>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <RestartManager.h>
#pragma comment(lib, "Rstrtmgr.lib")
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

namespace {
#ifdef Q_OS_WIN
// Ends the Restart Manager session on every path (the API allows 64 per user session).
struct RmSession {
  DWORD handle = 0;
  bool open = false;
  ~RmSession() {
    if (open) RmEndSession(handle);
  }
};

// The exe file name: Strata's own exe carries no description, so Restart Manager names it with a blank.
QString imageName(DWORD pid) {
  HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (!process) return {};
  std::vector<wchar_t> buffer(32768);
  DWORD size = static_cast<DWORD>(buffer.size());
  QString name;
  if (QueryFullProcessImageNameW(process, 0, buffer.data(), &size))
    name = QFileInfo(QString::fromWCharArray(buffer.data(), static_cast<int>(size))).fileName();
  CloseHandle(process);
  return name;
}
#endif
}  // namespace

QStringList processesHolding(const QString& path, QString* note) {
  if (note) note->clear();
  QStringList holders;
#ifdef Q_OS_WIN
  const auto fail = [note](const QString& step, DWORD code) {
    if (note) *note = QStringLiteral("Restart Manager %1 오류 %2").arg(step, QString::number(code));
    return QStringList();
  };
  RmSession session;
  WCHAR key[CCH_RM_SESSION_KEY + 1] = {};
  DWORD rc = RmStartSession(&session.handle, 0, key);
  if (rc != ERROR_SUCCESS) return fail(QStringLiteral("시작"), rc);
  session.open = true;
  const QString native = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
  LPCWSTR files[] = {reinterpret_cast<LPCWSTR>(native.utf16())};
  rc = RmRegisterResources(session.handle, 1, files, 0, nullptr, 0, nullptr);
  if (rc != ERROR_SUCCESS) return fail(QStringLiteral("등록"), rc);
  std::vector<RM_PROCESS_INFO> info;
  UINT needed = 0;
  UINT count = 0;
  DWORD reasons = 0;
  rc = ERROR_MORE_DATA;
  // The list can grow between calls; ask again with room to spare a few times.
  for (int attempt = 0; attempt < 4 && rc == ERROR_MORE_DATA; ++attempt) {
    info.resize(needed + 2);
    count = static_cast<UINT>(info.size());
    rc = RmGetList(session.handle, &needed, &count, info.data(), &reasons);
  }
  if (rc != ERROR_SUCCESS) return fail(QStringLiteral("목록"), rc);
  const DWORD self = GetCurrentProcessId();
  for (UINT i = 0; i < count; ++i) {
    const DWORD pid = info[i].Process.dwProcessId;
    const QString exe = imageName(pid);
    const QString description = QString::fromWCharArray(info[i].strAppName).trimmed();
    QString label = exe.isEmpty() ? description : exe;
    if (!exe.isEmpty() && !description.isEmpty() && description.compare(exe, Qt::CaseInsensitive) != 0)
      label += QStringLiteral(" «%1»").arg(description);
    if (label.isEmpty()) label = QStringLiteral("이름 모름");
    holders << QStringLiteral("%1 (pid %2%3)")
                   .arg(label, QString::number(pid), pid == self ? QStringLiteral(" · 이 앱") : QString());
  }
#else
  Q_UNUSED(path);
  if (note) *note = QStringLiteral("Windows 밖");
#endif
  return holders;
}

QString lockDiagnosis(const QString& path) {
  QString note;
  const QStringList holders = processesHolding(path, &note);
  QString text = !holders.isEmpty() ? QStringLiteral("열고 있는 프로그램: %1").arg(holders.join(QStringLiteral(", ")))
                 : !note.isEmpty()  ? QStringLiteral("열고 있는 프로그램 확인하지 못함(%1)").arg(note)
                                    : QStringLiteral("열고 있는 프로그램 없음(Restart Manager 기준)");
#ifdef Q_OS_WIN
  const QString native = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
  const DWORD attributes = GetFileAttributesW(reinterpret_cast<LPCWSTR>(native.utf16()));
  if (attributes == INVALID_FILE_ATTRIBUTES)
    text += QStringLiteral(" · 속성 확인 못 함");
  else
    text += (attributes & FILE_ATTRIBUTE_READONLY) ? QStringLiteral(" · 읽기 전용") : QStringLiteral(" · 읽기 전용 아님");
#endif
  return text;
}

}  // namespace SurveyDurability
