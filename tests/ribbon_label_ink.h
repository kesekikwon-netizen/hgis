#pragma once
// Pixel measure of a ribbon chip's label (everything under the 32 px icon tile), shared by the theme
// test and the main-window test: how much ink the label has and between which columns it sits.
#include <QColor>
#include <QImage>
#include <QWidget>

#include <algorithm>

namespace RibbonLabelInk {

struct Measure {
  int pixels = 0;  // dark label pixels
  int left = -1;   // first and last ink column in logical pixels, -1 when there is no ink
  int right = -1;
};

inline Measure measure(QWidget* chip) {
  const QImage image = chip->grab().toImage();
  const qreal dpr = image.devicePixelRatio();
  Measure out;
  for (int y = qRound(40 * dpr); y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      const QColor c = image.pixelColor(x, y);
      if (c.alpha() < 200 || c.lightness() >= 200) continue;
      ++out.pixels;
      const int column = qRound(x / dpr);
      out.left = out.left < 0 ? column : std::min(out.left, column);
      out.right = std::max(out.right, column);
    }
  }
  return out;
}

}  // namespace RibbonLabelInk
