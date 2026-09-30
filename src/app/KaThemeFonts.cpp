// Bundled font registration and the UI/mono family stacks. KaTheme::apply()
// calls registerBundledFonts() before app->setFont(); KaThemeSheet resolves the
// @uiFont@ / @monoFont@ tokens from the stacks below.
#include "KaThemeFonts.h"
#include "KaTheme.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFontDatabase>
#include <QGuiApplication>

namespace KaTheme {
namespace {

const QString kPlexSans = QStringLiteral("IBM Plex Sans KR");
const QString kPlexMono = QStringLiteral("IBM Plex Mono");
const QString kMalgun = QStringLiteral("Malgun Gothic");
const QString kConsolas = QStringLiteral("Consolas");

struct FontState {
  bool attempted = false;  // registerBundledFonts() ran once
  int registered = 0;      // font files accepted by QFontDatabase (all calls)
  bool sans = false;       // "IBM Plex Sans KR" is available
  bool mono = false;       // "IBM Plex Mono" is available
};

// Function-local static: KaThemeSheet may resolve tokens during another
// translation unit's static initialization.
FontState& state() {
  static FontState s;
  return s;
}

// KA_HGIS_BUNDLED_FONTS=1 turns the bundled fonts on for a run, =0 forces them off;
// otherwise DisplayOptions decides (off by default, KaTheme.h).
bool bundledFontsOn() {
  const QString env = qEnvironmentVariable("KA_HGIS_BUNDLED_FONTS");
  if (env == QLatin1String("1")) return true;
  if (env == QLatin1String("0")) return false;
  return displayOptions().bundledFonts;
}

}  // namespace

QStringList bundledFontDirs() {
  const QString appDir = QCoreApplication::applicationDirPath();
  return {
      QDir(appDir).filePath(QStringLiteral("../data/fonts")),
      QDir(appDir).filePath(QStringLiteral("data/fonts")),
      QDir::current().filePath(QStringLiteral("data/fonts")),
  };
}

int registerBundledFonts(const QStringList& dirs) {
  // QFontDatabase needs a GUI application; a core-only process keeps Malgun Gothic.
  if (!qobject_cast<QGuiApplication*>(QCoreApplication::instance())) return 0;
  int added = 0;
  for (const QString& dirPath : dirs) {
    const QDir dir(dirPath);
    const QStringList files =
        dir.entryList({QStringLiteral("*.ttf"), QStringLiteral("*.otf")}, QDir::Files, QDir::Name);
    if (files.isEmpty()) continue;
    for (const QString& file : files) {
      const QString path = dir.filePath(file);
      const int id = QFontDatabase::addApplicationFont(path);
      if (id < 0) {
        qWarning().noquote() << "KaThemeFonts: could not register" << path;
        continue;
      }
      const QStringList families = QFontDatabase::applicationFontFamilies(id);
      if (families.contains(kPlexSans)) state().sans = true;
      if (families.contains(kPlexMono)) state().mono = true;
      ++added;
    }
    // The first folder with font files wins, like the style sheet candidates.
    break;
  }
  state().registered += added;
  return added;
}

int registerBundledFonts() {
  FontState& s = state();
  if (s.attempted) return s.registered;
  s.attempted = true;
  if (!bundledFontsOn()) {
    // Nothing to load: ~10 MB of TTFs would only cost start-up time while Malgun Gothic is in use.
    qInfo() << "KaThemeFonts: bundled fonts off; Malgun Gothic (KA_HGIS_BUNDLED_FONTS=1 turns them on)";
    return 0;
  }
  const int added = registerBundledFonts(bundledFontDirs());
  qInfo().noquote() << "KaThemeFonts:" << added << "bundled font file(s) registered";
  return added;
}

bool bundledFontsRegistered() { return state().sans; }

QString uiFontFamily() { return bundledFontsOn() && state().sans ? kPlexSans : kMalgun; }

QString monoFontFamily() { return bundledFontsOn() && state().mono ? kPlexMono : kConsolas; }

QStringList uiFontStack() {
  QStringList families{kMalgun, QStringLiteral("Segoe UI")};
  if (uiFontFamily() == kPlexSans) families.prepend(kPlexSans);
  return families;
}

QStringList monoFontStack() {
  QStringList families{kConsolas, QStringLiteral("D2Coding"), kMalgun};
  if (monoFontFamily() == kPlexMono) families.prepend(kPlexMono);
  return families;
}

QString quotedFamilies(const QStringList& families) {
  QStringList quoted;
  quoted.reserve(families.size());
  for (const QString& family : families) quoted << QLatin1Char('"') + family + QLatin1Char('"');
  return quoted.join(QStringLiteral(", "));
}

}  // namespace KaTheme
