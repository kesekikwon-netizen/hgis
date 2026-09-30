#include "VworldSettings.h"
#include "KaPortableRuntime.h"
#include <QSettings>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QStringList>
#include <QCoreApplication>

namespace {
// Current store scope (F037). Guarded: the key is read from worker threads (tile jobs).
QMutex g_scopeMutex;
VworldSettings::Scope g_scope;
}  // namespace

void VworldSettings::setScope(const Scope& scope) {
  Scope clean = scope;
  clean.organization = clean.organization.trimmed();
  clean.application = clean.application.trimmed();
  clean.folder = clean.folder.trimmed();
  if (clean.organization.isEmpty()) clean.organization = QStringLiteral("ka-hgis");
  if (clean.application.isEmpty()) clean.application = QStringLiteral("ka-hgis");
  if (!clean.folder.isEmpty()) {
    clean.folder = QDir(clean.folder).absolutePath();
    QDir().mkpath(clean.folder);
  }
  const QMutexLocker lock(&g_scopeMutex);
  g_scope = clean;
}

VworldSettings::Scope VworldSettings::scope() {
  const QMutexLocker lock(&g_scopeMutex);
  return g_scope;
}

void VworldSettings::resetScope() { setScope(Scope{}); }

static QString ssotKey() { return QStringLiteral("VWorld/ApiKey"); }
static QString legacyKey() { return QStringLiteral("vworld/apiKey"); }
static QString historySsotKey() { return QStringLiteral("HistoryGis/ApiKey"); }

static bool isPortableBundle() {
  return KaPortableRuntime::discover(KaPortableRuntime::resolvedExeDir()).looksBundled();
}

// Native store: QSettings(organization, application), or an INI file standing in for it
// when the scope has a folder (tests), so the registry is never written there.
std::unique_ptr<QSettings> VworldSettings::openNativeStore() {
  const Scope current = scope();
  if (current.folder.isEmpty())
    return std::make_unique<QSettings>(current.organization, current.application);
  const QString file = QStringLiteral("native-%1-%2.ini").arg(current.organization, current.application);
  return std::make_unique<QSettings>(QDir(current.folder).filePath(file), QSettings::IniFormat);
}

// Folder of ka-hgis-vworld.ini: the portable config folder, the scope folder, or AppConfig.
static QString keyIniFolder() {
  if (!isPortableBundle()) {
    const QString folder = VworldSettings::scope().folder;
    if (!folder.isEmpty()) return folder;
  }
  return KaPortableRuntime::userConfigDir();
}

static QString readKeyFromIni(const QString& path) {
  if (path.isEmpty() || !QFile::exists(path))
    return {};
  QSettings ini(path, QSettings::IniFormat);
  QString k = ini.value(ssotKey()).toString().trimmed();
  if (k.isEmpty())
    k = ini.value(legacyKey()).toString().trimmed();
  if (k.isEmpty())
    k = ini.value(QStringLiteral("apiKey")).toString().trimmed();
  return k;
}

static QString portableSecretsPath() {
  return QDir(KaPortableRuntime::userConfigDir()).filePath(QStringLiteral("secrets.ini"));
}

static QString readRepoSecretsIni() {
  const QStringList cands = {
      QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../config/secrets.ini")),
      QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../../config/secrets.ini")),
      QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("config/secrets.ini")),
      QDir::current().filePath(QStringLiteral("config/secrets.ini")),
  };
  for (const QString& p : cands) {
    const QString k = readKeyFromIni(p);
    if (!k.isEmpty())
      return k;
  }
  return {};
}

static QSettings makeSettings() {
  const QString ini = QDir(keyIniFolder()).filePath(QStringLiteral("ka-hgis-vworld.ini"));
  return QSettings(ini, QSettings::IniFormat);
}

static void writeKey(QSettings& settings, const QString& key) {
  // Clear then set: never remove(legacy) after set(ssot) — case-fold would wipe the value.
  settings.remove(ssotKey());
  if (!key.isEmpty())
    settings.setValue(ssotKey(), key);
  settings.sync();
}

