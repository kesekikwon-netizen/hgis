#pragma once

#include <QElapsedTimer>
#include <QPixmap>
#include <QRectF>
#include <QWidget>

#include <memory>

class KaSplashScene;
class QTimer;

// Startup notice shown before the main window. Names, copyright and data sources
// are painted from the first frame as a drawing title block and stay for the whole
// reading interval. The reading interval starts after synchronous initialization
// is complete, so the ten-second gauge and the motion run on the normal event
// loop, never a busy wait. The plan on the left is interactive: the mouse works
// as a trowel that scrapes the soil off the measured drawing. Clicking never
// closes the notice early.
class KaStartupSplash final : public QWidget {
  Q_OBJECT
public:
  static constexpr int ReadingDurationMs = 10000;
  explicit KaStartupSplash(QWidget* parent = nullptr,
                           int readingDurationMs = ReadingDurationMs);
  ~KaStartupSplash() override;
  static QString attributionText();
  // Names, copyright and data sources exactly as the title block shows them.
  static QString creditsText();
  void markReady();
  // 0..1000, the share of the reading interval that has passed.
  int readingProgress() const { return m_progress; }
  QRectF planRect() const;
  double revealedFraction() const;

signals:
  void readyToShow();

protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void leaveEvent(QEvent*) override;

private:
  void tick();
  void layoutScene();
  void trackPointer(const QPointF& pos, bool pressed);
  double phase() const;
  QString statusText() const;
  QString secondsText() const;

  int m_readingDurationMs;
  QElapsedTimer m_readingClock;
  QElapsedTimer m_frameClock;
  QTimer* m_timer = nullptr;
  std::unique_ptr<KaSplashScene> m_scene;
  QPixmap m_icon;
  int m_progress = 0;
  bool m_completed = false;
  bool m_reducedMotion = false;
};
