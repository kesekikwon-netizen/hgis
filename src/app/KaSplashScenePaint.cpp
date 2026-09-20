// Painting for KaSplashScene: grid, measured drawing and survey markers.
#include "KaSplashScene.h"

#include "KaSplashPalette.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QPixmap>


using namespace KaSplashPalette;

void KaSplashScene::paint(QPainter& painter, double phase, const QPixmap&) const {
  if (m_rect.isEmpty()) return;
  const double w = m_rect.width(), h = m_rect.height();
  QPainterPath frame;
  frame.addRect(m_rect);
  painter.save();
  painter.setClipPath(frame, Qt::IntersectClip);
  painter.fillPath(frame, kPlanShade);
  painter.translate(m_rect.topLeft());

  const double cell = h / 10.0;
  for (int i = 0; i * cell <= w + 0.5; ++i) {
    painter.setPen(QPen(withAlpha(kLine, i % 5 ? 0.1 : 0.26), 1));
    painter.drawLine(QPointF(i * cell, 0), QPointF(i * cell, h));
  }
  for (int j = 0; j <= 10; ++j) {
    painter.setPen(QPen(withAlpha(kLine, j % 5 ? 0.1 : 0.26), 1));
    painter.drawLine(QPointF(0, j * cell), QPointF(w, j * cell));
  }

  for (const Feature& f : m_features) {
    if (f.style == Style::Filled) {
      painter.fillPath(f.shape, withAlpha(kInk, 0.82));
      continue;
    }
    QPen pen(kInk, 1.6);
    if (f.style == Style::Dashed) pen.setDashPattern({6, 4});
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(f.shape);
  }
  painter.setPen(withAlpha(kInk, 0.75));
  painter.setFont(monoFont(h * 0.034));
  painter.drawText(QPointF(w * 0.2, h * 0.64), QStringLiteral("+32.45m"));

  painter.setFont(uiFont(h * 0.036, true));
  for (const Feature& f : m_features) {
    if (f.label.isNull()) continue;
    painter.setPen(kSky);
    painter.drawText(f.label, f.name);
  }

  paintMarkers(painter, phase);
  painter.restore();
  painter.setPen(QPen(withAlpha(kLine, 0.45), 1));
  painter.setBrush(Qt::NoBrush);
  painter.drawPath(frame);
}

void KaSplashScene::paintMarkers(QPainter& painter, double phase) const {
  const double alpha = easeOut((phase - 0.12) * 3.2);
  if (alpha <= 0.0) return;
  const double w = m_rect.width(), h = m_rect.height(), r = h * 0.016;
  painter.save();
  painter.setOpacity(alpha);
  for (const QPointF& bm : {QPointF(w * 0.09, h * 0.08), QPointF(w * 0.86, h * 0.62)}) {
    painter.setPen(QPen(kInk, 1.1));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(bm, r, r);
    painter.drawLine(bm - QPointF(r * 1.6, 0), bm + QPointF(r * 1.6, 0));
    painter.drawLine(bm - QPointF(0, r * 1.6), bm + QPointF(0, r * 1.6));
  }
  painter.setPen(kSky);
  painter.setFont(monoFont(h * 0.027));  // leaves room for the north arrow
  painter.drawText(QPointF(w * 0.09 + r * 2.4, h * 0.08 - r * 0.5),
                   QStringLiteral("BM-1  X 450118.07  Y 200431.52"));
  painter.drawText(QPointF(w * 0.86 - r * 6.2, h * 0.62 - r * 1.6), QStringLiteral("BM-2"));

  painter.save();
  painter.translate(w - h * 0.07, h * 0.1);
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
