#pragma once

#include <QColor>
#include <QFont>

#include <algorithm>

// Colors, fonts and easing shared by the startup splash. The blue chrome matches
// the splash card and app accent; the earth tones reuse KaTheme's icon palette
// (soil, ochre, sand, stone) so the soil reads as the same material as the icons.
namespace KaSplashPalette {

inline const QColor kInk{0xE5, 0xF2, 0xFF};
inline const QColor kSky{0xA8, 0xE7, 0xFF};
inline const QColor kLine{182, 225, 255};
inline const QColor kGold{0xD9, 0xA9, 0x3A};
inline const QColor kSand{0xD8, 0xBB, 0x7B};
inline const QColor kSoil{0x93, 0x60, 0x39};
inline const QColor kOchre{0x95, 0x60, 0x29};
inline const QColor kStone{0x79, 0x6B, 0x62};
inline const QColor kLoam{0xA6, 0x74, 0x45};
inline const QColor kPlanShade{6, 34, 64, 120};

inline double clamp01(double v) { return std::clamp(v, 0.0, 1.0); }

inline double easeOut(double v) {
  const double t = 1.0 - clamp01(v);
  return 1.0 - t * t * t;
}

inline QColor withAlpha(QColor color, double alpha) {
  color.setAlphaF(float(clamp01(alpha)));
  return color;
}

inline QFont uiFont(double pixels, bool bold = false) {
  QFont font(QStringLiteral("Malgun Gothic"));
  font.setPixelSize(std::max(9, int(pixels + 0.5)));
  font.setBold(bold);
  return font;
}

inline QFont monoFont(double pixels) {
  QFont font(QStringLiteral("Consolas"));
  font.setStyleHint(QFont::Monospace);
  font.setPixelSize(std::max(9, int(pixels + 0.5)));
  return font;
}

}  // namespace KaSplashPalette
