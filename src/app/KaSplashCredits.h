#pragma once

#include <QRectF>
#include <QString>
#include <QVector>

class QPainter;
class QPixmap;

// Names, copyright and data sources for the startup splash, laid out as a drawing
// title block (표제란) — the form field archaeologists read every day. They are
// painted from the very first frame and stay for the whole reading interval.
namespace KaSplashCredits {

struct Row {
  QString key;
  QString value;
  bool emphasis = false;
};

QVector<Row> rows();
QString creators();
QString copyrightLine();
// All credit text in reading order, for accessibility and tests.
QString plainText();

void paintHeader(QPainter& painter, const QRectF& area, const QPixmap& icon, double shine);
// sweep in (0, 1) runs a light "being recorded" band down the rows.
void paintTitleBlock(QPainter& painter, const QRectF& area, double sweep);
// The reading progress is drawn as a drawing scale bar (축척바).
void paintFooter(QPainter& painter, const QRectF& bar, const QRectF& card, double unit,
                 double progress, const QString& status, const QString& seconds);

}  // namespace KaSplashCredits
