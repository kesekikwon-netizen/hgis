#pragma once

#include "core/SurveyStorage.h"

#include <QString>

class QgsProject;

// 조사 열기/저장의 파일 쪽. UI(알림·창 제목)는 MainWindow 에 남긴다.
namespace SurveySession {

struct PersistInput {
  QString surveyPath;
  QString recoveryDirectory;
  QString fallbackDirectory;
  bool writeCompanionQgz = true;
};

struct PersistResult {
  bool saved = false;
  bool companionSaved = true;
  QString surveyPath;
  QString companionQgzPath;
  QString companionError;
  SurveyStorage::PersistAttempt workspace;
};

QString recoveryDirectoryFor(const QString& surveyPath);
QString companionQgzPathFor(const QString& surveyPath);

// persistWorkspace 로 다음 세대 GPKG 를 검증·교체한 뒤, 같은 이름의 동반 QGZ 를
// 원자 기록한다. MainWindow 없이 단위 시험할 수 있다. 동반 QGZ 를 쓰지 못하면
// companionStaleMarkerPath 에 표시를 남기고, 다음에 쓰면 지운다.
PersistResult persistWork(QgsProject* project, const PersistInput& input);

// 저장 실패 안내의 원본 상태 문장. 원본을 건드리지 않았으면 "덮어쓰지 않았습니다", 막힌
// 레이어 때문에 저장할 수 있는 레이어만 먼저 원본에 커밋했으면 그 레이어 이름을 적는다.
QString originalStateNote(const SurveyStorage::PersistAttempt& attempt);

// 동반 .qgz 가 마지막 저장보다 오래되었다는 표시 파일(복구사본/<이름>.qgz-오래됨.txt).
QString companionStaleMarkerPath(const QString& surveyPath);
// 마지막 저장에서 동반 .qgz 를 쓰지 못했는가(표시 파일이 있는가). 읽기만 한다.
bool companionMayBeStale(const QString& surveyPath);

// 조사 파일 안 작업공간을 읽지 못해 동반 작업공간(companionPath)으로 연 경우 알림 줄에
// 보일 문장. 같은 이름 덮어쓰기는 막아 둔 상태를 전제로 하며, 무엇도 자동 복원하지 않는다.
// 동반 .qgz 가 오래되었을 수 있으면 그 사실도 적는다.
QString embeddedFallbackNotice(const QString& surveyPath, const QString& companionPath);

}  // namespace SurveySession
