#include "KaSplashScene.h"

#include "KaSplashPalette.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QPen>
#include <QPixmap>
#include <QRadialGradient>
#include <QtMath>

#include <cmath>

using namespace KaSplashPalette;

namespace {

double appear(double phase, double start, double span) {
  return easeOut((phase - start) / qMax(0.04, span));
}

QPainterPath surveyArea(double w, double h) {
  QPainterPath path;
  path.moveTo(w * 0.18, h * 0.28);
  path.cubicTo(w * 0.32, h * 0.14, w * 0.58, h * 0.16, w * 0.74, h * 0.30);
  path.cubicTo(w * 0.88, h * 0.42, w * 0.86, h * 0.68, w * 0.70, h * 0.78);
  path.cubicTo(w * 0.52, h * 0.90, w * 0.28, h * 0.86, w * 0.16, h * 0.66);
  path.closeSubpath();
  return path;
}

}  // namespace

void KaSplashScene::paint(QPainter& painter, double phase, const QPixmap& icon) const {
  if (m_rect.isEmpty()) return;
  const double w = m_rect.width();
  const double h = m_rect.height();
  const double t = m_clock;
  const double p = clamp01(phase);

  painter.save();
  QPainterPath frame;
  frame.addRoundedRect(QRectF(0, 0, w, h), 10, 10);
  painter.translate(m_rect.topLeft());
  painter.setClipPath(frame);

  QLinearGradient field(QPointF(0, 0), QPointF(w, h));
  field.setColorAt(0, QColor(8, 42, 78));
  field.setColorAt(0.55, QColor(6, 28, 54));
  field.setColorAt(1, QColor(4, 16, 36));
  painter.fillRect(QRectF(0, 0, w, h), field);

  QRadialGradient glow(QPointF(w * 0.42, h * 0.46), h * 0.62);
  glow.setColorAt(0, QColor(80, 170, 230, int(70 + 28 * qSin(t * 1.4))));
  glow.setColorAt(1, QColor(80, 170, 230, 0));
  painter.fillRect(QRectF(0, 0, w, h), glow);

  const double land = appear(p, 0.02, 0.18);
  if (land > 0) {
    painter.setOpacity(0.22 * land);
    for (int band = 0; band < 4; ++band) {
      QPainterPath ridge;
      const double y0 = h * (0.38 + band * 0.12);
      ridge.moveTo(0, h);
      for (int i = 0; i <= 16; ++i) {
        const double x = w * i / 16.0;
        const double y = y0 + h * 0.04 * qSin(i * 0.7 + t * 0.6 + band);
        if (i == 0)
          ridge.lineTo(x, y);
        else
          ridge.lineTo(x, y);
      }
      ridge.lineTo(w, h);
      ridge.closeSubpath();
      painter.fillPath(ridge, QColor(18, 70, 110));
    }
    painter.setOpacity(1.0);
  }

  const double gridA = appear(p, 0.08, 0.2);
  if (gridA > 0) {
    const double drift = std::fmod(t * 8.0, 18.0);
    const double step = h / 9.0;
    for (int i = -1; i * step <= w + step; ++i) {
      const double x = i * step + drift * 0.15;
      painter.setPen(QPen(withAlpha(kLine, gridA * (i % 3 ? 0.10 : 0.22)), 1));
      painter.drawLine(QPointF(x, 0), QPointF(x, h));
    }
    for (int j = 0; j <= 10; ++j) {
      painter.setPen(QPen(withAlpha(kLine, gridA * (j % 3 ? 0.10 : 0.22)), 1));
      painter.drawLine(QPointF(0, j * step), QPointF(w, j * step));
    }
  }

  const QPainterPath area = surveyArea(w, h);
  const double areaA = appear(p, 0.16, 0.22);
  if (areaA > 0) {
    painter.setBrush(QColor(40, 140, 200, int(28 * areaA)));
    QPen edge(withAlpha(kSky, 0.35 + 0.55 * areaA), 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    edge.setDashPattern({10, 6});
    edge.setDashOffset(-t * 28.0);
    painter.setPen(edge);
    painter.drawPath(area);
  }

  const double contourA = appear(p, 0.28, 0.22);
  if (contourA > 0) {
    QPen contour(withAlpha(kInk, 0.55 * contourA), 1.2);
    painter.setPen(contour);
    painter.setBrush(Qt::NoBrush);
    for (int c = 0; c < 3; ++c) {
      const double reveal = appear(p, 0.28 + c * 0.06, 0.16);
      if (reveal <= 0) continue;
      QPainterPath line;
      const double y = h * (0.34 + c * 0.16);
      line.moveTo(w * 0.08, y);
      line.cubicTo(w * 0.28, y - h * 0.08 * reveal, w * 0.55, y + h * 0.07, w * 0.92, y - h * 0.03);
      painter.setOpacity(reveal);
      painter.drawPath(line);
    }
    painter.setOpacity(1.0);
  }

  struct Site {
    QPointF at;
    const char* name;
    double start;
  };
  const Site sites[] = {
      {QPointF(w * 0.34, h * 0.40), "1", 0.40},
      {QPointF(w * 0.62, h * 0.36), "2", 0.50},
      {QPointF(w * 0.48, h * 0.64), "3", 0.60},
  };
  const double r = h * 0.038;
  for (const Site& site : sites) {
    const double a = appear(p, site.start, 0.12);
    if (a <= 0) continue;
    const double drop = (1.0 - a) * h * 0.08;
    const QPointF c(site.at.x(), site.at.y() - drop);
    const double pulse = 1.0 + 0.35 * (0.5 + 0.5 * qSin(t * 3.2 + site.start * 8));
    painter.setPen(QPen(withAlpha(kGold, 0.35 * a), 1.2));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(c, r * pulse * 1.7, r * pulse * 1.7);
    QRadialGradient disc(c, r * 1.15);
    disc.setColorAt(0, QColor(255, 230, 150, int(230 * a)));
    disc.setColorAt(1, QColor(180, 120, 30, int(210 * a)));
    painter.setPen(QPen(withAlpha(QColor(255, 255, 255), 0.7 * a), 1.3));
    painter.setBrush(disc);
    painter.drawEllipse(c, r, r);
    painter.setPen(QColor(40, 24, 8, int(230 * a)));
    painter.setFont(uiFont(r * 1.15, true));
    painter.drawText(QRectF(c.x() - r, c.y() - r, r * 2, r * 2), Qt::AlignCenter,
                     QString::fromLatin1(site.name));
  }

  const double northA = appear(p, 0.68, 0.14);
  if (northA > 0) {
    painter.save();
    painter.setOpacity(northA);
    painter.translate(w * 0.88, h * 0.16);
    painter.rotate(-70.0 * (1.0 - northA));
    painter.setPen(Qt::NoPen);
    painter.setBrush(kInk);
    painter.drawPolygon(QPolygonF({QPointF(0, -h * 0.055), QPointF(h * 0.018, h * 0.022),
                                   QPointF(0, h * 0.008), QPointF(-h * 0.018, h * 0.022)}));
    painter.setPen(kSky);
    painter.setFont(uiFont(h * 0.034, true));
    painter.drawText(QRectF(-h * 0.05, -h * 0.095, h * 0.1, h * 0.04), Qt::AlignCenter,
                     QStringLiteral("N"));
    painter.restore();
  }

  const double barA = appear(p, 0.74, 0.14);
  if (barA > 0) {
    const QRectF bar(w * 0.10, h * 0.90, w * 0.28 * barA, h * 0.012);
    painter.fillRect(bar, withAlpha(kInk, 0.85 * barA));
    painter.setPen(withAlpha(kSky, 0.9 * barA));
    painter.setFont(monoFont(h * 0.028));
    painter.drawText(QPointF(bar.left(), bar.top() - 6), QStringLiteral("0          20m"));
  }

  const double titleA = appear(p, 0.78, 0.16);
  if (titleA > 0) {
    painter.setOpacity(titleA);
    if (!icon.isNull()) {
      const double s = h * 0.11;
      painter.drawPixmap(QRectF(w * 0.07, h * 0.07, s, s), icon, QRectF(icon.rect()));
    }
    painter.setPen(Qt::white);
    painter.setFont(uiFont(h * 0.048, true));
    painter.drawText(QPointF(w * 0.20, h * 0.13), QStringLiteral("현장 도면"));
    painter.setPen(kSky);
    painter.setFont(uiFont(h * 0.028));
    painter.drawText(QPointF(w * 0.20, h * 0.185), QStringLiteral("조사구역이 도면으로 모입니다"));
    painter.setOpacity(1.0);
  }

  const double sheen = std::fmod(t, 3.2) / 3.2;
  if (p > 0.2) {
    const double x = -w * 0.3 + sheen * w * 1.6;
    QLinearGradient sweep(QPointF(x, 0), QPointF(x + w * 0.22, 0));
    sweep.setColorAt(0, QColor(255, 255, 255, 0));
    sweep.setColorAt(0.5, QColor(255, 255, 255, 38));
    sweep.setColorAt(1, QColor(255, 255, 255, 0));
    painter.fillRect(QRectF(0, 0, w, h), sweep);
  }

  painter.setPen(QPen(withAlpha(kLine, 0.4), 1));
  painter.setBrush(Qt::NoBrush);
  painter.drawPath(frame);
  painter.restore();
}
