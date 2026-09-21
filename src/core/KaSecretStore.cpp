#include "KaSecretStore.h"
#include "KaPortableRuntime.h"

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

int g_portableOverride = -1;

QString plainKey(const QString& group) { return group + QStringLiteral("/password"); }
QString protectedKey(const QString& group) { return group + QStringLiteral("/password_dpapi"); }
QString portableKey(const QString& group) { return group + QStringLiteral("/password_portable"); }

// Extra entropy is not a secret. It stops a generic DPAPI dump that passes NULL entropy.
const char kEntropy[] = "ka-hgis-account-v1";

QByteArray xorEntropy(QByteArray data) {
  constexpr int n = static_cast<int>(sizeof(kEntropy) - 1);
  for (int i = 0; i < data.size(); ++i)
    data[i] = static_cast<char>(static_cast<unsigned char>(data[i]) ^
                                static_cast<unsigned char>(kEntropy[i % n]));
  return data;
}

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

void clearSecretKeys(QSettings& settings, const QString& group) {
  settings.remove(plainKey(group));
  settings.remove(protectedKey(group));
  settings.remove(portableKey(group));
}

bool writePortable(QSettings& settings, const QString& group, const QString& password) {
  QByteArray utf8 = password.toUtf8();
  const QByteArray wrapped = xorEntropy(utf8).toBase64();
  SecureZeroMemory(utf8.data(), static_cast<size_t>(utf8.size()));
  if (wrapped.isEmpty()) return false;
  settings.setValue(portableKey(group), QString::fromLatin1(wrapped));
  settings.remove(plainKey(group));
  settings.remove(protectedKey(group));
  return true;
}

QString unwrapPortable(const QString& stored) {
  if (stored.isEmpty()) return {};
  QByteArray raw = QByteArray::fromBase64(stored.toLatin1());
  if (raw.isEmpty()) return {};
  raw = xorEntropy(raw);
  const QString secret = QString::fromUtf8(raw);
  SecureZeroMemory(raw.data(), static_cast<size_t>(raw.size()));
  return secret;
}

}  // namespace

void KaSecretStore::setPortableSecretsForTests(bool portable) {
  g_portableOverride = portable ? 1 : 0;
}

void KaSecretStore::resetPortableSecretsForTests() {
  g_portableOverride = -1;
}

bool KaSecretStore::usePortableSecrets() {
  if (g_portableOverride >= 0) return g_portableOverride == 1;
  return KaPortableRuntime::discover(KaPortableRuntime::resolvedExeDir()).looksBundled();
}

bool KaSecretStore::writePassword(QSettings& settings, const QString& group, const QString& password,
                                  QString* error) {
  if (error) error->clear();
  if (password.isEmpty()) {
    clearSecretKeys(settings, group);
    return true;
  }
  if (usePortableSecrets()) {
    if (!writePortable(settings, group, password)) {
      if (error) *error = QStringLiteral("비밀번호를 포터블 형식으로 저장하지 못했습니다.");
      return false;
    }
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
  settings.remove(portableKey(group));
  return true;
}

QString KaSecretStore::readPassword(QSettings& settings, const QString& group, bool migrate) {
  const QString portable = unwrapPortable(settings.value(portableKey(group)).toString());
  if (!portable.isEmpty()) return portable;

  const QString stored = settings.value(protectedKey(group)).toString();
  if (!stored.isEmpty()) {
    QByteArray plain;
    if (unprotectBytes(stored.toLatin1(), &plain)) {
      const QString secret = QString::fromUtf8(plain);
      SecureZeroMemory(plain.data(), static_cast<size_t>(plain.size()));
      if (migrate && usePortableSecrets()) {
        QString error;
        writePassword(settings, group, secret, &error);
        settings.sync();
      }
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

bool KaSecretStore::hasUndecryptablePassword(QSettings& settings, const QString& group) {
  if (!unwrapPortable(settings.value(portableKey(group)).toString()).isEmpty()) return false;
  if (!settings.value(plainKey(group)).toString().isEmpty()) return false;
  const QString stored = settings.value(protectedKey(group)).toString();
  if (stored.isEmpty()) return false;
  QByteArray plain;
  if (unprotectBytes(stored.toLatin1(), &plain)) {
    SecureZeroMemory(plain.data(), static_cast<size_t>(plain.size()));
    return false;
  }
  return true;
}
