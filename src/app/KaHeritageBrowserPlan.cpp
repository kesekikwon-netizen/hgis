#include "KaHeritageBrowser.h"

#include "core/HeritageRecentDownloads.h"

#include <QDir>
#include <QLabel>
#include <QTimer>

// Multi-시·군 plan of the intranet window (evaluation F120, F174).
// Kept apart from KaHeritageBrowser.cpp so the page automation file does not grow.
// The request unit stays one 시/군: each ticked neighbour is its own start() run.

void KaHeritageBrowser::setFetchPlan(const HeritageFetchPlan& plan) {
  m_followUps = HeritageFetchPlanning::normalizedFollowUps({m_sido, m_city}, plan.followUps);
  m_targetIndex = 0;
  m_targetCount = 1 + static_cast<int>(m_followUps.size());
  m_reuseRecent = plan.reuseRecent;
}

QString KaHeritageBrowser::regionLabelForImport() const {
  return m_targetCount > 1 ? m_city : QString();
}

QString KaHeritageBrowser::targetPrefix() const {
  if (m_targetCount <= 1) return {};
  return QStringLiteral("%1 (%2/%3) · ").arg(m_city).arg(m_targetIndex + 1).arg(m_targetCount);
}

void KaHeritageBrowser::updateNotice() {
  if (!m_noticeLabel) return;
  QStringList parts;
  if (!m_retryNote.isEmpty()) parts << m_retryNote;
  if (!m_pledgeNote.isEmpty()) parts << m_pledgeNote;
  m_noticeLabel->setText(parts.join(QLatin1Char('\n')));
  m_noticeLabel->setVisible(!parts.isEmpty());
}

void KaHeritageBrowser::finishTarget(const QString& message) {
  m_running = false;
  m_poll->stop();
  if (!m_followUps.isEmpty()) {
    const HeritageCity next = m_followUps.takeFirst();
    ++m_targetIndex;
    logLine(QStringLiteral("=== %1 마침 · 다음 시·군 %2 (%3/%4) ===")
                .arg(m_city, next.display()).arg(m_targetIndex + 1).arg(m_targetCount));
    m_downloadRoot = HeritageFetchPlanning::siblingRoot(m_downloadRoot, next);
    QDir().mkpath(m_downloadRoot);
    m_sido = next.sido;
    m_city = next.city;
    m_datasets = m_planDatasets;
    m_datasetIndex = 0;
    m_detailLabel->setText(QStringLiteral("%1 · 이어서 %2 을(를) 받습니다.").arg(message, next.display()));
    // Start from the event loop, not from inside a page/script callback. 「취소」 clears it.
    m_pendingNext = true;
    QTimer::singleShot(0, this, [this]() {
      if (!m_pendingNext) return;
      m_pendingNext = false;
      start();
    });
    return;
  }
  setStage(HeritageStage::Done, message);
  emit allFinished();
}

bool KaHeritageBrowser::reuseRecentDatasets() {
  QVector<HeritageDataset> remaining;
  int reused = 0;
  for (HeritageDataset dataset : std::as_const(m_datasets)) {
    const QStringList files = HeritageRecentDownloads::find(m_downloadRoot, dataset);
    if (files.isEmpty()) {
      remaining.append(dataset);
      continue;
    }
    // The caller loads it now. A rejection (for example a damaged copy) means: fetch anew.
    m_reuseProbe = true;
    m_reuseRejected = false;
    emit datasetReady(dataset, files);
    m_reuseProbe = false;
    if (m_reuseRejected) {
      logLine(QStringLiteral("최근 자료를 쓰지 못해 새로 받음: %1").arg(HeritageStyle::layerName(dataset)));
      remaining.append(dataset);
      continue;
    }
    ++reused;
    logLine(QStringLiteral("최근 받은 자료 다시 씀: %1").arg(HeritageStyle::layerName(dataset)));
  }
  m_datasets = remaining;
  m_datasetIndex = 0;
  if (reused > 0)
    m_detailLabel->setText(
        QStringLiteral("%1최근 받은 자료 %2종을 다시 썼습니다.").arg(targetPrefix()).arg(reused));
  if (!m_datasets.isEmpty()) return false;
  finishTarget(QStringLiteral("%1 최근 받은 자료 %2종을 다시 썼습니다.").arg(m_city).arg(reused));
  return true;
}
