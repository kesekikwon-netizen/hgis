#pragma once

#include <QColor>
#include <QSet>
#include <QString>

class QgsVectorLayer;

// 「종류별 자동 색」 for feature polygons (implementer feature, no field decision).
//
// Rules:
// - a kind always gets the same colour: sorted kinds, colour from a stable hash of
//   the kind text (not QSet order, not a per-process seeded qHash);
// - a 「미분류」 category always exists, so a freshly drawn feature whose kind is
//   still empty stays visible (no attribute popup while drawing);
// - colours keep away from survey defaults and heritage colours
//   (LayerStyleDefaults::isNearReservedColor);
// - the choice is remembered on the layer (ka_hgis/style_mode = by_kind), so the
//   draw/attribute paths rebuild the categories instead of resetting to one
//   symbol; picking a single colour in the style dialog clears it.
namespace LayerStyleKinds {

constexpr const char* kPropStyleMode = "ka_hgis/style_mode";
constexpr const char* kModeByKind = "by_kind";

QString unclassifiedLabel();

// Stable colour for one kind. Colours in `taken` are skipped while others remain.
QColor colorForKind(const QString& kind, const QSet<QRgb>& taken = {});

// The attribute the categories use: kind, else period. Empty when neither exists.
QString categoryField(const QgsVectorLayer* layer);

// Builds the categorized renderer (kinds + 미분류) and remembers the mode.
// False when the layer has neither field; the caller keeps its single symbol.
bool apply(QgsVectorLayer* layer);

bool isByKind(const QgsVectorLayer* layer);
// Drops the by_kind mark only (another automatic look keeps its own mark).
void clearMode(QgsVectorLayer* layer);
// Another automatic look owns the renderer (e.g. FeaturePresets "preset"): the
// drawing path must not reset it to one symbol.
bool hasOtherAutomaticLook(const QgsVectorLayer* layer);
// The user picked one colour: every automatic look ends.
void clearAnyAutomaticLook(QgsVectorLayer* layer);

}  // namespace LayerStyleKinds
