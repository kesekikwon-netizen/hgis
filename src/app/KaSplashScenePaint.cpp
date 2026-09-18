// Painting for KaSplashScene: grid, measured drawing, remaining soil, survey
// markers and the trowel pointer. Kept apart from the scene state and scraping.
#include "KaSplashScene.h"

#include "KaSplashPalette.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QPixmap>


using namespace KaSplashPalette;

void KaSplashScene::paint(QPainter& painter, double phase, const QPixmap& trowel) const {
  if (m_soil.isNull()) return;
  const double w = m_rect.width(), h = m_rect.height();
  QPainterPath frame;
  frame.addRoundedRect(m_rect, 14, 14);
  painter.save();
  painter.setClipPath(frame, Qt::IntersectClip);
  painter.fillPath(frame, kPlanShade);
  painter.translate(m_rect.topLeft());

  const double cell = h / 10.0;  // 방안지: 1 m lines, heavier every 5 m
  for (int i = 0; i * cell <= w + 0.5; ++i) {
    painter.setPen(QPen(withAlpha(kLine, i % 5 ? 0.1 : 0.26), 1));
    painter.drawLine(QPointF(i * cell, 0), QPointF(i * cell, h));
  }
  for (int j = 0; j <= 10; ++j) {
    painter.setPen(QPen(withAlpha(kLine, j % 5 ? 0.1 : 0.26), 1));
    painter.drawLine(QPointF(0, j * cell), QPointF(w, j * cell));
  }

  const int hover = hoveredFeature();
  for (int i = 0; i < m_features.size(); ++i) {
    const Feature& f = m_features.at(i);
    const bool lit = i == hover;
    if (f.style == Style::Filled) {
      painter.fillPath(f.shape, lit ? QColor(Qt::white) : kGold);
      continue;
    }
    QPen pen(lit ? kGold : kInk, lit ? 2.6 : 1.6);
    if (f.style == Style::Dashed) pen.setDashPattern({6, 4});
    painter.setPen(pen);
    painter.setBrush(lit ? QBrush(withAlpha(kGold, 0.12)) : QBrush(Qt::NoBrush));
    painter.drawPath(f.shape);
  }
  painter.setPen(withAlpha(kInk, 0.75));
  painter.setFont(monoFont(h * 0.034));
  painter.drawText(QPointF(w * 0.2, h * 0.64), QStringLiteral("+32.45m"));

  painter.drawImage(QPointF(0, 0), m_soil);

  const double slope = h * kSlope, edge = m_autoScrape * (w + slope);
  if (m_autoScrape > 0.0 && m_autoScrape < 1.0) {  // the automatic trowel pass
    painter.setPen(QPen(kSand, 3, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(edge, 0), QPointF(edge - slope, h));
    if (!m_pointerInside && !trowel.isNull()) {
      const double s = h * 0.12;
      painter.drawPixmap(QRectF(edge - slope * 0.5 - s * 0.2, h * 0.5 - s * 0.9, s, s), trowel,
                         QRectF(trowel.rect()));
    }
  }

  painter.setFont(uiFont(h * 0.036, true));
  for (int i = 0; i < m_features.size(); ++i) {
    const Feature& f = m_features.at(i);
    if (f.label.isNull() || !uncovered(f.shape.boundingRect().center())) continue;
    painter.setPen(i == hover ? kGold : kSky);
    painter.drawText(f.label, f.name);
  }

  for (const Grain& g : m_grains) {
    const QColor shade = g.shade == 0 ? kSand : (g.shade == 1 ? kOchre : kLoam);
    painter.fillRect(QRectF(g.pos, QSizeF(2.2, 2.2)), withAlpha(shade, g.life));
  }

  paintMarkers(painter, phase);

  if (m_pointerInside) paintPointer(painter, hover, trowel);

  painter.restore();
  painter.setPen(QPen(withAlpha(kLine, 0.45), 1));
  painter.setBrush(Qt::NoBrush);
  painter.drawPath(frame);
}

// Survey benchmarks and the north arrow sit on the surface, above the soil.
void KaSplashScene::paintMarkers(QPainter& painter, double phase) const {
  const double alpha = easeOut((phase - 0.05) * 9);
  if (alpha <= 0.0) return;
  const double w = m_rect.width(), h = m_rect.height(), r = h * 0.02;
  painter.save();
  painter.setOpacity(alpha);
  for (const QPointF& bm : {QPointF(w * 0.09, h * 0.08), QPointF(w * 0.86, h * 0.62)}) {
    painter.setPen(QPen(kGold, 1.6));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(bm, r, r);
    painter.drawLine(bm - QPointF(r * 1.8, 0), bm + QPointF(r * 1.8, 0));
    painter.drawLine(bm - QPointF(0, r * 1.8), bm + QPointF(0, r * 1.8));
  }
  painter.setPen(kSky);
  painter.setFont(monoFont(h * 0.027));  // leaves room for the north arrow
  painter.drawText(QPointF(w * 0.09 + r * 2.4, h * 0.08 - r * 0.5),
                   QStringLiteral("BM-1  X 450118.07  Y 200431.52"));
  painter.drawText(QPointF(w * 0.86 - r * 6.2, h * 0.62 - r * 1.6), QStringLiteral("BM-2"));

  painter.save();
  painter.translate(w - h * 0.07, h * 0.1);
  painter.rotate((1.0 - easeOut((phase - 0.04) * 6)) * 126.0);  // settles on true north
  painter.setPen(Qt::NoPen);
  painter.setBrush(kInk);
  painter.drawPolygon(QPolygonF({QPointF(0, -h * 0.05), QPointF(h * 0.018, h * 0.022),
                                 QPointF(0, h * 0.01), QPointF(-h * 0.018, h * 0.022)}));
  painter.restore();
  painter.setPen(kInk);
  painter.setFont(uiFont(h * 0.032, true));
  painter.drawText(QRectF(w - h * 0.12, h * 0.005, h * 0.1, h * 0.04), Qt::AlignCenter,
                   QStringLiteral("N"));
  painter.restore();
}

// The pointer is a trowel with its scraping radius; a revealed feature under it
// is lit and named in a small tag.
void KaSplashScene::paintPointer(QPainter& painter, int hover, const QPixmap& trowel) const {
  const double w = m_rect.width(), h = m_rect.height(), radius = h * 0.05;
  painter.setPen(QPen(withAlpha(kSand, 0.7), 1, Qt::DashLine));
  painter.setBrush(Qt::NoBrush);
  painter.drawEllipse(m_pointer, radius, radius);
  if (!trowel.isNull()) {
    const double s = h * 0.13;
    painter.drawPixmap(QRectF(m_pointer.x() - s * 0.25, m_pointer.y() - s * 0.85, s, s), trowel,
                       QRectF(trowel.rect()));
  }
  if (hover < 0) return;
  const Feature& f = m_features.at(hover);
  const QString text = f.name + QStringLiteral("  ·  ") + f.detail;
  const QFont tipFont = uiFont(h * 0.034, true);
  const double tw = QFontMetricsF(tipFont).horizontalAdvance(text) + 18;
  QRectF tip(m_pointer + QPointF(14, 12), QSizeF(tw, h * 0.07));
  if (tip.right() > w - 4) tip.moveRight(m_pointer.x() - 14);
  if (tip.bottom() > h - 4) tip.moveBottom(m_pointer.y() - 12);
  QPainterPath bubble;
  bubble.addRoundedRect(tip, 8, 8);
  painter.fillPath(bubble, QColor(9, 47, 86, 230));
  painter.setPen(QPen(kGold, 1));
  painter.drawPath(bubble);
  painter.setFont(tipFont);
  painter.setPen(Qt::white);
  painter.drawText(tip, Qt::AlignCenter, text);
}
