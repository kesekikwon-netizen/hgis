#pragma once

#include <QPointer>
#include <QToolButton>

#include <functional>

class KaCanvasGridOverlay;
class QgsMapCanvas;

// 「원점」 button beside the grid step: picks the map point a grid line passes through, so the
// canvas grid can match a site's excavation grid. It changes only the grid overlay; nothing is
// written to the survey or the project.
class KaGridOriginButton : public QToolButton {
public:
  KaGridOriginButton(QgsMapCanvas* canvas, std::function<KaCanvasGridOverlay*()> grid, QWidget* parent = nullptr);
  // Applies an origin to the overlay and shows it in the tooltip.
  void setOrigin(double x, double y);

private:
  void editOrigin();

  QPointer<QgsMapCanvas> m_canvas;
  std::function<KaCanvasGridOverlay*()> m_grid;
};
