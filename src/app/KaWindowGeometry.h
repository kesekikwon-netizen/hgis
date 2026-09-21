#pragma once

#include <QMargins>
#include <QScreen>
#include <QWidget>

namespace KaWindowGeometry {

// Screen geometry is already in logical pixels (including Windows display scaling).
inline QRect fittedClientRect(QRect client, const QRect& available, const QMargins& frame) {
  const QRect bounds = available.marginsRemoved(frame + QMargins(4, 4, 4, 4));
  if (!bounds.isValid()) return client;
  client.setSize(client.size().boundedTo(bounds.size()));
  client.moveLeft(qBound(bounds.left(), client.left(), bounds.right() - client.width() + 1));
  client.moveTop(qBound(bounds.top(), client.top(), bounds.bottom() - client.height() + 1));
  return client;
}

inline void fit(QWidget* window) {
  if (!window || !window->isWindow() || window->isMaximized() || window->isFullScreen()) return;
  const QScreen* screen = window->screen();
  if (!screen) return;
  const QRect client = window->geometry();
  const QRect outer = window->frameGeometry();
  QMargins frame(client.left() - outer.left(), client.top() - outer.top(),
                 outer.right() - client.right(), outer.bottom() - client.bottom());
  if (frame.isNull()) frame = QMargins(8, 32, 8, 8);
  window->setGeometry(fittedClientRect(client, screen->availableGeometry(), frame));
}

// 주변유적·수치지형도·지적도 받기 창. 같은 크기, 부모 창 가운데.
inline void placeDownload(QWidget* window) {
  if (!window || !window->isWindow() || window->isMaximized() || window->isFullScreen()) return;
  window->resize(680, 330);
  QWidget* anchor = window->parentWidget();
  const QRect area = anchor && anchor->isVisible()
      ? anchor->frameGeometry()
      : (window->screen() ? window->screen()->availableGeometry() : QRect());
  if (area.isValid())
    window->move(area.center().x() - window->width() / 2, area.center().y() - window->height() / 2);
  fit(window);
}

}  // namespace KaWindowGeometry
