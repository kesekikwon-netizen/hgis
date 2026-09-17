#pragma once

#include <QElapsedTimer>
#include <QWidget>

class QLabel;
class QProgressBar;
class QTimer;

// The reading interval starts after synchronous initialization is complete, so
// the ten-second gauge is animated by the normal event loop, never a busy wait.
class KaStartupSplash final : public QWidget {
  Q_OBJECT
public:
  static constexpr int ReadingDurationMs = 10000;
  explicit KaStartupSplash(QWidget* parent = nullptr,
                           int readingDurationMs = ReadingDurationMs);
  static QString attributionText();
  void markReady();

signals:
  void readyToShow();

protected:
  void paintEvent(QPaintEvent*) override;

private:
  void updateProgress();
  int m_readingDurationMs;
  QElapsedTimer m_readingClock;
  QTimer* m_timer = nullptr;
  QProgressBar* m_progress = nullptr;
  QLabel* m_status = nullptr;
  QLabel* m_seconds = nullptr;
  bool m_completed = false;
};
