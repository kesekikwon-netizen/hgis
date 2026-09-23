#pragma once

#include <QFrame>
#include <QPointer>
#include <functional>

class QgsMapCanvas;

// Zoom buttons floating in the top-right corner of the map. 「맞춤」 runs a
// caller-supplied fit (the survey area), because the full project extent
// includes nation-wide reference maps.
class KaMapControls final : public QFrame {
  Q_OBJECT
public:
  KaMapControls(QgsMapCanvas* canvas, QWidget* host);
  void setFitHandler(std::function<void()> fit) { m_fit = std::move(fit); }

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  void reposition();

  QPointer<QgsMapCanvas> m_canvas;
  QWidget* m_host = nullptr;
  std::function<void()> m_fit;
};

// Scale bar in the bottom-left corner of the map: a round length (1, 2 or 5
// times a power of ten) about 120 px long. Hidden for geographic CRSs, where a
// pixel is not a fixed number of metres.
class KaMapScaleBar final : public QWidget {
  Q_OBJECT
public:
  KaMapScaleBar(QgsMapCanvas* canvas, QWidget* host);
  double lengthMetres() const { return m_metres; }

protected:
  void paintEvent(QPaintEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  void refresh();
  void reposition();

  QPointer<QgsMapCanvas> m_canvas;
  QWidget* m_host = nullptr;
  double m_metres = 0.0;
  double m_pixels = 0.0;
};
