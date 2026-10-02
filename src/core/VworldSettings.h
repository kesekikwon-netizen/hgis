#pragma once
#include <QString>

#include <memory>

class QSettings;

class VworldSettings {
public:
  static QString loadApiKey();
  static void saveApiKey(const QString& key);
  // 국사편찬위원회 역사지리정보DB (hgis.history.go.kr). VWorld 키와 별도.
  static QString loadHistoryGisApiKey();
  static void saveHistoryGisApiKey(const QString& key);

  // Where the keys live (F037). The default scope is the app's real store: the native
  // QSettings(organization, application) — the Windows registry — plus
  // <KaPortableRuntime::userConfigDir()>/ka-hgis-vworld.ini.
  // A test sets `folder`: the INI and the native store then become files in that folder
  // (native-<organization>-<application>.ini), so saveApiKey() never touches the user's
  // registry or AppData. A different organization/application alone moves the native store
  // to another registry key. A portable bundle (KaPortableRuntime) keeps its own config
  // folder whatever the scope says. Environment keys and the read-only repo
  // config/secrets.ini are still consulted. Key values are never logged.
  struct Scope {
    QString organization = QStringLiteral("ka-hgis");
    QString application = QStringLiteral("ka-hgis");
    QString folder;  // empty = real locations
  };
  static void setScope(const Scope& scope);
  static Scope scope();
  static void resetScope();
  // The native store for the current scope (registry by default, an INI file under
  // Scope::folder). Tests use it to seed or inspect legacy keys.
  static std::unique_ptr<QSettings> openNativeStore();
};
