#pragma once
#include <QString>
#include <QStringList>
#include <QVector>

class QSettings;

class RecentSurveys {
public:
  struct Item {
    QString name;
    QString path;
    qint64 lastOpenedMs = 0;
  };

  static constexpr int kMaxItems = 12;

  static QSettings userSettings();
  static QVector<Item> load(QSettings& settings);
  static QString lastPath(QSettings& settings);
  static void remember(QSettings& settings, const QString& path, const QString& name);
  static void forget(QSettings& settings, const QString& path);
  static void setSkipAutoRestore(QSettings& settings, bool skip);
  // USB 는 꽂을 때마다 드라이브 글자가 바뀐다(E: → F:). path 가 없으면 driveRoots 의 다른
  // 드라이브에서 같은 경로를 찾는다. 못 찾으면 빈 문자열.
  static QString onAnotherDrive(const QString& path, const QStringList& driveRoots);
  static bool takeSkipAutoRestore(QSettings& settings);
};
