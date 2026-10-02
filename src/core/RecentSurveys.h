#pragma once
#include <QString>
#include <QStringList>
#include <QVector>

class QSettings;

// 최근 조사 목록. 저장된 목록은 원본 그대로 둔다: USB·네트워크 드라이브가 빠져 있어 지금
// 없는 조사도 목록에서 지우지 않는다(remember/forget 이 그 항목을 떨어뜨리지 않는다).
// 없는 항목은 표시할 때만 available=false 로 알려 회색 「찾을 수 없음」으로 보이게 한다.
// 항목을 자동으로 열거나 복원하지 않는다.
class RecentSurveys {
public:
  struct Item {
    QString name;
    QString path;
    qint64 lastOpenedMs = 0;
    bool available = true;  // 지금 열 수 있는가(파일 존재). 표시 전용, 저장하지 않는다.
  };

  static constexpr int kMaxItems = 12;

  static QSettings userSettings();
  // 지금 열 수 있는 항목만(새것부터). 이어서 열기·기본 폴더 계산용.
  static QVector<Item> load(QSettings& settings);
  // 저장된 모든 항목(새것부터). 없는 조사는 available=false. 홈 화면 목록 표시용.
  static QVector<Item> loadAll(QSettings& settings);
  static QString lastPath(QSettings& settings);
  static void remember(QSettings& settings, const QString& path, const QString& name);
  static void forget(QSettings& settings, const QString& path);
  static void setSkipAutoRestore(QSettings& settings, bool skip);
  // USB 는 꽂을 때마다 드라이브 글자가 바뀐다(E: → F:). path 가 없으면 driveRoots 의 다른
  // 드라이브에서 같은 경로를 찾는다. 못 찾으면 빈 문자열.
  static QString onAnotherDrive(const QString& path, const QStringList& driveRoots);
  static bool takeSkipAutoRestore(QSettings& settings);
  // Windows 에서는 대소문자·구분자만 다른 경로를 같은 조사로 본다(c:\a 와 C:/a).
  static bool samePath(const QString& a, const QString& b);
  // UNC 경로이거나 연결된 네트워크 드라이브(Z:)의 경로인가. 표시 판단과 같은 기준이며
  // 파일 시스템을 읽지 않는다. 홈 화면은 이런 조사의 개수를 세러 파일을 열지 않는다.
  static bool isRemotePath(const QString& path);
};
