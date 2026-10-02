#include "KaSplashArt.h"

#include "KaSplashPalette.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPaintDevice>
#include <QPen>
#include <QPixmap>
#include <QRadialGradient>

#include <cmath>
#include <vector>

using namespace KaSplashPalette;

namespace {

constexpr double kPi = 3.14159265358979323846;

struct Hill {
  double cx, cy, sx, sy, angle, height;
};

// Smooth terrain in card-width units around the summit top: the mound itself, a
// shoulder toward the top edge, a knoll beyond the top right and a spur leaving
// the right edge. A gentle warp keeps the lines from reading as ellipses.
double terrain(double x, double y, const QPointF& top) {
  const double wx = x + 0.016 * std::sin(9.0 * y + 1.3) + 0.008 * std::sin(19.0 * y + 0.4);
  const double wy = y + 0.013 * std::sin(8.0 * x + 0.7) + 0.007 * std::sin(17.0 * x + 2.2);
  const Hill hills[] = {
      {top.x(), top.y(), 0.15, 0.11, 0.35, 1.0},
      {top.x() - 0.20, top.y() - 0.15, 0.10, 0.07, -0.5, 0.42},
      {top.x() + 0.18, top.y() - 0.22, 0.11, 0.08, 0.0, 0.50},
      {top.x() + 0.26, top.y() + 0.22, 0.17, 0.13, 0.2, 0.70},
  };
  double v = 0.0;
  for (const Hill& h : hills) {
    const double c = std::cos(h.angle), s = std::sin(h.angle);
    const double dx = wx - h.cx, dy = wy - h.cy;
    const double u = (c * dx + s * dy) / h.sx, t = (-s * dx + c * dy) / h.sy;
    v += h.height * std::exp(-0.5 * (u * u + t * t));
  }
  return v;
}

QPointF crossing(const QPointF& p1, double v1, const QPointF& p2, double v2, double level) {
  const double t = std::abs(v2 - v1) < 1e-12 ? 0.5 : (level - v1) / (v2 - v1);
  return p1 + (p2 - p1) * t;
}

void addSegment(QPainterPath& path, const QPointF& a, const QPointF& b) {
  path.moveTo(a);
  path.lineTo(b);
}

// One contour level by marching squares; corners a b / d c, set bit = above.
QPainterPath isoline(const std::vector<double>& field, int nx, int ny, double step, double level) {
  QPainterPath path;
  auto at = [&](int i, int j) { return field[size_t(j) * size_t(nx) + size_t(i)]; };
  for (int j = 0; j + 1 < ny; ++j)
    for (int i = 0; i + 1 < nx; ++i) {
      const double a = at(i, j), b = at(i + 1, j), c = at(i + 1, j + 1), d = at(i, j + 1);
      const int code = (a > level) << 3 | (b > level) << 2 | (c > level) << 1 | int(d > level);
      if (code == 0 || code == 15) continue;
      const QPointF pa(i * step, j * step), pb(pa.x() + step, pa.y()),
          pc(pa.x() + step, pa.y() + step), pd(pa.x(), pa.y() + step);
      const QPointF top = crossing(pa, a, pb, b, level), right = crossing(pb, b, pc, c, level),
                    bottom = crossing(pd, d, pc, c, level), left = crossing(pa, a, pd, d, level);
      const bool centreHigh = (a + b + c + d) / 4.0 > level;
      switch (code) {
        case 1: case 14: addSegment(path, left, bottom); break;
        case 2: case 13: addSegment(path, bottom, right); break;
        case 3: case 12: addSegment(path, left, right); break;
        case 4: case 11: addSegment(path, top, right); break;
        case 6: case 9: addSegment(path, top, bottom); break;
        case 7: case 8: addSegment(path, top, left); break;
        case 5:  // saddle, b and d high
          addSegment(path, top, centreHigh ? left : right);
          addSegment(path, bottom, centreHigh ? right : left);
          break;
        case 10:  // saddle, a and c high
          addSegment(path, top, centreHigh ? right : left);
          addSegment(path, bottom, centreHigh ? left : right);
          break;
      }
    }
  return path;
}

// The part of the icon file that is not transparent.
QImage visibleTile(const QPixmap& pixmap) {
  const QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
  int left = image.width(), top = image.height(), right = -1, bottom = -1;
  for (int y = 0; y < image.height(); ++y) {
    const QRgb* line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
    for (int x = 0; x < image.width(); ++x) {
      if (qAlpha(line[x]) <= 8) continue;
      left = std::min(left, x);
      right = std::max(right, x);
      top = std::min(top, y);
      bottom = std::max(bottom, y);
    }
  }
  return right < left ? image : image.copy(QRect(QPoint(left, top), QPoint(right, bottom)));
}

}  // namespace

