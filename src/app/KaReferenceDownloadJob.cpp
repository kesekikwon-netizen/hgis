#include "KaCrashGuard.h"
#include "KaReferenceDownloadJob.h"
#include <QTimer>
#include <qgsfeedback.h>
#include <utility>

KaReferenceDownloadJob::KaReferenceDownloadJob(const QString& title, Prepare prepare,
                                               Complete complete)
    : KaReferenceDownloadJob(title, [prepare = std::move(prepare)](QgsFeedback* feedback, const std::function<bool()>&) {
        return prepare(feedback);
      }, std::move(complete)) {}

KaReferenceDownloadJob::KaReferenceDownloadJob(const QString& title, CancellablePrepare prepare,
                                               Complete complete)
    : QgsTask(title, QgsTask::CanCancel | QgsTask::CancelWithoutPrompt),
      m_prepare(std::move(prepare)), m_complete(std::move(complete)) {}

bool KaReferenceDownloadJob::run() {
  if (isCanceled()) return false;
  // QgsFeedback's cancellation flag is not atomic. Keep every read/write on
  // this worker; QgsTask::isCanceled() provides the synchronized GUI boundary.
  QgsFeedback feedback;
  connect(&feedback, &QgsFeedback::progressChanged, &feedback, [this](double progress) { setProgress(progress); });
  QTimer cancellationPoll;
  connect(&cancellationPoll, &QTimer::timeout, &feedback, [this, &feedback]() {
    if (isCanceled()) feedback.cancel();
  });
  // QgsBlockingNetworkRequest runs a nested event loop, so a stalled download
  // can observe cancellation without blocking or calling into the GUI thread.
  cancellationPoll.start(50);
  if (isCanceled()) feedback.cancel();
  try {
    m_result = m_prepare(&feedback, [this]() { return isCanceled(); });
  } catch (...) {
    KaCrashGuard::logLine(QStringLiteral("[except] app/KaReferenceDownloadJob.cpp:33"));
    // QgsException does not derive from std::exception. No SDK exception may
    // escape this worker boundary and terminate the process.
    m_result.error = QStringLiteral("지도 자료를 준비하다 오류가 발생했습니다. "
                                   "기존 지도는 유지됩니다. 저장 공간을 확인한 뒤 다시 시도하세요.");
    m_result.status = PreparedReferenceMap::Status::Failed;
  }
  return m_result.isReady() && (!isCanceled() || m_result.outputCommitted);
}

void KaReferenceDownloadJob::finished(bool success) {
  if (m_result.isReady() && m_result.outputCommitted) { /* completed file wins a late cancel */ }
  else if (isCanceled()) m_result.status = PreparedReferenceMap::Status::Cancelled;
  else if (!success) m_result.status = PreparedReferenceMap::Status::Failed;
  m_complete(m_result);
  m_prepare = {};
  m_complete = {};
}
