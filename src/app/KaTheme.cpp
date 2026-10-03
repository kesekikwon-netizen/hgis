#include "KaTheme.h"
#include "KaThemeFonts.h"
#include "KaThemePaper.h"

#include <QAbstractScrollArea>
#include <QApplication>
#include <QCoreApplication>
#include <QFont>
#include <QWidget>

#include <cmath>

namespace KaTheme {
namespace {

// WCAG 2 relative luminance of an sRGB color.
double relativeLuminance(const QColor& color) {
  const auto linear = [](double value) {
    return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
  };
  return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
}

QColor softenFillSaturation(const QColor& color) {
  // Keep the semantic hue and lightness; reduce only HSL saturation by 20%.
  return QColor::fromHslF(color.hslHueF(), color.hslSaturationF() * 0.8f,
                          color.lightnessF(), color.alphaF()).toRgb();
}

void fillLegacyAliases(Tokens& t) {
  t.sky0 = t.accentWash;
  t.sky1 = t.accent;
  t.sky2 = t.accentHover;
  t.sky3 = t.accentDeep;
  t.sky4 = t.desk;
  t.sky5 = t.accent;
  t.sky6 = t.ink;
  t.bevelLight = t.edgeLight;
  t.bevelDark = t.borderStrong;
  // The gloss stops stay as flat aliases so any selector naming them paints plainly.
  t.glossMiddle = t.altRow;
  t.glossBottom = t.surface;
  t.glossReflection = t.surface;
  t.glossShoulder = t.surface;
  t.accentReflection = t.accent;
  t.hoverTop = t.hoverBottom = t.hover;
  t.pressedTop = t.pressedBottom = t.pressed;
  t.selectedTop = t.selectedBottom = t.selected;
}

// Strata chrome: flat surfaces, a navy frame and one blue accent. Map symbols,
// page contents and IconPalette are separate. Named assignments, not an
// initializer list: the old positional list was easy to misorder.
Tokens strataTokens() {
  Tokens t;
  t.accent = QColor(0x1F, 0x6F, 0xB2);       // white text 5.3:1
  t.accentHover = QColor(0x18, 0x5E, 0x99);
  t.accentDeep = QColor(0x12, 0x55, 0x8D);   // the startup-notice blue
  t.accentWash = QColor(0xE3, 0xEE, 0xF8);
  t.ink = QColor(0x1D, 0x27, 0x33);
  t.inkMuted = QColor(0x5B, 0x68, 0x75);     // >= 4.5 on every wash below
  t.inkDisabled = QColor(0x59, 0x68, 0x74);  // readable on disabledSurface; icons turn grey too
  t.surface = QColor(0xFF, 0xFF, 0xFF);
  t.desk = QColor(0xF4, 0xF6, 0xF8);         // page background under white cards
  t.altRow = QColor(0xF7, 0xF9, 0xFB);
  t.stripe = QColor(0xF2, 0xF6, 0xFA);
  t.hover = QColor(0xEE, 0xF4, 0xFA);
  t.selected = QColor(0xE6, 0xF0, 0xFA);
  t.pressed = QColor(0xE1, 0xEC, 0xF7);
  t.disabledSurface = QColor(0xEE, 0xF1, 0xF4);
  t.border = QColor(0xDC, 0xE3, 0xEA);
  t.borderStrong = QColor(0xB8, 0xC4, 0xCF);
  t.edgeLight = QColor(0xFF, 0xFF, 0xFF);
  t.canvasNeutral = QColor(0xFF, 0xFF, 0xFF);
  t.danger = QColor(0xB4, 0x23, 0x18);       // 5.8:1 on dangerSurface
  t.ok = QColor(0x25, 0x6B, 0x42);           // 5.7:1 on successSurface, 6.4:1 on white (chip ink)
  t.successSurface = QColor(0xEA, 0xF4, 0xEE);
  t.dangerSurface = QColor(0xFB, 0xED, 0xEA);
  t.warn = QColor(0x7A, 0x4A, 0x00);         // 6.8:1 on warnSurface, 7.5:1 on white
  t.warnSurface = QColor(0xFF, 0xF4, 0xDB);
  t.rail = QColor(0x0B, 0x3A, 0x63);         // Strata navy
  t.railText = QColor(0xFF, 0xFF, 0xFF);
  t.railMuted = QColor(0xD5, 0xE6, 0xF5);    // >= 4.5 on rail and accentDeep
  t.progressFill = QColor(0x60, 0x99, 0xD0);  // ink 5.0:1 on it, 3.0:1 against white
  t.focusRing = t.accent;
  t.ribbonLabelInk = QColor(0x20, 0x28, 0x30);   // 14.9:1 on white, 13.5:1 on the hover wash
  t.ribbonActiveInk = QColor(0x10, 0x50, 0x88);  // 8.3:1 on white, 7.5:1 on the hover wash
  t.ribbonGroupInk = QColor(0x5E, 0x66, 0x70);   // 5.8:1 on white
  t.primary = t.accent;
  t.primaryHover = t.accentHover;
  t.primaryPressed = t.accentDeep;
  t.primaryText = t.surface;
  t.chrome = t.surface;
  t.heroButton = t.surface;
  t.heroButtonText = t.rail;
  t.heroButtonHover = t.hover;
  t.heroButtonPressed = t.pressed;
  t.heroGhostHover = t.accentDeep;
  t.heroGhostPressed = t.rail;
  t.band = t.accentDeep;
  t.tile = QColor(0xE4, 0xEA, 0xED);
  t.tileHover = QColor(0xD9, 0xE2, 0xE7);
  t.tileOn = QColor(0xE0, 0xEC, 0xF8);
  t.tileOnBorder = QColor(0x1A, 0x68, 0xB0);
  t.tileDisabled = QColor(0xEE, 0xF1, 0xF3);
  t.tileStrong = QColor(0x20, 0x6C, 0xB0);
  t.glyph = QColor(0x2B, 0x48, 0x58);
  t.glyphOn = QColor(0x10, 0x50, 0x88);
  fillLegacyAliases(t);
  return t;
}

// Opt-in profile for glare: panel edges reach 3:1 against white, secondary text
// 7:1, and the state washes step further apart. Accent and surfaces stay.
Tokens highContrastTokens() {
  Tokens t = strataTokens();
  t.inkMuted = QColor(0x3E, 0x4A, 0x57);
  t.altRow = QColor(0xF0, 0xF3, 0xF6);
  t.stripe = QColor(0xE8, 0xEE, 0xF4);
  t.hover = QColor(0xE6, 0xF0, 0xFA);
  t.selected = QColor(0xD6, 0xE6, 0xF6);
  t.pressed = QColor(0xCC, 0xDF, 0xF2);
  t.border = QColor(0x7D, 0x8A, 0x96);
  t.borderStrong = QColor(0x6B, 0x78, 0x85);
  t.warn = QColor(0x5C, 0x37, 0x00);         // 9.2:1 on warnSurface
  t.warnSurface = QColor(0xFF, 0xEF, 0xC7);
  t.focusRing = t.accentDeep;
  t.ribbonGroupInk = t.inkMuted;
  fillLegacyAliases(t);
  return t;
}

// Function-local statics: other translation units may ask for tokens during
// their own static initialization.
DisplayOptions& activeOptions() {
  static DisplayOptions options;
  return options;
}

Tokens& activeTokens() {
  static Tokens active = strataTokens();
  return active;
}

void setGroup(QPalette& pal, QPalette::ColorGroup g, const Tokens& t, bool disabled) {
  const QColor text = disabled ? t.inkDisabled : t.ink;
  pal.setColor(g, QPalette::Window, t.desk);
  pal.setColor(g, QPalette::WindowText, text);
  pal.setColor(g, QPalette::Base, disabled ? t.disabledSurface : t.surface);
  pal.setColor(g, QPalette::AlternateBase, t.altRow);
  pal.setColor(g, QPalette::Text, text);
  pal.setColor(g, QPalette::Button, disabled ? t.disabledSurface : t.surface);
  pal.setColor(g, QPalette::ButtonText, text);
  pal.setColor(g, QPalette::BrightText, text);
  pal.setColor(g, QPalette::Highlight, disabled ? t.disabledSurface : t.accent);
  pal.setColor(g, QPalette::HighlightedText, disabled ? t.inkDisabled : t.surface);
  pal.setColor(g, QPalette::PlaceholderText, disabled ? t.inkDisabled : t.inkMuted);
  pal.setColor(g, QPalette::ToolTipBase, t.surface);
  pal.setColor(g, QPalette::ToolTipText, text);
  pal.setColor(g, QPalette::Light, t.edgeLight);
  pal.setColor(g, QPalette::Midlight, t.altRow);
  pal.setColor(g, QPalette::Mid, t.border);
  pal.setColor(g, QPalette::Dark, t.borderStrong);
  pal.setColor(g, QPalette::Shadow, t.border);
}

}  // namespace

const Tokens& tokens() { return activeTokens(); }

Tokens tokensFor(const DisplayOptions& options) {
  Tokens t = options.highContrast ? highContrastTokens() : strataTokens();
  if (options.paperLook) {
    applyPaperLook(t, options.highContrast);
    fillLegacyAliases(t);
  }
  return t;
}

const DisplayOptions& displayOptions() { return activeOptions(); }

int uiFontSize() { return activeOptions().largeText ? 15 : 13; }

double contrastRatio(const QColor& first, const QColor& second) {
  const double a = relativeLuminance(first);
  const double b = relativeLuminance(second);
  return (qMax(a, b) + 0.05) / (qMin(a, b) + 0.05);
}

const IconPalette& iconPalette() {
  static const IconPalette colors = [] {
    IconPalette palette = {
      QColor(0x23, 0x29, 0x30),  // ink: common charcoal outline
      QColor(0x32, 0x6B, 0x9B),  // file: steel blue
      QColor(0x95, 0x60, 0x29),  // record: ochre
      QColor(0x39, 0x73, 0x4C),  // map: green (hue 140, apart from the teal output)
      QColor(0x6B, 0x59, 0x96),  // align: muted violet
      QColor(0x24, 0x70, 0x78),  // output: teal (hue 186)
      QColor(0x1D, 0x6E, 0xB8),  // water: river blue
      QColor(0x93, 0x60, 0x39),  // earth: soil brown
      QColor(0xD8, 0xBB, 0x7B),  // earthLight: sandy layer
      QColor(0x79, 0x6B, 0x62),  // rock: warm stone
      QColor(0x71, 0x82, 0x50),  // vegetation: muted olive
      QColor(0x8F, 0x98, 0xA3),  // disabled: neutralized in icon rendering
      QColor(0x16, 0x3F, 0x59),  // selected: dark blue accent
    };
    // Explicit assignments: see the MSVC note in KaThemeSheet.cpp replacementTable().
    palette.file = softenFillSaturation(palette.file);
    palette.record = softenFillSaturation(palette.record);
    palette.map = softenFillSaturation(palette.map);
    palette.align = softenFillSaturation(palette.align);
    palette.output = softenFillSaturation(palette.output);
    palette.water = softenFillSaturation(palette.water);
    palette.earth = softenFillSaturation(palette.earth);
    palette.earthLight = softenFillSaturation(palette.earthLight);
    palette.rock = softenFillSaturation(palette.rock);
    palette.vegetation = softenFillSaturation(palette.vegetation);
    // Keep ink, disabled and selected-outline contrast exactly as before.
    return palette;
  }();
  return colors;
}

const ButtonMetrics& buttonMetrics() {
  static const ButtonMetrics metrics;
  return metrics;
}

QPalette palette() {
  QPalette pal;
  setGroup(pal, QPalette::Active, tokens(), false);
  setGroup(pal, QPalette::Inactive, tokens(), false);
  setGroup(pal, QPalette::Disabled, tokens(), true);
  return pal;
}

void apply(QApplication* app) {
  if (!app)
    return;
  // Field PCs ship Malgun Gothic. The bundled IBM Plex (data/fonts) leads only
  // once its files are registered; Malgun Gothic always follows in the stack so
  // Hangul never falls back to a Latin substitute (KaThemeFonts.h).
  registerBundledFonts();
  QFont ui;
  ui.setFamilies(uiFontStack());
  ui.setPixelSize(uiFontSize());
  ui.setHintingPreference(QFont::PreferFullHinting);
  ui.setStyleStrategy(QFont::PreferAntialias);
  app->setFont(ui);
  app->setStyle(createChromeStyle());
  app->setPalette(palette());
  app->setStyleSheet(applicationStyleSheet());
}

void setDisplayOptions(QApplication* app, const DisplayOptions& options) {
  activeOptions() = options;
  activeTokens() = tokensFor(options);
  if (!app)
    return;
  QFont ui = app->font();
  ui.setFamilies(uiFontStack());
  ui.setPixelSize(uiFontSize());
  app->setFont(ui);
  app->setPalette(palette());
  app->setStyleSheet(applicationStyleSheet());
}

void excludeMapSurface(QWidget* w) {
  if (!w)
    return;
  // Clears the *local* sheet only. Application QSS still applies; GIS exclude
  // selectors in ka-hgis.qss are the real protection.
  w->setStyleSheet(QString());
  w->setAttribute(Qt::WA_StyledBackground, false);
  if (auto* area = qobject_cast<QAbstractScrollArea*>(w)) {
    if (QWidget* vp = area->viewport()) {
      vp->setStyleSheet(QString());
      vp->setAttribute(Qt::WA_StyledBackground, false);
    }
  }
}

QString colorSwatchStyle(const QColor& fill) {
  const QColor use = fill.isValid() ? fill : tokens().surface;
  return QStringLiteral("background-color: %1; border: 1px solid %2; border-radius: 8px;")
      .arg(use.name(), tokens().border.name());
}

}  // namespace KaTheme
