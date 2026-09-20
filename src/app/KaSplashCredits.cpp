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
  return QStringLiteral("권영인");
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

void paintHeader(QPainter& painter, const QRectF& area, const QPixmap& icon, double appear) {
  painter.save();
  painter.setOpacity(clamp01(appear));
  const double side = area.height();
  const qreal dpr = painter.device() ? painter.device()->devicePixelRatioF() : 1.0;
  if (!icon.isNull()) {
    QImage image = icon.toImage()
                       .scaled(QSize(int(side * dpr), int(side * dpr)), Qt::KeepAspectRatio,
                               Qt::SmoothTransformation)
                       .convertToFormat(QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
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
  painter.restore();
}

void paintTitleBlock(QPainter& painter, const QRectF& area, double appear) {
  const QVector<Row> list = rows();
  const double rowHeight = area.height() / list.size();
  const double keyWidth = area.width() * 0.22;
  painter.save();
  painter.setOpacity(clamp01(appear));
  painter.fillRect(area, QColor(9, 47, 86, 120));
  painter.setClipRect(area);
  painter.setPen(QPen(withAlpha(kLine, 0.4), 1));
  painter.drawLine(QPointF(area.left() + keyWidth, area.top()),
                   QPointF(area.left() + keyWidth, area.bottom()));
  for (int i = 0; i < list.size(); ++i) {
    const Row& row = list.at(i);
    const double top = area.top() + i * rowHeight;
    if (i > 0) {
      painter.setPen(QPen(withAlpha(kLine, row.key.isEmpty() ? 0.1 : 0.28), 1));
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
  painter.setPen(QPen(withAlpha(kLine, 0.4), 1));
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(area.adjusted(0, 0, -1, -1));
  painter.restore();
}

void paintFooter(QPainter& painter, const QRectF& bar, const QRectF& card, double unit,
                 double progress, const QString& status, const QString& seconds) {
  Q_UNUSED(seconds);
  painter.setFont(uiFont(11 * unit));
  painter.setPen(withAlpha(kInk, 0.88));
  painter.drawText(QPointF(card.left() + 28 * unit, bar.top() - 12 * unit), status);

  painter.fillRect(bar, withAlpha(kInk, 0.14));
  painter.fillRect(QRectF(bar.left(), bar.top(), bar.width() * clamp01(progress), bar.height()),
                   withAlpha(kInk, 0.9));

  painter.setPen(withAlpha(kInk, 0.72));
  painter.setFont(uiFont(10.5 * unit));
  painter.drawText(QRectF(card.left() + 28 * unit, bar.top() - 28 * unit,
                          card.width() - 56 * unit, 16 * unit),
                   Qt::AlignRight | Qt::AlignVCenter, copyrightLine());
}

}  // namespace KaSplashCredits
