#pragma once

#include <QString>
#include <QtGlobal>

// 저장 직전에 "다른 곳(다른 PC·다른 포터블 사본)이 이 조사 파일을 저장했는가"를 알아보는 지문.
//
// 조사 파일은 잠그지 않는다. 혼자 쓰는 현장 사용자가 여러 PC·USB 를 오가는 것이 기본이고,
// 저장은 다음 세대 GPKG 로 원본을 통째로 바꾼다. 그래서 같은 파일을 두 곳에서 열어 두면
// 나중에 저장한 쪽이 먼저 저장한 쪽을 조용히 덮는다. 이 도구는 경고할 근거만 준다.
//
// 앱 안의 편집 커밋·WAL 정리는 같은 파일을 제자리에서 고치므로 크기·수정 시각이 수시로 바뀐다.
// 그것까지 "다른 곳의 변경"으로 보면 저장할 때마다 잘못 경고한다. 그래서 판단은 파일 식별자
// (볼륨 일련번호 + 파일 번호)로 한다. 저장은 새 파일로 이름을 바꿔 끼우므로 다른 곳이 저장하면
// 식별자가 바뀌고, 제자리 커밋으로는 바뀌지 않는다. 식별자를 얻지 못하면(비 Windows 등) 알 수
// 없음으로 보고 경고하지 않는다.
namespace SurveyFileFingerprint {

struct Fingerprint {
  bool exists = false;
  qint64 size = -1;
  qint64 modifiedMs = 0;
  QString fileId;  // "<볼륨 일련번호>:<파일 번호>" 16진. 얻지 못하면 비어 있다.
};

// 지금 디스크에 있는 파일의 지문. 파일이 없으면 exists=false.
Fingerprint capture(const QString& path);

// 이 프로세스가 마지막으로 열었거나 저장한 상태로 기억한다. 조사를 연 직후와 저장·교체가 끝난
// 직후에 부른다. SurveyStorage::publishSurveyGeneration 은 성공하면 스스로 부른다.
void remember(const QString& path);
void forget(const QString& path);
bool isRemembered(const QString& path);

// 기억한 뒤 다른 곳이 이 파일을 바꿔 끼웠는가(저장했는가, 지웠는가). 기억이 없거나 식별자를
// 얻을 수 없으면 false. detail 에는 사용자에게 보일 짧은 설명을 넣는다.
bool replacedElsewhere(const QString& path, QString* detail = nullptr);

}  // namespace SurveyFileFingerprint
