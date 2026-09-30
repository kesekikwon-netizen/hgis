#include "KaHomeStatus.h"

#include "core/RecentSurveys.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

#include <qgscoordinatereferencesystem.h>

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

// Mapped network drives (Z:) and UNC paths can stall for seconds when the server is gone;
// the home screen never waits on them.
bool isNetworkPath(const QString& directory) {
  const QString native = QDir::toNativeSeparators(QFileInfo(directory).absoluteFilePath());
  if (native.startsWith(QLatin1String("\\\\"))) return true;
#ifdef Q_OS_WIN
  if (native.size() >= 2 && native.at(1) == QLatin1Char(':')) {
    const std::wstring root = native.left(2).toStdWString() + L"\\";
    return GetDriveTypeW(root.c_str()) == DRIVE_REMOTE;
  }
#endif
  return false;
}

}  // namespace

namespace KaHomeStatus {

QString defaultSurveyFolder() {
  QSettings st = RecentSurveys::userSettings();
  const QString remembered = st.value(QStringLiteral("Survey/LastDir")).toString();
  if (!remembered.isEmpty()) return remembered;
  const QString last = RecentSurveys::lastPath(st);
  if (!last.isEmpty()) return QFileInfo(last).absolutePath();
  return QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
}

Folder folderState(const QString& directory) {
  if (directory.isEmpty()) return Folder::None;
  if (isNetworkPath(directory)) return Folder::Network;
#ifdef Q_OS_WIN
  QNtfsPermissionCheckGuard ntfs;  // ask the ACL, not only the read-only attribute
#endif
  const QFileInfo info(directory);
  if (!info.isDir()) return Folder::Missing;
  return info.isWritable() ? Folder::Writable : Folder::ReadOnly;
}

bool crsDatabaseReady() {
  for (const char* id : {"EPSG:5186", "EPSG:5187", "EPSG:5179"})
    if (!QgsCoordinateReferenceSystem(QString::fromLatin1(id)).isValid()) return false;
  return true;
}

Inputs collectLocal() {
  Inputs in;
  in.accounts = AccountStatus::snapshot();
  in.crsDatabase = crsDatabaseReady();
  in.folderPath = defaultSurveyFolder();
  in.folder = folderState(in.folderPath);
  return in;
}

QVector<Line> describe(const Inputs& inputs) {
  QVector<Line> lines;
  for (const AccountStatus::Entry& entry : inputs.accounts)
    lines.append({QStringLiteral("%1 · %2").arg(entry.label, AccountStatus::stateText(entry.ready)),
                  entry.ready ? QString() : QStringLiteral("%1 — %2").arg(entry.menuPath, entry.usedFor),
                  entry.ready});
  lines.append({inputs.crsDatabase ? QStringLiteral("좌표계 자료(proj.db) · 정상")
                                   : QStringLiteral("좌표계 자료(proj.db) · 확인 필요"),
                inputs.crsDatabase ? QString()
                                   : QStringLiteral("5186·5187·5179를 읽지 못했습니다. 프로그램 폴더의 "
                                                    "share/proj 가 온전한지 확인하세요."),
                inputs.crsDatabase});
  const QString folder = QDir::toNativeSeparators(inputs.folderPath);
  switch (inputs.folder) {
    case Folder::None:
      lines.append({QStringLiteral("조사 저장 폴더 · 아직 없음"), QString(), true});
      break;
    case Folder::Writable:
      lines.append({QStringLiteral("조사 저장 폴더 · 쓰기 가능"), folder, true});
      break;
    case Folder::ReadOnly:
      lines.append({QStringLiteral("조사 저장 폴더 · 쓸 수 없음"),
                    QStringLiteral("%1 — 새 조사는 다른 폴더를 고르세요.").arg(folder), false});
      break;
    case Folder::Missing:
      lines.append({QStringLiteral("조사 저장 폴더 · 찾을 수 없음"),
                    QStringLiteral("%1 — USB·외장 드라이브가 연결됐는지 확인하세요.").arg(folder), false});
      break;
    case Folder::Network:
      lines.append({QStringLiteral("조사 저장 폴더 · 네트워크(확인 생략)"), folder, true});
      break;
  }
  return lines;
}

}  // namespace KaHomeStatus
