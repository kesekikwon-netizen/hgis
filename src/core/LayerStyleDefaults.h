#pragma once

#include <QColor>
#include <QString>
#include <QVector>

class QgsSymbol;

// The one table of default symbols for survey domain layers. applyDomainDrawStyle
// draws from it and readSimpleVectorStyle falls back to it, so both always agree.
// Saved user colours (ka_hgis/style_*) override it and are never rewritten here.
namespace LayerStyleDefaults {

struct DomainStyle {
  QColor fill;
  QColor stroke;
  double strokeWidthMm = 1.2;
  double markerSizeMm = 3.5;
  // section_line: a white casing under the coloured core keeps the cut line
  // readable on satellite and cadastral pictures. Colour edits change the core only.
  bool casing = false;
  QColor casingColor;
  double casingWidthMm = 0.0;
};

// Defaults for a domain layer_key; other keys get the generic blue style.
DomainStyle forKey(const QString& layerKey);

// Line symbol with the white casing plus a core line of the given colour/width.
QgsSymbol* casedLineSymbol(const DomainStyle& base, const QColor& core, double coreWidthMm,
                           bool dashed = false);

// The simple symbol both applyDomainDrawStyle and applySimpleVectorStyle draw with.
// geometryType is a Qgis::GeometryType value; null for unsupported geometry. A cased
// base (section_line) keeps its casing whenever the stroke is drawn.
QgsSymbol* buildSymbol(int geometryType, const DomainStyle& base, const QColor& fill,
                       const QColor& stroke, double strokeWidthMm, double markerSizeMm,
                       bool noFill, bool noStroke, bool dashed);

// Colours owned by the survey domain defaults and the heritage datasets. Colours the
// app picks by itself (e.g. 종류별 자동 색) keep away from them so survey output and
// reference data stay apart on the map. User-picked colours are never checked.
QVector<QColor> reservedColors();

// CIE76 colour difference in L*a*b* (alpha ignored).
double deltaE76(const QColor& a, const QColor& b);

// HeritageStyle::isReservedColor (black, grey, red) or closer than minDeltaE to
// one of reservedColors().
bool isNearReservedColor(const QColor& color, double minDeltaE = 20.0);

}  // namespace LayerStyleDefaults
