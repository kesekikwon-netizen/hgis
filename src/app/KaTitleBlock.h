#pragma once
// Optional title block (표제란) for the drawing sheet. The default sheet stays
// "map frame only"; this item is added only when the user asks for it.
#include <QDate>
#include <QRectF>
#include <QString>

class QgsLayout;
class QgsLayoutItemLabel;
class QgsProject;

namespace KaTitleBlock {
QString itemId();
struct Info {
  QString surveyName;
  QString siteName;
};
// survey_name / site_name of the first survey_area feature that has them.
Info collect(QgsProject* project);
// Label text. The scale line follows the linked map item through a QGIS expression.
QString text(const QString& drawingTitle, const Info& info, const QDate& date,
             const QString& mapItemId);
// Free space below the CRS label; inside the lower right of the map when the strip is too small.
QRectF defaultRect(const QRectF& page, const QRectF& mapRect);
// Creates the title block at rect, or updates the text of the existing one in place.
QgsLayoutItemLabel* place(QgsLayout* layout, const QRectF& rect, const QString& drawingTitle,
                          const QString& text);
// Drawing name typed last time, empty when there is no title block.
QString drawingTitleOf(QgsLayout* layout);
}  // namespace KaTitleBlock
