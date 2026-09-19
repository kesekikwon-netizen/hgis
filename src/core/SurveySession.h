#pragma once

#include "core/SurveyStorage.h"

#include <QString>

class QgsProject;

// 조사 열기/저장의 파일 쪽. UI(알림·창 제목)는 MainWindow 에 남긴다.
namespace SurveySession {

struct PersistInput {
  QString surveyPath;
  QString recoveryDirectory;
  bool writeCompanionQgz = true;
};

struct PersistResult {
  bool saved = false;
  bool companionSaved = true;
  QString companionQgzPath;
  QString companionError;
  SurveyStorage::PersistAttempt workspace;
};

QString recoveryDirectoryFor(const QString& surveyPath);
QString companionQgzPathFor(const QString& surveyPath);

// persistWorkspace 로 다음 세대 GPKG 를 검증·교체한 뒤, 같은 이름의 동반 QGZ 를
// 원자 기록한다. MainWindow 없이 단위 시험할 수 있다.
PersistResult persistWork(QgsProject* project, const PersistInput& input);

}  // namespace SurveySession
