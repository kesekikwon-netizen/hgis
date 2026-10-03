#include "KaThemePaper.h"
#include "KaThemeFonts.h"

namespace KaTheme {

// Design canvas board 5 (색·글꼴·버튼): ivory paper, slate ink, clay as the one accent.
// Named assignments like strataTokens(); each contrast note is checked in test_theme_options.cpp.
void applyPaperLook(Tokens& t, bool highContrast) {
  t.accent = QColor(0xB5, 0x57, 0x3A);       // clay that carries white text, 4.8:1
  t.accentHover = QColor(0xA4, 0x4D, 0x32);
  t.accentDeep = QColor(0x8F, 0x42, 0x29);
  t.accentWash = QColor(0xF4, 0xE3, 0xDA);
  t.ink = QColor(0x14, 0x14, 0x13);
  t.inkMuted = QColor(0x5E, 0x5D, 0x59);     // >= 4.5 on every wash below
  t.inkDisabled = QColor(0x69, 0x68, 0x62);  // 4.8:1 on disabledSurface
  t.surface = QColor(0xFF, 0xFF, 0xFF);
  t.desk = QColor(0xF5, 0xF4, 0xED);
  t.altRow = QColor(0xFA, 0xF9, 0xF5);
  t.stripe = QColor(0xF7, 0xF6, 0xF0);
  t.hover = QColor(0xF3, 0xF1, 0xEA);
  t.selected = QColor(0xF4, 0xE3, 0xDA);     // the clay wash marks the chosen row
  t.pressed = QColor(0xE3, 0xE0, 0xD4);
  t.disabledSurface = QColor(0xF0, 0xEE, 0xE6);
  t.border = QColor(0xDE, 0xDC, 0xD1);
  t.borderStrong = QColor(0xC2, 0xC0, 0xB6);
  t.danger = QColor(0xA3, 0x3B, 0x3B);       // 5.0:1 on dangerSurface
  t.dangerSurface = QColor(0xF3, 0xDE, 0xDA);
  t.ok = QColor(0x3F, 0x6B, 0x31);           // 5.3:1 on successSurface
  t.successSurface = QColor(0xE6, 0xEE, 0xDD);
  t.warn = QColor(0x7A, 0x5A, 0x00);         // 5.4:1 on warnSurface
  t.warnSurface = QColor(0xF6, 0xEC, 0xC8);
  t.rail = QColor(0xF0, 0xEE, 0xE6);         // the home hero is a paper band with slate text
  t.railText = t.ink;
  t.railMuted = t.inkMuted;
  t.progressFill = QColor(0xD9, 0x77, 0x57);  // clay: ink 5.9:1 on it, 3.1:1 against white
  t.focusRing = t.accent;
  t.ribbonLabelInk = t.ink;
  t.ribbonActiveInk = QColor(0x9C, 0x4A, 0x2F);  // 5.8:1 on the paper chrome
  t.ribbonGroupInk = t.inkMuted;
  t.primary = t.ink;                         // the main button is slate, clay stays the accent
  t.primaryHover = QColor(0x30, 0x30, 0x2E);
  t.primaryPressed = QColor(0x00, 0x00, 0x00);
  t.primaryText = QColor(0xFA, 0xF9, 0xF5);
  t.chrome = QColor(0xFA, 0xF9, 0xF5);
  t.heroButton = t.primary;
  t.heroButtonText = t.primaryText;
  t.heroButtonHover = t.primaryHover;
  t.heroButtonPressed = t.primaryPressed;
  t.heroGhostHover = QColor(0xE8, 0xE6, 0xDC);
  t.heroGhostPressed = QColor(0xDE, 0xDC, 0xD1);
  t.band = t.rail;
  t.tile = QColor(0xF0, 0xEE, 0xE6);
  t.tileHover = QColor(0xE8, 0xE6, 0xDC);
  t.tileOn = QColor(0xF4, 0xE3, 0xDA);
  t.tileOnBorder = t.accent;
  t.tileDisabled = QColor(0xF5, 0xF4, 0xED);
  t.tileStrong = t.accent;
  t.glyph = QColor(0x30, 0x30, 0x2E);
  t.glyphOn = t.ribbonActiveInk;
  if (!highContrast)
    return;
  // Glare profile on paper: edges reach 3:1 on white, secondary text 7:1, washes step further apart.
  t.inkMuted = QColor(0x3D, 0x3D, 0x3A);
  t.altRow = QColor(0xF5, 0xF4, 0xED);
  t.stripe = QColor(0xF0, 0xEE, 0xE6);
  t.hover = QColor(0xF0, 0xEE, 0xE6);
  t.selected = QColor(0xEF, 0xD6, 0xC8);
  t.pressed = QColor(0xDA, 0xD7, 0xCA);
  t.border = QColor(0x87, 0x86, 0x7F);
  t.borderStrong = QColor(0x73, 0x72, 0x6C);
  t.warn = QColor(0x5C, 0x37, 0x00);
  t.railMuted = t.inkMuted;
  t.focusRing = t.accentDeep;
  t.ribbonGroupInk = t.inkMuted;
}

QString titleFontFamilies() {
  if (!displayOptions().paperLook)
    return quotedFamilies(uiFontStack());
  // Noto Serif KR where it is installed; Batang ships with Windows; Malgun Gothic always ends the list.
  return quotedFamilies({QStringLiteral("Noto Serif KR"), QStringLiteral("Batang"), QStringLiteral("Malgun Gothic")});
}

int titleFontWeight() { return displayOptions().paperLook ? 500 : 700; }

int buttonEdgeWidth() { return displayOptions().paperLook ? 1 : 2; }

}  // namespace KaTheme