namespace KaSplashArt {

void paintShadow(QPainter& painter, const QRectF& card, double radius) {
  painter.save();
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(4, 18, 34, 6));
  // Stacked, slightly lowered rounded rects read as one soft shadow.
  for (int i = 12; i >= 1; --i) {
    QPainterPath layer;
    layer.addRoundedRect(card.adjusted(-i, -0.5 * i + 5, i, i + 5), radius + i, radius + i);
    painter.drawPath(layer);
  }
  painter.restore();
}

void paintCard(QPainter& painter, const QRectF& card, double radius, const QImage& texture) {
  QPainterPath path;
  path.addRoundedRect(card, radius, radius);
  QLinearGradient blue(card.topLeft(), card.bottomRight());
  blue.setColorAt(0.0, kTop);
  blue.setColorAt(0.42, kMid);
  blue.setColorAt(1.0, kDeep);
  painter.fillPath(path, blue);

  painter.save();
  painter.setClipPath(path);
  if (!texture.isNull()) {
    // Contour strokes fade under the text band, so the small licence lines
    // never sit on a line (the texture already fades toward the lower left).
    QImage faded = texture.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QPainter mask(&faded);
    mask.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    QLinearGradient keep(QPointF(0.0, card.height() * 0.62), QPointF(0.0, card.height() * 0.80));
    keep.setColorAt(0.0, QColor(0, 0, 0, 255));
    keep.setColorAt(1.0, QColor(0, 0, 0, 60));
    mask.fillRect(QRectF(QPointF(0.0, 0.0), card.size()), keep);
    mask.end();
    painter.drawImage(card.topLeft(), faded);
  }
  QLinearGradient sheen(card.topLeft(), QPointF(card.left(), card.top() + card.height() * 0.45));
  sheen.setColorAt(0.0, QColor(255, 255, 255, 34));
  sheen.setColorAt(1.0, QColor(255, 255, 255, 0));
  painter.fillRect(card, sheen);
  // The title, status and notices sit in the lower part: deepen it a little so
  // the small licence lines stay readable over the lighter left-hand blue.
  QLinearGradient foot(QPointF(card.left(), card.top() + card.height() * 0.55), card.bottomLeft());
  foot.setColorAt(0.0, withAlpha(kDeep, 0.0));
  foot.setColorAt(1.0, withAlpha(kDeep, 0.30));
  painter.fillRect(card, foot);
  painter.restore();

  painter.save();
  painter.setBrush(Qt::NoBrush);
  painter.setPen(QPen(QColor(255, 255, 255, 56), 1.0));
  painter.drawRoundedRect(card.adjusted(0.5, 0.5, -0.5, -0.5), radius - 0.5, radius - 0.5);
  painter.restore();
}

