#include "KaSplashScene.h"

#include "KaSplashPalette.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QStringList>

using namespace KaSplashPalette;

namespace {

double appear(double phase, double start, double span) {
  return easeOut((phase - start) / qMax(0.04, span));
}

QPainterPath surveyArea(double w, double h) {
  QPainterPath path;
  path.moveTo(w * 0.22, h * 0.42);
  path.cubicTo(w * 0.34, h * 0.30, w * 0.58, h * 0.32, w * 0.72, h * 0.44);
  path.cubicTo(w * 0.84, h * 0.54, w * 0.80, h * 0.74, w * 0.62, h * 0.80);
  path.cubicTo(w * 0.44, h * 0.86, w * 0.26, h * 0.78, w * 0.18, h * 0.62);
  path.closeSubpath();
  return path;
}

}  // namespace

void KaSplashScene::paint(QPainter& painter, double phase, const QPixmap& icon) const {
  if (m_rect.isEmpty()) return;
  const double w = m_rect.width();
  const double h = m_rect.height();
  const double p = clamp01(phase);
  // Settles in the first part of the reading interval, then stays still.
  const double settle = appear(p, 0.0, 0.42);

  painter.save();
  QPainterPath frame;
  frame.addRoundedRect(QRectF(0, 0, w, h), 10, 10);
  painter.translate(m_rect.topLeft());
  painter.setClipPath(frame);

  painter.fillRect(QRectF(0, 0, w, h), QColor(236, 242, 246));

  const QRectF window(w * 0.06, h * (0.10 - 0.04 * (1.0 - settle)), w * 0.88, h * 0.80);
  painter.setOpacity(0.35 + 0.65 * settle);
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(255, 255, 255));
  painter.drawRoundedRect(window, 8, 8);

  const QRectF title(window.left(), window.top(), window.width(), window.height() * 0.11);
  painter.setBrush(QColor(18, 52, 84));
  painter.drawRoundedRect(title, 8, 8);
  painter.fillRect(QRectF(title.left(), title.bottom() - 8, title.width(), 8), QColor(18, 52, 84));

  if (!icon.isNull() && settle > 0.2) {
    const double s = title.height() * 0.62;
    painter.drawPixmap(QRectF(title.left() + 10, title.center().y() - s / 2, s, s), icon,
                       QRectF(icon.rect()));
  }
  painter.setPen(QColor(255, 255, 255));
  painter.setFont(uiFont(title.height() * 0.42, true));
  painter.drawText(QRectF(title.left() + title.height(), title.top(), title.width() * 0.7, title.height()),
                   Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("필드고고학GIS"));

  const QRectF map(window.left() + window.width() * 0.22, title.bottom(),
                   window.width() * 0.78, window.bottom() - title.bottom());
  painter.fillRect(map, QColor(214, 226, 232));

  const QRectF legend(window.left(), title.bottom(), window.width() * 0.22,
                      window.bottom() - title.bottom());
  painter.fillRect(legend, QColor(248, 250, 252));
  painter.setPen(QPen(QColor(210, 220, 228), 1));
  painter.drawLine(legend.topRight(), legend.bottomRight());

  const double listA = appear(p, 0.12, 0.2);
  if (listA > 0) {
    painter.setOpacity((0.35 + 0.65 * settle) * listA);
    const QStringList rows = {QStringLiteral("조사 데이터"), QStringLiteral("지적도"),
                              QStringLiteral("참조 지도")};
    painter.setFont(uiFont(legend.width() * 0.13));
    for (int i = 0; i < rows.size(); ++i) {
      const QRectF row(legend.left() + 8, legend.top() + 12 + i * legend.height() * 0.16,
                       legend.width() - 16, legend.height() * 0.12);
      painter.setPen(Qt::NoPen);
      painter.setBrush(QColor(232, 238, 244));
      painter.drawRoundedRect(row, 3, 3);
      painter.setPen(QColor(40, 58, 74));
      painter.drawText(row.adjusted(6, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft, rows.at(i));
    }
  }

  painter.setOpacity(0.35 + 0.65 * settle);
  const double gridA = appear(p, 0.08, 0.16);
  if (gridA > 0) {
    const double step = map.height() / 6.0;
    painter.setPen(QPen(QColor(180, 198, 208, int(90 * gridA)), 1));
    for (double x = map.left() + step; x < map.right(); x += step)
      painter.drawLine(QPointF(x, map.top()), QPointF(x, map.bottom()));
    for (double y = map.top() + step; y < map.bottom(); y += step)
      painter.drawLine(QPointF(map.left(), y), QPointF(map.right(), y));
  }

  const double areaA = appear(p, 0.18, 0.22);
  if (areaA > 0) {
    painter.save();
    painter.setClipRect(map);
    painter.translate(map.topLeft());
    const QPainterPath area = surveyArea(map.width(), map.height());
    painter.setOpacity((0.35 + 0.65 * settle) * areaA);
    painter.setBrush(QColor(40, 120, 168, 36));
    painter.setPen(QPen(QColor(18, 72, 112), 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(area);
    painter.restore();
  }

  painter.setOpacity(1.0);
  painter.setPen(QPen(QColor(196, 208, 216), 1));
  painter.setBrush(Qt::NoBrush);
  painter.drawRoundedRect(window, 8, 8);
  painter.restore();
}
