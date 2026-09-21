#include "core/SurveySession.h"

#include "core/KaSafeQgis.h"
#include "core/KaSessionLog.h"

#include <QDir>
#include <QFileInfo>

QString SurveySession::recoveryDirectoryFor(const QString& surveyPath) {
  if (surveyPath.isEmpty()) return {};
  return QDir(QFileInfo(surveyPath).absolutePath()).filePath(QStringLiteral("복구사본"));
}

QString SurveySession::companionQgzPathFor(const QString& surveyPath) {
  if (surveyPath.isEmpty()) return {};
  const QFileInfo file(surveyPath);
  return file.dir().filePath(file.completeBaseName() + QStringLiteral(".qgz"));
}

SurveySession::PersistResult SurveySession::persistWork(QgsProject* project,
                                                        const PersistInput& input) {
  PersistResult out;
  if (!project || input.surveyPath.isEmpty()) {
    out.workspace.error = QStringLiteral("조사 경로가 없습니다.");
    return out;
  }
  const QString surveyPath = SurveyStorage::writableSurveyPath(input.surveyPath, input.fallbackDirectory);
  out.surveyPath = surveyPath;
  const QString recovery = input.recoveryDirectory.isEmpty()
      ? recoveryDirectoryFor(surveyPath)
      : input.recoveryDirectory;
  try {
    out.workspace = SurveyStorage::persistWorkspace(project, surveyPath, recovery,
                                                    input.fallbackDirectory);
    if (!out.workspace.surveyPath.isEmpty())
      out.surveyPath = out.workspace.surveyPath;
    if (!out.workspace.saved) return out;
    kaClearQgisProjectUnsafeMark(out.surveyPath);
    if (input.writeCompanionQgz) {
      out.companionQgzPath = companionQgzPathFor(out.surveyPath);
      out.companionSaved = kaWriteQgisProjectAtomic(project, out.companionQgzPath,
                                                    &out.companionError);
      if (out.companionSaved)
        kaClearQgisProjectUnsafeMark(out.companionQgzPath);
    }
    out.saved = true;
  } catch (...) {
    KaSessionLog::line(QStringLiteral("[except] core/SurveySession.cpp:41"));
    out.saved = false;
    out.companionSaved = false;
    if (out.workspace.error.isEmpty())
      out.workspace.error = QStringLiteral("저장 중 예외가 발생했습니다. 원본은 덮어쓰지 않았습니다.");
  }
  return out;
}
