#pragma once
#include <QColor>
#include <QPalette>
#include <QString>
#include <QStringList>

class QApplication;
class QWidget;

namespace KaTheme {

struct Tokens {
  QColor sky0, sky1, sky2, sky3, sky4, sky5, sky6;
  QColor ink, inkMuted, inkDisabled, border, bevelLight, bevelDark;
  QColor canvasNeutral, desk, danger, ok;
  QColor surface, glossMiddle, glossBottom, hoverTop, hoverBottom;
  QColor pressedTop, pressedBottom, selectedTop, selectedBottom, disabledSurface;
  QColor rail, railText, railMuted, successSurface, dangerSurface;
  QColor glossReflection, glossShoulder, accentReflection;
};

const Tokens& tokens();

// Restrained function colors; labels and outlines remain dark for field use.
struct IconPalette {
  QColor ink, file, record, map, align, output;
  QColor water, earth, earthLight, rock, vegetation;
  QColor disabled, selected;
};

const IconPalette& iconPalette();

struct ButtonMetrics {
  int ribbonIconSize = 32;
  int ribbonFontSize = 13;
  int ribbonMinWidth = 64;
  int ribbonHeight = 82;
  int buttonPadding = 2;
  int buttonSpacing = 4;
  int scaleButtonHeight = 30;
  int scaleButtonMinWidth = 54;
  int scaleFontSize = 13;
  int layoutIconSize = 32;
  int layoutButtonHeight = 64;
  int panelMargin = 8;
};

const ButtonMetrics& buttonMetrics();

QPalette palette();
QString embeddedStyleSheet();
QString resolvedStyleSheet(const QString& sheet);
QStringList styleSheetCandidates();
QString resolveStyleSheetPath();
QString loadStyleSheet();
void apply(QApplication* app);
void excludeMapSurface(QWidget* w);
QString colorSwatchStyle(const QColor& fill);

}  // namespace KaTheme
