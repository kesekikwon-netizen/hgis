#pragma once
// Section sheet decorations shared by SectionLayoutService::buildSectionLayout
// (new sheet) and applyDecorationOptions (in place, keeps user moves).

#include <QFont>
#include <QRectF>
#include <QString>

#include "SectionLayoutService.h"

class QgsLayout;
class QgsLayoutItem;
class QgsLayoutItemLabel;
class QgsLayoutItemPolyline;

namespace SectionSheetDecor {

// Custom properties written on generated items.
QString tickKindKey();   // "elevation" / "distance" on tick labels
QString tickValueKey();  // numeric tick value on tick labels

// Font through the label text format (QgsLayoutItemLabel::setFont is deprecated).
void setLabelFont(QgsLayoutItemLabel* label, const QFont& font);

QString titleText(const SectionLayoutOptions& options);
QString elevationText(double value, const SectionLayoutOptions& options);
QString distanceText(double value);
QFont tickFont(const SectionLayoutOptions& options);
// "EPSG:5187 · 동부원점(GRS80)" when the origin has a Korean name.
QString crsText(const QString& authId);

// Red dashed reference line style (colour/width from options).
void styleReferenceLine(QgsLayoutItemPolyline* item, const SectionLayoutOptions& options);
// Reference line along the bottom edge of the section map frame.
QgsLayoutItemPolyline* addReferenceLine(QgsLayout* layout, const QRectF& mapScene,
                                        const SectionLayoutOptions& options);
// Optional one-line note just above the map frame's left edge.
QgsLayoutItemLabel* addNote(QgsLayout* layout, const QRectF& mapScene, const QString& text);
// Remember where the builder put a chrome item, so a later rebuild can tell
// whether the user moved it.
void markAutoPosition(QgsLayoutItem* item);

}  // namespace SectionSheetDecor
