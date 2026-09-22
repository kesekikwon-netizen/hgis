#pragma once

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

class QPainter;

// Name, copyright and data sources for the startup splash. The notice paints a
// short form in the lower left: the Strata title, the copyright line and one
// line of data providers. plainText() keeps the full list for accessibility;
// the About box lists every licence.
namespace KaSplashCredits {

struct Row {
  QString key;
  QString value;
};

QString productName();
// "필드고고학 GIS · v2.0.0", or the major version alone when none is set.
QString productSubtitle(const QString& version);
QString creators();
QString copyrightLine();
// The map and data providers, as the notice paints them on one line.
QString dataLine();
QVector<Row> rows();
// All credit text in reading order, for accessibility and tests.
QString plainText();

// "Strata │ 필드고고학 GIS · v2.0.0" on one baseline.
void paintTitle(QPainter& painter, const QPointF& baseline, double unit, const QString& version);
// Copyright line over the data line, each kept on one line inside area.
void paintNotices(QPainter& painter, const QRectF& area, double unit);

}  // namespace KaSplashCredits
