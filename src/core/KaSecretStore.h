#pragma once

#include <QString>

class QSettings;

// Windows DPAPI (CryptProtectData, current user, no prompt).
// https://learn.microsoft.com/en-us/windows/win32/api/dpapi/nf-dpapi-cryptprotectdata
// The INI keeps username in the clear and the password only as password_dpapi.
namespace KaSecretStore {

bool writePassword(QSettings& settings, const QString& group, const QString& password, QString* error = nullptr);

// migrate=true rewrites a personal file: plaintext password is encrypted and removed.
// migrate=false is for bundled fallback files that must not be rewritten.
QString readPassword(QSettings& settings, const QString& group, bool migrate);

}  // namespace KaSecretStore
