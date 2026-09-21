#pragma once

#include <QString>

class QSettings;

// 설치본: Windows DPAPI (CryptProtectData, current user).
// https://learn.microsoft.com/en-us/windows/win32/api/dpapi/nf-dpapi-cryptprotectdata
// 포터블: 폴더와 같이 옮기는 password_portable. DPAPI는 다른 PC에서 풀린다.
namespace KaSecretStore {

void setPortableSecretsForTests(bool portable);
void resetPortableSecretsForTests();

bool usePortableSecrets();

bool writePassword(QSettings& settings, const QString& group, const QString& password, QString* error = nullptr);

// migrate=true 는 개인 파일을 현재 모드로 다시 쓴다. 번들 폴백은 migrate=false.
QString readPassword(QSettings& settings, const QString& group, bool migrate);

// password_dpapi 가 있으나 이 PC에서 풀리지 않고, 옮길 수 있는 값도 없을 때.
bool hasUndecryptablePassword(QSettings& settings, const QString& group);

}  // namespace KaSecretStore
