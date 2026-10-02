#pragma once
// Pixel measure of a ribbon chip's label (everything under the icon tile), shared by the theme test and
// the main-window test: how much ink the label has and between which columns it sits.
#include <QColor>
#include <QImage>
#include <QToolButton>
#include <QWidget>

#include <algorithm>

namespace RibbonLabelInk {

struct Measure {
  int pixels = 0;  // dark label pixels
  int left = -1;   // first and last ink column in logical pixels, -1 when there is no ink
  int right = -1;
};

// The tile sits 4 px below the chip's top (border, padding and the icon box's own 2 px), so the label
// starts under tile + 8 px whatever size the ribbon drew the chip at.
inline int labelTop(const QWidget* chip) {
  const auto* button = qobject_cast<const QToolButton*>(chip);
  return (button ? button->iconSize().height() : 32) + 8;
}

inline Measure measure(QWidget* chip) {
  const QImage image = chip->grab().toImage();
  const qreal dpr = image.devicePixelRatio();
  Measure out;
  for (int y = qRound(labelTop(chip) * dpr); y < image.height(); ++y) {
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
