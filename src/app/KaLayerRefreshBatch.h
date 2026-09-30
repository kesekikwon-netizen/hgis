#pragma once

#include <QPointer>
#include <functional>

class QObject;
class QTimer;

// Runs one callback on the next event-loop turn however many times it was
// requested before then (evaluation F131). Many layers added or removed in a
// row (수치지형도 도엽, 주변유적 묶음) then cost one canvas sync instead of one
// per signal. Order and visibility rules still run where the caller runs them;
// only the heavy refresh is merged.
class KaLayerRefreshBatch final {
public:
  KaLayerRefreshBatch(QObject* owner, std::function<void()> run);
  ~KaLayerRefreshBatch();
  KaLayerRefreshBatch(const KaLayerRefreshBatch&) = delete;
  KaLayerRefreshBatch& operator=(const KaLayerRefreshBatch&) = delete;

  // Schedules the callback for the next turn unless it is already scheduled.
  void request();
  bool pending() const;
  // Requests merged into the last run (diagnostics and tests).
  int lastBatchSize() const { return m_lastBatch; }

private:
  QPointer<QTimer> m_timer;
  std::function<void()> m_run;
  int m_requests = 0;
  int m_lastBatch = 0;
};
