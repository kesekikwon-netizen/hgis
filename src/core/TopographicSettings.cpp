#include "TopographicSettings.h"
#include "KaPortableRuntime.h"
#include "KaSecretStore.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace {
QString personalPath() {
  const QString directory = KaPortableRuntime::userConfigDir();
  return directory.isEmpty() ? QString() : QDir(directory).filePath(QStringLiteral("ngii-account.ini"));
}
TopographicSettings::Credentials readIni(const QString& path, bool migrate) {
  if (!QFileInfo(path).isFile()) return {};
  QSettings settings(path, QSettings::IniFormat);
  settings.setFallbacksEnabled(false);
  settings.sync();
  if (settings.status() != QSettings::NoError) return {};
  return {settings.value(QStringLiteral("ngii/username")).toString(),
          KaSecretStore::readPassword(settings, QStringLiteral("ngii"), migrate)};
}
QStringList bundledFiles() {
  const QDir app(QCoreApplication::applicationDirPath());
  return {app.filePath(QStringLiteral("config/ngii-local.ini")),
          app.filePath(QStringLiteral("../../config/ngii-local.ini"))};
}
}

TopographicSettings::Credentials TopographicSettings::credentials() {
  return readFromFiles(personalPath(), bundledFiles());
}

bool TopographicSettings::hasCredentials() {
  const Credentials c = readFromFiles(personalPath(), bundledFiles(), false);
  return !c.username.trimmed().isEmpty() && !c.password.isEmpty();
}

TopographicSettings::Credentials TopographicSettings::readFromFiles(
    const QString& personalFile, const QStringList& fallbackFiles, bool migrate) {
  // Existence, not a non-empty password, controls priority. Saving empty values
  // disables this PC's default login instead of restoring the bundled account.
  if (!personalFile.isEmpty() && QFileInfo::exists(personalFile)) {
    const Credentials personal = readIni(personalFile, migrate);
    if (!personal.password.isEmpty()) return personal;
    QSettings probe(personalFile, QSettings::IniFormat);
    probe.setFallbacksEnabled(false);
    if (!KaSecretStore::hasUndecryptablePassword(probe, QStringLiteral("ngii")))
      return personal;
  }
  for (const auto& fallback : fallbackFiles)
    if (QFileInfo::exists(fallback)) return readIni(fallback, false);
  return {};
}

bool TopographicSettings::saveCredentials(const Credentials& credentials, QString* error) {
  return saveToFile(personalPath(), credentials, error);
}

bool TopographicSettings::saveToFile(const QString& personalFile, const Credentials& credentials, QString* error) {
  if (error) error->clear();
  auto fail = [error](const QString& message) { if (error) *error = message; return false; };
  if (personalFile.isEmpty()) return fail(QStringLiteral("이 PC의 사용자 설정 경로를 찾지 못했습니다."));
  const auto directory = QFileInfo(personalFile).absolutePath();
  if (!QDir().mkpath(directory)) return fail(QStringLiteral("계정 설정 폴더를 만들지 못했습니다. 폴더 권한을 확인하세요."));
  QTemporaryDir staging(QDir(directory).filePath(QStringLiteral(".ngii-account-XXXXXX")));
  if (!staging.isValid()) return fail(QStringLiteral("계정 설정을 준비하지 못했습니다. 폴더 권한을 확인하세요."));
  const auto serialized = staging.filePath(QStringLiteral("account.ini"));
  {
    // Username stays plain. Installed builds use DPAPI; portable uses
    // password_portable so another PC can read the same folder.
    QSettings settings(serialized, QSettings::IniFormat);
    settings.setFallbacksEnabled(false);
    settings.setValue(QStringLiteral("ngii/username"), credentials.username);
    if (!KaSecretStore::writePassword(settings, QStringLiteral("ngii"), credentials.password, error))
      return false;
    settings.sync();
    if (settings.status() != QSettings::NoError)
      return fail(QStringLiteral("계정 설정을 기록하지 못했습니다. 저장 공간과 폴더 권한을 확인하세요."));
  }
  QFile input(serialized);
  if (!input.open(QIODevice::ReadOnly)) return fail(QStringLiteral("준비한 계정 설정을 읽지 못했습니다."));
  const auto data = input.readAll();
  if (input.error() != QFileDevice::NoError) return fail(QStringLiteral("준비한 계정 설정을 끝까지 읽지 못했습니다."));
  input.close();
  QSaveFile output(personalFile);
  output.setDirectWriteFallback(false);
  if (!output.open(QIODevice::WriteOnly) || output.write(data) != data.size() || !output.commit())
    return fail(QStringLiteral("계정 설정을 저장하지 못했습니다. 이전 설정은 유지됩니다: %1").arg(output.errorString()));
  return true;
}
