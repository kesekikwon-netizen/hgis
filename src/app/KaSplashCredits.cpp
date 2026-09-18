#include "KaSplashCredits.h"

#include "KaSplashPalette.h"

#include <QFontMetricsF>
#include <QImage>
#include <QLinearGradient>
#include <QPaintDevice>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QStringList>

using namespace KaSplashPalette;

namespace {

QFont fitted(const QString& text, double pixels, bool bold, double width) {
  QFont font = uiFont(pixels, bold);
  while (font.pixelSize() > 9 && QFontMetricsF(font).horizontalAdvance(text) > width)
    font.setPixelSize(font.pixelSize() - 1);
  return font;
}

}  // namespace

namespace KaSplashCredits {

QString creators() {
  return QStringLiteral("권영인 · 조유량 · 박종환");
}

QString copyrightLine() {
  return QStringLiteral("© 2026 동국문화재연구원 · %1 · GNU GPL v2 이상으로 배포")
      .arg(creators());
}

QVector<Row> rows() {
  return {
      {QStringLiteral("도면명"), QStringLiteral("필드고고학GIS v2"), true},
      {QStringLiteral("조사기관"), QStringLiteral("동국문화재연구원"), false},
      {QStringLiteral("만든이"), creators(), true},
      {QStringLiteral("좌표계"), QStringLiteral("EPSG:5186 · 5187 → 제출 5179"), false},
      {QStringLiteral("사용 기술"), QStringLiteral("QGIS GPL v2+ · Qt LGPLv3/GPL"), false},
      {QString(), QStringLiteral("GDAL/OGR MIT · PROJ MIT"), false},
      {QString(), QStringLiteral("GEOS LGPLv2.1 · SQLite PD"), false},
      {QString(), QStringLiteral("Qt WebEngine / Chromium 오픈소스"), false},
      {QStringLiteral("참조 자료"), QStringLiteral("VWorld · 국토정보플랫폼"), false},
      {QString(), QStringLiteral("국가유산 공간정보 · 흙토람 · KIGAM"), false},
      {QString(), QStringLiteral("제공처의 저작권·이용조건을 따름"), false},
  };
}

QString plainText() {
  QStringList lines;
  QString key;
  for (const Row& row : rows()) {
    if (!row.key.isEmpty()) key = row.key;
    lines << QStringLiteral("%1: %2").arg(key, row.value);
  }
  lines << copyrightLine();
  return lines.join(QLatin1Char('\n'));
}

void paintHeader(QPainter& painter, const QRectF& area, const QPixmap& icon, double shine) {
  const double side = area.height();
  const qreal dpr = painter.device() ? painter.device()->devicePixelRatioF() : 1.0;
  if (!icon.isNull()) {
    QImage image = icon.toImage()
                       .scaled(QSize(int(side * dpr), int(side * dpr)), Qt::KeepAspectRatio,
                               Qt::SmoothTransformation)
                       .convertToFormat(QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    if (shine > 0.0 && shine < 1.0) {
      // A light glint crosses the gold trowel, clipped to the icon's own shape.
      QPainter glint(&image);
      glint.setCompositionMode(QPainter::CompositionMode_SourceAtop);
      const double x = -side * 0.4 + shine * side * 1.8;
      QLinearGradient band(x, 0, x + side * 0.3, 0);
      band.setColorAt(0, QColor(255, 248, 220, 0));
      band.setColorAt(0.5, QColor(255, 248, 220, 190));
      band.setColorAt(1, QColor(255, 248, 220, 0));
      glint.fillRect(QRectF(0, 0, side, side), band);
    }
    painter.drawImage(QPointF(area.left(), area.top()), image);
  }
  const double textLeft = area.left() + side * 1.22;
  painter.setPen(Qt::white);
  painter.setFont(uiFont(side * 0.5, true));
  painter.drawText(QPointF(textLeft, area.top() + side * 0.56), QStringLiteral("필드고고학GIS"));
  painter.setPen(kSky);
  painter.setFont(uiFont(side * 0.25));
  painter.drawText(QPointF(textLeft, area.bottom() - side * 0.06),
                   QStringLiteral("v2 · 현장을 도면으로"));
}

void paintTitleBlock(QPainter& painter, const QRectF& area, double sweep) {
  const QVector<Row> list = rows();
  const double rowHeight = area.height() / list.size();
  const double keyWidth = area.width() * 0.22;
  QPainterPath frame;
  frame.addRoundedRect(area, 10, 10);
  painter.save();
  painter.fillPath(frame, QColor(9, 47, 86, 120));
  painter.setClipPath(frame);
  if (sweep > 0.0 && sweep < 1.0) {
    const double y = area.top() + sweep * area.height();
    QLinearGradient band(0, y - rowHeight, 0, y + rowHeight);
    band.setColorAt(0, QColor(255, 255, 255, 0));
    band.setColorAt(0.5, QColor(255, 255, 255, 34));
    band.setColorAt(1, QColor(255, 255, 255, 0));
    painter.fillRect(area, band);
  }
  painter.setPen(QPen(withAlpha(kLine, 0.55), 1));
  painter.drawLine(QPointF(area.left() + keyWidth, area.top()),
                   QPointF(area.left() + keyWidth, area.bottom()));
  for (int i = 0; i < list.size(); ++i) {
    const Row& row = list.at(i);
    const double top = area.top() + i * rowHeight;
    if (i > 0) {
      painter.setPen(QPen(withAlpha(kLine, row.key.isEmpty() ? 0.14 : 0.42), 1));
      painter.drawLine(QPointF(area.left(), top), QPointF(area.right(), top));
    }
    const double baseline = top + rowHeight * 0.68;
    painter.setPen(kSky);
    painter.setFont(fitted(row.key, rowHeight * 0.4, false, keyWidth - 14));
    painter.drawText(QPointF(area.left() + 8, baseline), row.key);
    // Names and licence notices are never cut off: the text shrinks to fit instead.
    painter.setFont(fitted(row.value, rowHeight * (row.emphasis ? 0.5 : 0.42), row.emphasis,
                           area.width() - keyWidth - 16));
    painter.setPen(row.emphasis ? QColor(Qt::white) : kInk);
    painter.drawText(QPointF(area.left() + keyWidth + 9, baseline), row.value);
  }
  painter.restore();
  painter.setPen(QPen(withAlpha(kLine, 0.6), 1));
  painter.setBrush(Qt::NoBrush);
  painter.drawPath(frame);
}

void paintFooter(QPainter& painter, const QRectF& bar, const QRectF& card, double unit,
                 double progress, const QString& status, const QString& seconds) {
  painter.setFont(uiFont(12.5 * unit));
  painter.setPen(kInk);
  painter.drawText(QPointF(bar.left(), bar.top() - 8 * unit), status);
  painter.drawText(QRectF(bar.left(), bar.top() - 26 * unit, bar.width(), 20 * unit),
                   Qt::AlignRight | Qt::AlignBottom, seconds);

  QPainterPath track;
  track.addRoundedRect(bar, bar.height() / 2, bar.height() / 2);
  painter.save();
  painter.fillPath(track, withAlpha(kInk, 0.18));
  painter.setClipPath(track);
  constexpr int kSegments = 5;
  const double segment = bar.width() / kSegments;
  for (int i = 0; i < kSegments; ++i) {
    const double filled = clamp01(progress * kSegments - i);
    painter.fillRect(QRectF(bar.left() + segment * i, bar.top(), segment * filled, bar.height()),
                     i % 2 ? kInk : kGold);
  }
  painter.restore();
  painter.setPen(QPen(withAlpha(kInk, 0.6), 1));
  painter.setBrush(Qt::NoBrush);
  painter.drawPath(track);

  painter.setPen(kSky);
  painter.setFont(uiFont(11 * unit));
  for (int i = 0; i <= kSegments; ++i) {
    const QString label = i == kSegments ? QStringLiteral("10초") : QString::number(i * 2);
    const double x = bar.left() + segment * i;
    const double width = QFontMetricsF(painter.font()).horizontalAdvance(label);
    const double left = i == 0 ? x : (i == kSegments ? x - width : x - width / 2);
    painter.drawText(QPointF(left, bar.bottom() + 15 * unit), label);
  }

  painter.setPen(withAlpha(kInk, 0.88));
  painter.setFont(uiFont(11.5 * unit));
  painter.drawText(QRectF(card.left() + 24 * unit, card.bottom() - 30 * unit,
                          card.width() - 48 * unit, 22 * unit),
                   Qt::AlignRight | Qt::AlignVCenter, copyrightLine());
}

}  // namespace KaSplashCredits
