#pragma once
#include <QString>

class VworldSettings {
public:
  static QString loadApiKey();
  static void saveApiKey(const QString& key);
  // 국사편찬위원회 역사지리정보DB (hgis.history.go.kr). VWorld 키와 별도.
  static QString loadHistoryGisApiKey();
  static void saveHistoryGisApiKey(const QString& key);
};
