#pragma once
#include <QString>
#include <QStringList>

// Bundled IBM Plex (data/fonts/*.ttf, SIL OFL). KaTheme::apply() registers the
// files once; the chrome leads with Plex only while DisplayOptions::bundledFonts
// is on and a file really came out of QFontDatabase. Malgun Gothic always follows,
// so Hangul never falls back to a Latin substitute when a family is missing.
namespace KaTheme {

// data/fonts under the same three roots as styleSheetCandidates().
QStringList bundledFontDirs();
// Registers every .ttf/.otf in the first folder of bundledFontDirs() that has
// one. Runs once per process and returns the same count afterwards. 0 without
// files, without a QGuiApplication, or with KA_HGIS_BUNDLED_FONTS=0.
int registerBundledFonts();
// The same for explicit folders (tests). Adds to the known families; never resets.
int registerBundledFonts(const QStringList& dirs);
// True once "IBM Plex Sans KR" came out of a registered file.
bool bundledFontsRegistered();
// First UI family: "IBM Plex Sans KR" when on and registered, else "Malgun Gothic".
QString uiFontFamily();
// First monospace family: "IBM Plex Mono" when on and registered, else "Consolas".
QString monoFontFamily();
// Families in order for QFont::setFamilies; Malgun Gothic sits at index 0 or 1.
QStringList uiFontStack();
// Consolas, D2Coding and Malgun Gothic, led by IBM Plex Mono when on and registered.
QStringList monoFontStack();
// "A", "B" for a QSS font-family value.
QString quotedFamilies(const QStringList& families);

}  // namespace KaTheme
