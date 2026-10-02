#pragma once
#include <QString>
#include <QStringList>

// 국가유산 GIS통합인트라넷(intranet.gis-heritage.go.kr) 로그인 계정.
// 수치지형도(국토정보플랫폼) 계정과는 다른 자리다. TopographicSettings 와 섞지 않는다.
//
// 값은 화면·로그·문서에 찍지 않는다. 여기 담긴 자료는 동국문화재연구원의 재산이고
// 배포 경로(제출 SHP·포터블 기본 패키지)에 실리지 않는다.
class HeritageIntranetSettings {
public:
  struct Credentials { QString username; QString password; };

  static Credentials credentials();
  static bool saveCredentials(const Credentials& credentials, QString* error = nullptr);
  // 아이디·비밀번호가 모두 있는가. 값 자체를 꺼내지 않고 확인만 할 때 쓴다.
  // 읽기 전용이다: 계정 파일을 고쳐 쓰지 않는다.
  static bool hasCredentials();
  // 로그·화면에 쓸 수 있는 표기. 비밀번호는 절대 포함하지 않는다.
  static QString describeForLog();

private:
  friend class HeritageIntranetSettingsTest;
  friend class HeritageIntranetFlowTest;
  static Credentials readFromFiles(const QString& personalFile, const QStringList& fallbackFiles,
                                   bool migrate = true);
  static bool saveToFile(const QString& personalFile, const Credentials& credentials, QString* error);
};
