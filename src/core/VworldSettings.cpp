#include "VworldSettings.h"
#include "KaPortableRuntime.h"
#include <QSettings>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QStringList>
#include <QCoreApplication>

static QString orgName() { return QStringLiteral("ka-hgis"); }
static QString appName() { return QStringLiteral("ka-hgis"); }
static QString ssotKey() { return QStringLiteral("VWorld/ApiKey"); }
static QString legacyKey() { return QStringLiteral("vworld/apiKey"); }
static QString historySsotKey() { return QStringLiteral("HistoryGis/ApiKey"); }

static bool isPortableBundle() {
  return KaPortableRuntime::discover(KaPortableRuntime::resolvedExeDir()).looksBundled();
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
  const QString ini = QDir(KaPortableRuntime::userConfigDir()).filePath(QStringLiteral("ka-hgis-vworld.ini"));
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
    QSettings native(orgName(), appName());
    key = native.value(ssotKey()).toString().trimmed();
    if (key.isEmpty())
      key = native.value(legacyKey()).toString().trimmed();
    if (!key.isEmpty()) {
      writeKey(settings, key);
      writeKey(native, key);
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
  if (key.isEmpty()) {
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

  QSettings native(orgName(), appName());
  writeKey(native, k);
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
    QSettings native(orgName(), appName());
    key = native.value(historySsotKey()).toString().trimmed();
    if (!key.isEmpty()) {
      writeHistoryKey(settings, key);
      writeHistoryKey(native, key);
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
  QSettings native(orgName(), appName());
  writeHistoryKey(native, k);
}
