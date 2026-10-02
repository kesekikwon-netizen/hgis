#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QPixmap>
#include <QRectF>
#include <QWidget>

class QTimer;

// Startup notice shown before the main window. A blue card with the app icon on
// a contoured mound; every line of text sits in the lower left: the Strata
// title, the status with progress dots flowing left to right, the copyright and
// the data sources. Only the dots move. Clicking never closes the notice early.
class KaStartupSplash final : public QWidget {
  Q_OBJECT
public:
  static constexpr int ReadingDurationMs = 5000;
  explicit KaStartupSplash(QWidget* parent = nullptr,
                           int readingDurationMs = ReadingDurationMs);
  ~KaStartupSplash() override;
  static QString attributionText();
  // Full credit text (name, copyright, data sources and licences), for
  // accessibility; the notice paints a short form of it.
  static QString creditsText();
  void markReady();
  // 0..1000, the share of the reading interval that has passed.
  int readingProgress() const { return m_progress; }
  // The blue card inside the transparent shadow margin.
  QRectF cardRect() const;
  // The lane the progress dots cross.
  QRectF dotsRect() const;
  // Where the copyright and data lines are painted.
  QRectF noticesRect() const;
  // The card without any text (shadow, blue, contours, icon), for contrast QA.
  QImage backdropImage() const;

signals:
  void readyToShow();

protected:
  void paintEvent(QPaintEvent*) override;

private:
  void tick();
  void placeWindow();
  QString statusText() const;
  const QPixmap& staticLayer();
  void paintBackdrop(QPainter& painter, qreal dpr) const;

  int m_readingDurationMs;
  QElapsedTimer m_readingClock;
  QTimer* m_timer = nullptr;
  QPixmap m_icon;
  QPixmap m_static;
  int m_progress = 0;
  bool m_completed = false;
  bool m_reducedMotion = false;
};
