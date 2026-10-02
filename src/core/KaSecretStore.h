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

// migrate=true 는 평문 password= 만 현재 모드의 보호 형식으로 올린다. 번들 폴백은 migrate=false.
// 읽기만으로 DPAPI 값을 포터블 형식으로 낮추지 않는다(명시 저장 때만 바뀐다).
QString readPassword(QSettings& settings, const QString& group, bool migrate);

// 저장된 비밀번호가 이 PC에서 읽히는가. 값을 돌려주지 않고 파일도 고치지 않는다.
bool hasReadablePassword(QSettings& settings, const QString& group);

// password_dpapi 가 있으나 이 PC에서 풀리지 않고, 옮길 수 있는 값도 없을 때.
bool hasUndecryptablePassword(QSettings& settings, const QString& group);

}  // namespace KaSecretStore
