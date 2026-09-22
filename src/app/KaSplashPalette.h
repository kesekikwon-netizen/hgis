#pragma once

#include <QColor>
#include <QFont>

#include <algorithm>

// Colors and fonts shared by the startup splash. The blues are the app's glossy
// accent; sky and ink are the light text tones used on it.
namespace KaSplashPalette {

inline const QColor kTop{0x2B, 0x86, 0xC9};
inline const QColor kMid{0x12, 0x55, 0x8D};
inline const QColor kDeep{0x09, 0x2F, 0x56};
inline const QColor kInk{0xE5, 0xF2, 0xFF};
inline const QColor kSky{0xA8, 0xE7, 0xFF};

inline double clamp01(double v) { return std::clamp(v, 0.0, 1.0); }

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

// Lighter face for secondary lines; falls back to Malgun Gothic when missing.
inline QFont lightFont(double pixels) {
  QFont font(QStringLiteral("Malgun Gothic Semilight"));
  font.setFamilies({QStringLiteral("Malgun Gothic Semilight"), QStringLiteral("Malgun Gothic")});
  font.setPixelSize(std::max(9, int(pixels + 0.5)));
  return font;
}

// Latin face for the Strata wordmark.
inline QFont wordmarkFont(double pixels) {
  QFont font(QStringLiteral("Segoe UI"));
  font.setFamilies({QStringLiteral("Segoe UI"), QStringLiteral("Malgun Gothic")});
  font.setPixelSize(std::max(9, int(pixels + 0.5)));
  font.setWeight(QFont::DemiBold);
  font.setLetterSpacing(QFont::AbsoluteSpacing, pixels * 0.02);
  return font;
}

}  // namespace KaSplashPalette
