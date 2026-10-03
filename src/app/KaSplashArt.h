#pragma once

#include <QColor>
#include <QImage>
#include <QPointF>
#include <QSizeF>

class QPainter;
class QPixmap;
class QRectF;

// Theme artwork for the startup splash: the blue card, its soft shadow, a faint
// topographic contour texture and the flowing progress dots.
namespace KaSplashArt {

// Soft shadow, drawn in the window's transparent margin below the card.
void paintShadow(QPainter& painter, const QRectF& card, double radius);
// Gradient card with the texture inside it, a light sheen, a darker foot under
// the text lines and a hairline edge.
void paintCard(QPainter& painter, const QRectF& card, double radius, const QImage& texture);
// Contour lines of a mound whose summit sits at summit (card coordinates). The
// summit itself stays clear for the app icon, and the lines fade out toward the
// text in the lower left. Every fifth line is an index contour, drawn a little
// stronger as on a survey map.
// unit is the length the hills are sized against (the width when 0); clearRadius
// is the empty disc around the summit (11% of the width when negative, none at 0).
// ink is the line colour: white on the blue card, a dark ink on a paper ground.
QImage contours(const QSizeF& size, qreal devicePixelRatio, const QPointF& summit,
                double unit = 0.0, double clearRadius = -1.0, const QColor& ink = Qt::white);
// The app icon centred in box. The icon file has uneven transparent margins, so
// the visible tile is cropped out first; a soft shadow falls below the tile.
void paintIcon(QPainter& painter, const QRectF& box, const QPixmap& icon);
// Progress dots crossing lane from left to right, time in seconds. When still,
// the dots rest evenly spaced instead.
void paintFlowDots(QPainter& painter, const QRectF& lane, double time, bool still);

}  // namespace KaSplashArt
