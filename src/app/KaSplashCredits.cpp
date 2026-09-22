#include "KaSplashCredits.h"

#include "KaSplashPalette.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QStringList>

using namespace KaSplashPalette;

namespace {

// Names and notices are never cut off: the text shrinks to fit instead.
QFont fitted(QFont font, const QString& text, double width) {
  while (font.pixelSize() > 9 && QFontMetricsF(font).horizontalAdvance(text) > width)
    font.setPixelSize(font.pixelSize() - 1);
  return font;
}

}  // namespace

namespace KaSplashCredits {

QString productName() {
  return QStringLiteral("Strata");
}

QString productSubtitle(const QString& version) {
  return QStringLiteral("필드고고학 GIS · v%1").arg(version.isEmpty() ? QStringLiteral("2") : version);
}

QString creators() {
  return QStringLiteral("권영인");
}

QString copyrightLine() {
  return QStringLiteral("© 2026 동국문화재연구원 · %1 · GNU GPL v2 이상으로 배포")
      .arg(creators());
}

QString dataLine() {
  return QStringLiteral("지도·자료  VWorld · 국토정보플랫폼 · 국가유산 공간정보 · 흙토람 · KIGAM · "
                        "국사편찬위원회 — 제공처의 이용조건을 따릅니다");
}

QVector<Row> rows() {
  return {
      {QStringLiteral("프로그램"), QStringLiteral("Strata · 필드고고학 GIS v2")},
      {QStringLiteral("기관"), QStringLiteral("동국문화재연구원")},
      {QStringLiteral("만든이"), creators()},
      {QStringLiteral("사용 기술"), QStringLiteral("QGIS GPL v2+ · Qt LGPLv3/GPL")},
      {QString(), QStringLiteral("GDAL/OGR MIT · PROJ MIT")},
      {QString(), QStringLiteral("GEOS LGPLv2.1 · SQLite PD")},
      {QString(), QStringLiteral("Qt WebEngine / Chromium 오픈소스")},
      {QStringLiteral("참조 자료"), QStringLiteral("VWorld · 국토정보플랫폼")},
      {QString(), QStringLiteral("국가유산 공간정보 · 흙토람 · KIGAM · 국사편찬위원회")},
      {QString(), QStringLiteral("제공처의 저작권·이용조건을 따름")},
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

void paintTitle(QPainter& painter, const QPointF& baseline, double unit, const QString& version) {
  const double u = unit;
  painter.save();
  painter.setRenderHint(QPainter::Antialiasing);
  const QFont word = wordmarkFont(36 * u);
  painter.setFont(word);
  painter.setPen(Qt::white);
  painter.drawText(baseline, productName());
  const double x = baseline.x() + QFontMetricsF(word).horizontalAdvance(productName()) + 16 * u;
  // A thin divider, then the Korean name and version on the same baseline.
  painter.setPen(QPen(withAlpha(kSky, 0.45), 1.0));
  painter.drawLine(QPointF(x, baseline.y() - 21 * u), QPointF(x, baseline.y() + 1 * u));
  painter.setFont(lightFont(16 * u));
  painter.setPen(withAlpha(kInk, 0.84));
  painter.drawText(QPointF(x + 15 * u, baseline.y() - 1 * u), productSubtitle(version));
  painter.restore();
}

void paintNotices(QPainter& painter, const QRectF& area, double unit) {
  const double u = unit;
  painter.save();
  const QString copyright = copyrightLine();
  painter.setFont(fitted(uiFont(11.5 * u), copyright, area.width()));
  painter.setPen(withAlpha(kInk, 0.78));
  painter.drawText(QPointF(area.left(), area.top() + 12 * u), copyright);
  const QString data = dataLine();
  painter.setFont(fitted(lightFont(11 * u), data, area.width()));
  painter.setPen(withAlpha(kInk, 0.58));
  painter.drawText(QPointF(area.left(), area.top() + 30 * u), data);
  painter.restore();
}

}  // namespace KaSplashCredits