QImage contours(const QSizeF& size, qreal devicePixelRatio, const QPointF& summit, double unit,
                double clearRadius) {
  const qreal dpr = devicePixelRatio > 0 ? devicePixelRatio : 1.0;
  QImage image((size * dpr).toSize(), QImage::Format_ARGB32_Premultiplied);
  image.setDevicePixelRatio(dpr);
  image.fill(Qt::transparent);
  const double w = size.width(), h = size.height();
  if (w < 8 || h < 8) return image;

  const double step = 4.0;
  const int nx = int(std::ceil(w / step)) + 1, ny = int(std::ceil(h / step)) + 1;
  const double ref = unit > 0.0 ? unit : w;
  const QPointF top(summit.x() / ref, summit.y() / ref);
  std::vector<double> field(size_t(nx) * size_t(ny));
  double peak = 0.0;
  for (int j = 0; j < ny; ++j)
    for (int i = 0; i < nx; ++i) {
      const double v = terrain(i * step / ref, j * step / ref, top);
      field[size_t(j) * size_t(nx) + size_t(i)] = v;
      peak = std::max(peak, v);
    }

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing);
  const double first = 0.12, gap = 0.068;
  for (int k = 0; first + k * gap < peak; ++k) {
    const bool index = k % 5 == 4;
    painter.strokePath(isoline(field, nx, ny, step, first + k * gap),
                       QPen(QColor(255, 255, 255, index ? 50 : 24), index ? 1.2 : 0.9,
                            Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  }

  painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
  const double clearing = clearRadius < 0.0 ? 0.11 * w : clearRadius;
  if (clearing > 0.0) {
    QRadialGradient clear(summit, clearing);
    clear.setColorAt(0.0, QColor(0, 0, 0, 0));
    clear.setColorAt(0.55, QColor(0, 0, 0, 0));
    clear.setColorAt(1.0, QColor(0, 0, 0, 255));
    painter.fillRect(QRectF(0, 0, w, h), clear);
  }
  QLinearGradient away(QPointF(0.06 * w, 0.98 * h), QPointF(0.50 * w, 0.38 * h));
  away.setColorAt(0.0, QColor(0, 0, 0, 0));
  away.setColorAt(1.0, QColor(0, 0, 0, 255));
  painter.fillRect(QRectF(0, 0, w, h), away);
  painter.end();
  return image;
}

void paintIcon(QPainter& painter, const QRectF& box, const QPixmap& icon) {
  if (icon.isNull()) return;
  const QImage tile = visibleTile(icon);
  const QSizeF fitted = QSizeF(tile.size()).scaled(box.size(), Qt::KeepAspectRatio);
  const QRectF target(box.center() - QPointF(fitted.width() / 2, fitted.height() / 2), fitted);
  const double radius = target.width() * 0.22;
  painter.save();
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(3, 16, 32, 10));
  // Stacked rounded rects under the tile read as one soft shadow below it.
  for (int i = 1; i <= 5; ++i)
    painter.drawRoundedRect(target.adjusted(-i * 0.5, i * 0.7 + 1, i * 0.5, i * 1.0 + 2),
                            radius + i, radius + i);
  // Scaled once per cached layer, so the smooth filter costs nothing per frame.
  const qreal dpr = painter.device() ? painter.device()->devicePixelRatioF() : 1.0;
  QImage scaled = tile.scaled((fitted * dpr).toSize(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
  scaled.setDevicePixelRatio(dpr);
  painter.drawImage(target.topLeft(), scaled);
  painter.restore();
}

void paintFlowDots(QPainter& painter, const QRectF& lane, double time, bool still) {
  constexpr int kDots = 5;
  const double r = std::max(1.6, lane.height() * 0.22);
  const double y = lane.center().y();
  painter.save();
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(Qt::NoPen);
  if (still) {
    painter.setBrush(withAlpha(kSky, 0.55));
    const double spacing = r * 4.2;
    for (int i = 0; i < kDots; ++i) painter.drawEllipse(QPointF(lane.left() + r + i * spacing, y), r, r);
    painter.restore();
    return;
  }
  // Each dot crosses the lane in turn: fast at the edges, bunching in the middle.
  const double cycle = 2.6, travel = 0.62, stagger = 0.075;
  const double phase = std::fmod(std::max(0.0, time), cycle) / cycle;
  for (int i = 0; i < kDots; ++i) {
    const double t = (phase - i * stagger) / travel;
    if (t <= 0.0 || t >= 1.0) continue;
    const double f = t + 0.82 * std::sin(2.0 * kPi * t) / (2.0 * kPi);
    painter.setBrush(withAlpha(kSky, 0.95 * clamp01(std::min(t, 1.0 - t) / 0.12)));
    painter.drawEllipse(QPointF(lane.left() + f * lane.width(), y), r, r);
  }
  painter.restore();
}

}  // namespace KaSplashArt
