#include "KaLayerRefreshBatch.h"

#include <QObject>
#include <QTimer>

KaLayerRefreshBatch::KaLayerRefreshBatch(QObject* owner, std::function<void()> run)
    : m_timer(new QTimer(owner)), m_run(std::move(run)) {
  m_timer->setSingleShot(true);
  m_timer->setInterval(0);
  QObject::connect(m_timer, &QTimer::timeout, m_timer, [this]() {
    m_lastBatch = m_requests;
    m_requests = 0;
    if (m_run) m_run();
  });
}

KaLayerRefreshBatch::~KaLayerRefreshBatch() {
  // The timer belongs to the owner; stop it so no callback reaches a dead batch.
  if (m_timer) {
    m_timer->stop();
    QObject::disconnect(m_timer, nullptr, m_timer, nullptr);
  }
}

void KaLayerRefreshBatch::request() {
  if (!m_timer) return;
  ++m_requests;
  if (!m_timer->isActive()) m_timer->start();
}

bool KaLayerRefreshBatch::pending() const {
  return m_timer && m_timer->isActive();
}
