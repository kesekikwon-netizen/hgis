#pragma once
// Coordinate callouts on the drawing sheet: a lettered X/Y box with an arrow to the point.
// One builder is shared by clicked points and points imported from the map.
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

class QgsLayout;

namespace KaDrawingCoordCallout {
// Letter for the n-th point (A..Z, then again from A).
QString tagFor(int index);
// Korean survey convention: X is northing (map Y), Y is easting (map X).
QString xyText(double mapX, double mapY);
// Box beside a clicked point, kept on the paper width.
QRectF clickBoxRect(const QPointF& tip, double paperWidthMm);
// First candidate box around tip that does not touch the boxes already used.
QRectF freeBoxRect(const QPointF& tip, const QVector<QRectF>& used);
// Adds box, letter, shaft and head items (ids ka_coord_{box,let,arr,head}_<tag>).
void add(QgsLayout* layout, const QPointF& tip, const QString& tag, const QString& text,
         const QRectF& boxRect);
// Removes every ka_coord_* item.
void removeAll(QgsLayout* layout);
}  // namespace KaDrawingCoordCallout
