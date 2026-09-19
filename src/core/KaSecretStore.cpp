#include "KaSecretStore.h"

#include <QSettings>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincrypt.h>

namespace {

QString plainKey(const QString& group) { return group + QStringLiteral("/password"); }
QString protectedKey(const QString& group) { return group + QStringLiteral("/password_dpapi"); }

// Extra entropy is not a secret. It stops a generic DPAPI dump that passes NULL entropy.
const char kEntropy[] = "ka-hgis-account-v1";

bool protectBytes(const QByteArray& plain, QByteArray* out) {
  if (!out) return false;
  out->clear();
  DATA_BLOB input;
  input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plain.constData()));
  input.cbData = static_cast<DWORD>(plain.size());
  DATA_BLOB entropy;
  entropy.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(kEntropy));
  entropy.cbData = static_cast<DWORD>(sizeof(kEntropy) - 1);
  DATA_BLOB output{};
  const BOOL ok = CryptProtectData(&input, L"ka-hgis", &entropy, nullptr, nullptr,
                                   CRYPTPROTECT_UI_FORBIDDEN, &output);
  if (!ok || !output.pbData || output.cbData == 0) return false;
  *out = QByteArray(reinterpret_cast<const char*>(output.pbData), static_cast<int>(output.cbData)).toBase64();
  LocalFree(output.pbData);
  return !out->isEmpty();
}

bool unprotectBytes(const QByteArray& base64, QByteArray* out) {
  if (!out) return false;
  out->clear();
  const QByteArray blob = QByteArray::fromBase64(base64);
  if (blob.isEmpty()) return false;
  DATA_BLOB input;
  input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(blob.constData()));
  input.cbData = static_cast<DWORD>(blob.size());
  DATA_BLOB entropy;
  entropy.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(kEntropy));
  entropy.cbData = static_cast<DWORD>(sizeof(kEntropy) - 1);
  DATA_BLOB output{};
  const BOOL ok = CryptUnprotectData(&input, nullptr, &entropy, nullptr, nullptr,
                                     CRYPTPROTECT_UI_FORBIDDEN, &output);
  if (!ok || !output.pbData) return false;
  *out = QByteArray(reinterpret_cast<const char*>(output.pbData), static_cast<int>(output.cbData));
  SecureZeroMemory(output.pbData, output.cbData);
  LocalFree(output.pbData);
  return true;
}

}  // namespace

bool KaSecretStore::writePassword(QSettings& settings, const QString& group, const QString& password,
                                  QString* error) {
  if (error) error->clear();
  if (password.isEmpty()) {
    settings.remove(plainKey(group));
    settings.remove(protectedKey(group));
    return true;
  }
  QByteArray utf8 = password.toUtf8();
  QByteArray blob;
  const bool ok = protectBytes(utf8, &blob);
  SecureZeroMemory(utf8.data(), static_cast<size_t>(utf8.size()));
  if (!ok) {
    if (error) *error = QStringLiteral("비밀번호를 암호화하지 못했습니다. 평문으로는 저장하지 않습니다.");
    return false;
  }
  settings.setValue(protectedKey(group), QString::fromLatin1(blob));
  settings.remove(plainKey(group));
  return true;
}

QString KaSecretStore::readPassword(QSettings& settings, const QString& group, bool migrate) {
  const QString stored = settings.value(protectedKey(group)).toString();
  if (!stored.isEmpty()) {
    QByteArray plain;
    if (unprotectBytes(stored.toLatin1(), &plain)) {
      const QString secret = QString::fromUtf8(plain);
      SecureZeroMemory(plain.data(), static_cast<size_t>(plain.size()));
      return secret;
    }
  }
  const QString legacy = settings.value(plainKey(group)).toString();
  if (legacy.isEmpty() || !migrate) return legacy;
  QString error;
  if (!writePassword(settings, group, legacy, &error)) return legacy;
  settings.sync();
  if (settings.status() != QSettings::NoError) return legacy;
  return legacy;
}
