#include "core/SurveySession.h"
#include "KaLogExcept.h"

#include "core/KaSafeQgis.h"
#include "core/KaSessionLog.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace {

void noteCompanionState(const QString& surveyPath, bool upToDate, const QString& error) {
  const QString marker = SurveySession::companionStaleMarkerPath(surveyPath);
  if (marker.isEmpty()) return;
  if (upToDate) {
    QFile::remove(marker);
    return;
  }
  if (!QDir().mkpath(QFileInfo(marker).absolutePath())) return;
  QSaveFile out(marker);
  const QByteArray text = (QDateTime::currentDateTime().toString(Qt::ISODate) + QLatin1Char('\n') +
                           error + QLatin1Char('\n'))
                              .toUtf8();
  if (out.open(QIODevice::WriteOnly) && out.write(text) == text.size()) out.commit();
}

}  // namespace

QString SurveySession::recoveryDirectoryFor(const QString& surveyPath) {
  if (surveyPath.isEmpty()) return {};
  return QDir(QFileInfo(surveyPath).absolutePath()).filePath(QStringLiteral("복구사본"));
}

QString SurveySession::companionQgzPathFor(const QString& surveyPath) {
  if (surveyPath.isEmpty()) return {};
  const QFileInfo file(surveyPath);
  return file.dir().filePath(file.completeBaseName() + QStringLiteral(".qgz"));
}

QString SurveySession::companionStaleMarkerPath(const QString& surveyPath) {
  if (surveyPath.isEmpty()) return {};
  return QDir(recoveryDirectoryFor(surveyPath))
      .filePath(QFileInfo(surveyPath).completeBaseName() + QStringLiteral(".qgz-오래됨.txt"));
}

bool SurveySession::companionMayBeStale(const QString& surveyPath) {
  const QString marker = companionStaleMarkerPath(surveyPath);
  return !marker.isEmpty() && QFileInfo::exists(marker);
}

QString SurveySession::originalStateNote(const SurveyStorage::PersistAttempt& attempt) {
  if (attempt.originalCommittedLayers.isEmpty())
    return QStringLiteral("원본 조사 파일은 덮어쓰지 않았습니다.");
  return QStringLiteral("원본 조사 파일에는 저장할 수 있던 %1의 편집만 먼저 반영했고, 나머지는 그대로입니다.")
      .arg(attempt.originalCommittedLayers.join(QStringLiteral(", ")));
}

QString SurveySession::embeddedFallbackNotice(const QString& surveyPath, const QString& companionPath) {
  QString text = QStringLiteral(
      "조사 파일 안의 작업공간을 읽지 못해 동반 작업공간(%1)으로 열었습니다. 원래 작업공간은 같은 "
      "이름으로 덮어쓰지 않도록 막아 두었습니다. 「저장」을 누르면 다른 이름의 조사 파일로 저장합니다.")
                     .arg(QFileInfo(companionPath).fileName());
  if (companionMayBeStale(surveyPath))
    text += QStringLiteral(" 이 작업공간은 마지막 저장 때 갱신하지 못해 그보다 오래된 상태일 수 있습니다.");
  return text;
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
      // The embedded workspace is newer than a companion that could not be written. Remember
      // that, so a later fallback to the .qgz can say it may be older than the last save.
      noteCompanionState(out.surveyPath, out.companionSaved, out.companionError);
    }
    out.saved = true;
  } catch (...) {
    KA_LOG_EXCEPT();
    out.saved = false;
    out.companionSaved = false;
    if (out.workspace.error.isEmpty())
      out.workspace.error = QStringLiteral("저장 중 예외가 발생했습니다. 원본은 덮어쓰지 않았습니다.");
  }
  return out;
}
