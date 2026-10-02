#pragma once
#include <QColor>
#include <QPalette>
#include <QString>
#include <QStringList>

class QApplication;
class QStyle;
class QWidget;

namespace KaTheme {

// Strata chrome colors. The first block holds the semantic names that new code
// and ka-hgis.qss (@name@) use. The legacy block keeps the old sky-blue theme's
// names as aliases of those values so existing callers paint the same colors.
struct Tokens {
  // The one blue accent and its hover, deep and wash steps.
  QColor accent, accentHover, accentDeep, accentWash;
  QColor ink, inkMuted, inkDisabled;
  // surface: cards, fields, toolbars. desk: page under the cards. altRow:
  // alternate rows and inner panels. stripe: the dense layer list's alternate rows.
  QColor surface, desk, altRow, stripe;
  // State washes, lightest to strongest: hover < selected < pressed.
  QColor hover, selected, pressed, disabledSurface;
  QColor border, borderStrong, edgeLight;
  QColor canvasNeutral, danger, ok, successSurface, dangerSurface;
  // Warning pair for chips and notes: warn reads 6.8:1 on warnSurface, 7.5:1 on white.
  QColor warn, warnSurface;
  QColor rail, railText, railMuted;
  // Progress chunk: ink text on it and the chunk on white both stay readable.
  QColor progressFill;
  QColor focusRing;
  // Ribbon (mockup): chip labels, the chosen tool's label, and the group names under the chips.
  QColor ribbonLabelInk, ribbonActiveInk, ribbonGroupInk;

  // Legacy aliases (same values as above). Do not use in new code.
  QColor sky0, sky1, sky2, sky3, sky4, sky5, sky6;
  QColor bevelLight, bevelDark;
  QColor glossMiddle, glossBottom, hoverTop, hoverBottom;
  QColor pressedTop, pressedBottom, selectedTop, selectedBottom;
  QColor glossReflection, glossShoulder, accentReflection;
};

// Optional display profile. The default is the stock Strata look; every option is
// opt-in and none changes chip sizes or the 10 px layer list. bundledFonts is off:
// the chrome uses Malgun Gothic, whose Windows hinting stays crisp at 11-13 px. The
// bundled IBM Plex Sans KR (data/fonts, KaThemeFonts.h) rendered visibly softer and
// thinner on the field PC (user report 2026-09-30), so it is only on request:
// DisplayOptions or KA_HGIS_BUNDLED_FONTS=1.
struct DisplayOptions {
  bool highContrast = false;  // stronger borders, muted text and state washes
  bool largeText = false;     // 13 px body text becomes 15 px
  bool bundledFonts = false;  // IBM Plex Sans KR / Mono from data/fonts when registered
  bool operator==(const DisplayOptions&) const = default;
};

// Active tokens (the default profile unless setDisplayOptions chose another).
const Tokens& tokens();
Tokens tokensFor(const DisplayOptions& options);
const DisplayOptions& displayOptions();
// Rebuilds tokens, palette and style sheet. Default options restore the stock look.
void setDisplayOptions(QApplication* app, const DisplayOptions& options);

// Restrained function colors; labels and outlines remain dark for field use.
struct IconPalette {
  QColor ink, file, record, map, align, output;
  QColor water, earth, earthLight, rock, vegetation;
  QColor disabled, selected;
};

const IconPalette& iconPalette();

struct ButtonMetrics {
  int ribbonIconSize = 32;  // the mockup's 32 px icon tile
  int ribbonFontSize = 13;
  // Ribbon labels shrink one pixel at a time when even the widest chip cannot hold them, never below this.
  int ribbonMinFontSize = 12;
  // A chip is as wide as its label plus ribbonLabelPadding (mockup: 8 px) but never narrower than ribbonChipWidth;
  // a label wider than ribbonMaxLabelWidth is cut there and its full wording stays in the tooltip.
  int ribbonChipWidth = 40;
  int ribbonMinWidth = 40;
  int ribbonLabelPadding = 8;
  int ribbonMaxLabelWidth = 64;
  int buttonPadding = 1;
  int buttonSpacing = 4;
  int ribbonChipGap = 0;
  int ribbonGroupPad = 1;
  int scaleButtonHeight = 30;
  int scaleButtonMinWidth = 54;
  int scaleFontSize = 13;
  int layoutIconSize = 32;
  int layoutButtonHeight = 64;
  int panelMargin = 8;
  // Status chips (KaChip): one height and corner radius for every tone.
  int chipHeight = 22;
  int chipRadius = 11;
};

const ButtonMetrics& buttonMetrics();
// Body text size for the active display options (13 px, or 15 px for large text).
int uiFontSize();
// WCAG 2 contrast between two opaque colors: 1.0 (same) to 21.0 (black on white).
double contrastRatio(const QColor& first, const QColor& second);

QPalette palette();
QString embeddedStyleSheet();
// Replaces every known @name@ token. Unknown tokens stay as written.
QString resolvedStyleSheet(const QString& sheet);
// @name@ tokens left in a resolved sheet; empty when it is complete.
QStringList unresolvedTokens(const QString& resolved);
QStringList styleSheetCandidates();
// True when KA_HGIS_QSS_FROM_DISK=1: developers then edit data/theme/ka-hgis.qss
// without rebuilding. Otherwise a stale copy left beside an old portable build
// cannot override the sheet compiled into this executable.
bool styleSheetFromDiskRequested();
// The embedded sheet, or the first readable disk candidate in development mode.
QString loadStyleSheet();
// raw resolved, or the resolved embedded sheet (with a warning) when raw names
// a token this build does not know, e.g. a newer disk copy.
QString completeStyleSheet(const QString& raw);
// The resolved sheet apply() installs: completeStyleSheet(loadStyleSheet()).
QString applicationStyleSheet();
void apply(QApplication* app);
void excludeMapSurface(QWidget* w);
QString colorSwatchStyle(const QColor& fill);

// The Fusion-based proxy style that paints field faces, chevrons, check boxes,
// tab close crosses and the keyboard focus ring (KaThemeStyle.cpp).
QStyle* createChromeStyle();

}  // namespace KaTheme
