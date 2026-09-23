#pragma once

#include <QPixmap>
#include <QWidget>

// Navy band at the top of the home page. It carries the contour texture of the
// startup notice, so the home screen continues the notice; the name, the start
// actions and the continue card are laid out on top of it as child widgets.
class KaStartHero final : public QWidget {
public:
  explicit KaStartHero(QWidget* parent = nullptr);

protected:
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

private:
  QPixmap m_texture;  // contour lines at the current size, rebuilt after a resize
};