QString VworldSettings::loadApiKey() {
  if (isPortableBundle()) {
    QSettings settings = makeSettings();
    QString key = settings.value(ssotKey()).toString().trimmed();
    if (key.isEmpty())
      key = settings.value(legacyKey()).toString().trimmed();
    if (key.isEmpty())
      key = readKeyFromIni(portableSecretsPath());
    if (key.isEmpty()) {
      const QByteArray env = qgetenv("VWORLD_API_KEY");
      if (!env.isEmpty())
        key = QString::fromUtf8(env).trimmed();
    }
    return key;
  }

  QSettings settings = makeSettings();
  QString key = settings.value(ssotKey()).toString().trimmed();
  if (!key.isEmpty())
    return key;

  {
    const std::unique_ptr<QSettings> native = openNativeStore();
    key = native->value(ssotKey()).toString().trimmed();
    if (key.isEmpty())
      key = native->value(legacyKey()).toString().trimmed();
    if (!key.isEmpty()) {
      writeKey(settings, key);
      writeKey(*native, key);
      return key;
    }
  }

  const QByteArray env = qgetenv("VWORLD_API_KEY");
  if (!env.isEmpty()) {
    key = QString::fromUtf8(env).trimmed();
    if (!key.isEmpty()) {
      writeKey(settings, key);
      return key;
    }
  }

  key = settings.value(legacyKey()).toString().trimmed();
  if (key.isEmpty() && scope().folder.isEmpty()) {
    // The application's default store (read only); not consulted in an isolated scope.
    QSettings bare;
    key = bare.value(legacyKey()).toString().trimmed();
  }
  if (key.isEmpty())
    key = readRepoSecretsIni();
  if (!key.isEmpty()) {
    writeKey(settings, key);
    return key;
  }
  return {};
}

void VworldSettings::saveApiKey(const QString& key) {
  const QString k = key.trimmed();
  QSettings settings = makeSettings();
  writeKey(settings, k);
  if (isPortableBundle()) {
    QSettings secrets(portableSecretsPath(), QSettings::IniFormat);
    writeKey(secrets, k);
    return;
  }

  const std::unique_ptr<QSettings> native = openNativeStore();
  writeKey(*native, k);
}

static void writeHistoryKey(QSettings& settings, const QString& key) {
  settings.remove(historySsotKey());
  if (!key.isEmpty())
    settings.setValue(historySsotKey(), key);
  settings.sync();
}

static QString readHistoryKeyFromIni(const QString& path) {
  if (path.isEmpty() || !QFile::exists(path))
    return {};
  QSettings ini(path, QSettings::IniFormat);
  return ini.value(historySsotKey()).toString().trimmed();
}

QString VworldSettings::loadHistoryGisApiKey() {
  QSettings settings = makeSettings();
  QString key = settings.value(historySsotKey()).toString().trimmed();
  if (!key.isEmpty())
    return key;

  if (isPortableBundle()) {
    key = readHistoryKeyFromIni(portableSecretsPath());
    if (!key.isEmpty())
      return key;
  } else {
    const std::unique_ptr<QSettings> native = openNativeStore();
    key = native->value(historySsotKey()).toString().trimmed();
    if (!key.isEmpty()) {
      writeHistoryKey(settings, key);
      writeHistoryKey(*native, key);
      return key;
    }
  }

  const QByteArray env = qgetenv("HISTORY_GIS_API_KEY");
  if (!env.isEmpty())
    return QString::fromUtf8(env).trimmed();
  return {};
}

void VworldSettings::saveHistoryGisApiKey(const QString& key) {
  const QString k = key.trimmed();
  QSettings settings = makeSettings();
  writeHistoryKey(settings, k);
  if (isPortableBundle()) {
    QSettings secrets(portableSecretsPath(), QSettings::IniFormat);
    writeHistoryKey(secrets, k);
    return;
  }
  const std::unique_ptr<QSettings> native = openNativeStore();
  writeHistoryKey(*native, k);
}
