#pragma once
#include <QString>
#include <QStringList>

class TopographicSettings {
public:
  struct Credentials { QString username; QString password; };
  static Credentials credentials();
  static bool saveCredentials(const Credentials& credentials, QString* error = nullptr);
  // ID and password are both set. Read-only: never rewrites the account file.
  static bool hasCredentials();

private:
  friend class TopographicSettingsTest;
  static Credentials readFromFiles(const QString& personalFile, const QStringList& fallbackFiles,
                                   bool migrate = true);
  static bool saveToFile(const QString& personalFile, const Credentials& credentials, QString* error);
};
