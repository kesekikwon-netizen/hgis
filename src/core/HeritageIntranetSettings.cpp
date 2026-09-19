#include "HeritageIntranetSettings.h"
#include "KaPortableRuntime.h"
#include "KaSecretStore.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QTemporaryDir>

namespace {

QString personalPath() {
  const QString directory = KaPortableRuntime::userConfigDir();
  return directory.isEmpty() ? QString()
                             : QDir(directory).filePath(QStringLiteral("heritage-account.ini"));
}

HeritageIntranetSettings::Credentials readIni(const QString& path, bool migrate) {
  if (!QFileInfo(path).isFile()) return {};
  QSettings settings(path, QSettings::IniFormat);
  settings.setFallbacksEnabled(false);
  settings.sync();
  if (settings.status() != QSettings::NoError) return {};
  return {settings.value(QStringLiteral("heritage/username")).toString(),
          KaSecretStore::readPassword(settings, QStringLiteral("heritage"), migrate)};
}

}  // namespace

HeritageIntranetSettings::Credentials HeritageIntranetSettings::credentials() {
  const QDir app(QCoreApplication::applicationDirPath());
  return readFromFiles(personalPath(), {
      app.filePath(QStringLiteral("config/heritage-local.ini")),
      app.filePath(QStringLiteral("../../config/heritage-local.ini"))});
}

HeritageIntranetSettings::Credentials HeritageIntranetSettings::readFromFiles(
    const QString& personalFile, const QStringList& fallbackFiles) {
  // 존재 여부가 우선순위를 정한다. 빈 값을 저장하면 이 PC의 기본 로그인을 끄는 것이지
  // 번들 계정으로 되돌아가는 것이 아니다. (TopographicSettings 와 같은 규칙)
  if (!personalFile.isEmpty() && QFileInfo::exists(personalFile)) return readIni(personalFile, true);
  for (const auto& fallback : fallbackFiles)
    if (QFileInfo::exists(fallback)) return readIni(fallback, false);
  return {};
}

bool HeritageIntranetSettings::hasCredentials() {
  const Credentials c = credentials();
  return !c.username.trimmed().isEmpty() && !c.password.isEmpty();
}

QString HeritageIntranetSettings::describeForLog() {
  const Credentials c = credentials();
  if (c.username.trimmed().isEmpty()) return QStringLiteral("계정 없음");
  // 계정 식별자와 비밀번호는 값·길이 모두 남기지 않는다.
  return c.password.isEmpty() ? QStringLiteral("계정 설정 미완료")
                             : QStringLiteral("저장된 계정 사용");
}

bool HeritageIntranetSettings::saveCredentials(const Credentials& credentials, QString* error) {
  return saveToFile(personalPath(), credentials, error);
}

bool HeritageIntranetSettings::saveToFile(const QString& personalFile,
                                          const Credentials& credentials, QString* error) {
  if (error) error->clear();
  auto fail = [error](const QString& message) {
    if (error) *error = message;
    return false;
  };
  if (personalFile.isEmpty())
    return fail(QStringLiteral("이 PC의 사용자 설정 경로를 찾지 못했습니다."));
  const auto directory = QFileInfo(personalFile).absolutePath();
  if (!QDir().mkpath(directory))
    return fail(QStringLiteral("계정 설정 폴더를 만들지 못했습니다. 폴더 권한을 확인하세요."));
  QTemporaryDir staging(QDir(directory).filePath(QStringLiteral(".heritage-account-XXXXXX")));
  if (!staging.isValid())
    return fail(QStringLiteral("계정 설정을 준비하지 못했습니다. 폴더 권한을 확인하세요."));
  const auto serialized = staging.filePath(QStringLiteral("account.ini"));
  {
    // 사용자 이름은 평문이다. 비밀번호는 DPAPI 암호문으로만 저장한다.
    QSettings settings(serialized, QSettings::IniFormat);
    settings.setFallbacksEnabled(false);
    settings.setValue(QStringLiteral("heritage/username"), credentials.username);
    if (!KaSecretStore::writePassword(settings, QStringLiteral("heritage"), credentials.password, error))
      return false;
    settings.sync();
    if (settings.status() != QSettings::NoError)
      return fail(QStringLiteral("계정 설정을 기록하지 못했습니다. 저장 공간과 폴더 권한을 확인하세요."));
  }
  QFile input(serialized);
  if (!input.open(QIODevice::ReadOnly))
    return fail(QStringLiteral("준비한 계정 설정을 읽지 못했습니다."));
  const auto data = input.readAll();
  if (input.error() != QFileDevice::NoError)
    return fail(QStringLiteral("준비한 계정 설정을 끝까지 읽지 못했습니다."));
  input.close();
  QSaveFile output(personalFile);
  output.setDirectWriteFallback(false);
  if (!output.open(QIODevice::WriteOnly) || output.write(data) != data.size() || !output.commit())
    return fail(QStringLiteral("계정 설정을 저장하지 못했습니다. 이전 설정은 유지됩니다: %1")
                    .arg(output.errorString()));
  return true;
}
