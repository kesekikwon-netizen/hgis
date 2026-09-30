#pragma once

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

// 크래시·강제 종료 뒤 조사 폴더에 남는 앱 임시 자료(.ka-*)와 오래된 crash 덤프 정리.
//
// 정리는 언제나 목록을 보여 주고 사용자가 고른 뒤에만 한다(조사 폴더 쪽). 다음은 절대
// 목록에 넣지 않는다.
// - 복구사본(조사복구_*) 폴더와 그 안의 파일
// - 저장 실패 때 일부러 남긴 세대 폴더(보존 표시 파일이 있는 .ka-survey-gen-*)
// - 앱 세션 임시 폴더(ka-hgis-XXXXXX): 저장된 작업공간이 가리킬 수 있어 지우지 않는다(사용자 결정)
// - 지금 열린 프로젝트가 가리키는 파일이 들어 있는 항목(protectPaths)
// - 방금 만들어진 항목(다른 저장이 아직 쓰는 중일 수 있다)
namespace SurveyFileHygiene {

struct StaleItem {
  QString path;      // 절대 경로
  QString kind;      // 사용자에게 보일 짧은 종류 이름
  bool isDir = false;
  qint64 bytes = 0;
  QDateTime modified;
};

// 저장 실패로 남기는 세대 폴더에 적는 표시 파일 이름.
QString preservedMarkerName();

// 세대 폴더에 "저장 실패로 보존 — 지우지 않는다" 표시를 남긴다.
bool markPreservedGeneration(const QString& generationDir, const QString& reason);
bool isPreservedGeneration(const QString& generationDir);

// surveyPath 가 있는 폴더(하위 폴더는 보지 않음)의 오래된 앱 임시 자료 목록. 가장 오래된 것부터.
// minAgeSecs 보다 최근에 바뀐 항목은 넣지 않는다.
QList<StaleItem> staleStagingNear(const QString& surveyPath, const QStringList& protectPaths = {},
                                  qint64 minAgeSecs = 3600);

// 사용자가 확인한 항목만 지운다. 지우기 직전에 규칙을 다시 확인하고(minAgeSecs 포함), 맞지
// 않게 된 항목은 건너뛴다. 지운 개수를 돌려주고, 지우지 못한 경로는 failed 에 넣는다.
int removeStaleStaging(const QList<StaleItem>& confirmed, const QStringList& protectPaths = {},
                       QStringList* failed = nullptr, qint64 minAgeSecs = 60);

}  // namespace SurveyFileHygiene
