// P1 design-system: one chip shape for every tone, the warn token pair, field
// contrast, the appended QSS tokens and the bundled-font connection point.
// Runs without QGIS. Chip pixels come from KaChip::paintChip and the widget.
#include <QtTest>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QRegularExpression>
#include <QTemporaryDir>
#include "app/KaChip.h"
#include "app/KaTheme.h"
#include "app/KaThemeFonts.h"

namespace {

using Tone = KaChip::Tone;
const Tone kTones[] = {Tone::Ok, Tone::Warn, Tone::Danger, Tone::Accent, Tone::Neutral};
const QString kPlexSans = QStringLiteral("IBM Plex Sans KR");
const QString kPlexMono = QStringLiteral("IBM Plex Mono");
const QString kMalgun = QStringLiteral("Malgun Gothic");

QImage renderChip(const QString& text, Tone tone, const QString& glyph) {
  const bool hasGlyph = !glyph.isEmpty() || !KaChip::defaultGlyph(tone).isEmpty();
  const QSize size = KaChip::sizeForText(QFontMetrics(KaChip::chipFont(qApp->font())), text, hasGlyph);
  QImage image(size, QImage::Format_ARGB32);
  image.fill(Qt::transparent);
  QPainter p(&image);
  p.setFont(qApp->font());
  KaChip::paintChip(p, QRect(QPoint(0, 0), size), text, tone, glyph);
  return image;
}

// Pixels in zone painted (nearly) in ink: text and glyph strokes.
int inkPixels(const QImage& image, const QRect& zone, const QColor& ink) {
  int count = 0;
  for (int y = zone.top(); y <= zone.bottom(); ++y) {
    for (int x = zone.left(); x <= zone.right(); ++x) {
      if (!image.rect().contains(x, y)) continue;
      const QColor c = image.pixelColor(x, y);
      const int distance = qAbs(c.red() - ink.red()) + qAbs(c.green() - ink.green()) + qAbs(c.blue() - ink.blue());
      if (c.alpha() >= 200 && distance <= 90) ++count;
    }
  }
  return count;
}

bool sameAlphaMask(const QImage& a, const QImage& b) {
  if (a.size() != b.size()) return false;
  for (int y = 0; y < a.height(); ++y)
    for (int x = 0; x < a.width(); ++x)
      if (qAbs(a.pixelColor(x, y).alpha() - b.pixelColor(x, y).alpha()) > 2) return false;
  return true;
}

}  // namespace

class TestChipTheme : public QObject {
  Q_OBJECT
 private slots:
  void initTestCase();
  void fonts_malgunUntilRegistered();
  void fonts_registerWhenPresent();
  void fonts_optionOffFallsBackToMalgun();
  void fonts_applyInstallsFamilies();
  void tokens_chipInksReachFieldContrast();
  void chip_oneShapeAllTones();
  void chip_paintChipMatchesWidget();
  void chip_hasGlyphAndText();
  void chip_widgetPropertiesAndSize();
  void qss_newTokensResolve();
  void qss_windowFaceStillLeadsWithMalgun();
  void qss_lateFontOverrideIsLast();
};

void TestChipTheme::initTestCase() {
#ifdef Q_OS_WIN
  if (QGuiApplication::platformName() == QLatin1String("offscreen")) {
    // The offscreen platform does not discover Windows system fonts; chip text needs the Korean face.
    const QDir windows(qEnvironmentVariable("WINDIR"));
    for (const QString& file : {QStringLiteral("malgun.ttf"), QStringLiteral("malgunbd.ttf")}) {
      const QString path = windows.filePath(QStringLiteral("Fonts/") + file);
      QVERIFY2(QFontDatabase::addApplicationFont(path) >= 0, qPrintable(path));
    }
  }
#endif
  // No KaTheme::apply() here: the first font test observes the state before it.
}

void TestChipTheme::fonts_malgunUntilRegistered() {
  QVERIFY2(!KaTheme::DisplayOptions().bundledFonts, "off by default: Plex read softer than Malgun (KaTheme.h)");
  QVERIFY(!KaTheme::bundledFontsRegistered());
  QCOMPARE(KaTheme::uiFontFamily(), kMalgun);
  QCOMPARE(KaTheme::monoFontFamily(), QStringLiteral("Consolas"));
  QCOMPARE(KaTheme::uiFontStack().first(), kMalgun);
  QTemporaryDir empty;
  QVERIFY(empty.isValid());
  QCOMPARE(KaTheme::registerBundledFonts({empty.path()}), 0);
  QCOMPARE(KaTheme::uiFontFamily(), kMalgun);
  QCOMPARE(KaTheme::quotedFamilies({kPlexSans, kMalgun}), QStringLiteral("\"IBM Plex Sans KR\", \"Malgun Gothic\""));
}

void TestChipTheme::fonts_registerWhenPresent() {
  const QString sans = QStringLiteral("data/fonts/IBMPlexSansKR-Regular.ttf");
  const QString mono = QStringLiteral("data/fonts/IBMPlexMono-Regular.ttf");
  if (!QFile::exists(sans) || !QFile::exists(mono)) QSKIP("data/fonts has no IBM Plex files in this checkout");
  QTemporaryDir folder;
  QVERIFY(folder.isValid());
  QVERIFY(QFile::copy(sans, QDir(folder.path()).filePath(QStringLiteral("sans.ttf"))));
  QVERIFY(QFile::copy(mono, QDir(folder.path()).filePath(QStringLiteral("mono.ttf"))));
  QCOMPARE(KaTheme::registerBundledFonts({folder.path()}), 2);
  QVERIFY(KaTheme::bundledFontsRegistered());
  QCOMPARE(KaTheme::uiFontFamily(), kMalgun);  // registration alone does not switch the family
  KaTheme::DisplayOptions on; on.bundledFonts = true;
  KaTheme::setDisplayOptions(nullptr, on);  // opt in for the rest of this test
  QVERIFY(QFontDatabase::families().contains(kPlexSans));
  QVERIFY(QFontDatabase::families().contains(kPlexMono));
  QCOMPARE(KaTheme::uiFontFamily(), kPlexSans);
  QCOMPARE(KaTheme::monoFontFamily(), kPlexMono);
  // Malgun Gothic always follows Plex: Hangul fallback stays Korean.
  QCOMPARE(KaTheme::uiFontStack().first(), kPlexSans);
  QCOMPARE(KaTheme::uiFontStack().indexOf(kMalgun), 1);
  QCOMPARE(KaTheme::monoFontStack().first(), kPlexMono);
  KaTheme::setDisplayOptions(nullptr, KaTheme::DisplayOptions());  // back to the default (off)
}

void TestChipTheme::fonts_optionOffFallsBackToMalgun() {
  KaTheme::DisplayOptions off;
  off.bundledFonts = false;
  KaTheme::setDisplayOptions(nullptr, off);
  QCOMPARE(KaTheme::uiFontFamily(), kMalgun);
  QCOMPARE(KaTheme::uiFontStack(), (QStringList{kMalgun, QStringLiteral("Segoe UI")}));
  const QString resolved = KaTheme::resolvedStyleSheet(KaTheme::embeddedStyleSheet());
  QVERIFY(resolved.contains(QStringLiteral("font-family: \"Malgun Gothic\", \"Segoe UI\"; }")));
  QVERIFY(!resolved.contains(kPlexSans));
  KaTheme::setDisplayOptions(nullptr, KaTheme::DisplayOptions());
  QCOMPARE(KaTheme::displayOptions(), KaTheme::DisplayOptions());
}

void TestChipTheme::fonts_applyInstallsFamilies() {
  KaTheme::apply(qApp);
  const QStringList families = qApp->font().families();
  QCOMPARE(families.first(), kMalgun);  // default profile leads with Malgun even after Plex registered
  QCOMPARE(families.first(), KaTheme::uiFontFamily());
  QVERIFY2(qApp->styleSheet().contains(QStringLiteral("font-family: \"Malgun Gothic\", \"Segoe UI\"; }")),
           "the appended override resolves to Malgun Gothic while the option is off");
  QVERIFY(!qApp->styleSheet().contains(QStringLiteral("font-family: \"IBM Plex Sans KR\"")));
  QVERIFY(KaTheme::unresolvedTokens(qApp->styleSheet()).isEmpty());
}

void TestChipTheme::tokens_chipInksReachFieldContrast() {
  for (const bool highContrast : {false, true}) {
    KaTheme::DisplayOptions options;
    options.highContrast = highContrast;
    const KaTheme::Tokens t = KaTheme::tokensFor(options);
    const char* profile = highContrast ? "high contrast" : "stock";
    // Status tones carry meaning: 5.5:1 or better on their own wash.
    QVERIFY2(KaTheme::contrastRatio(t.ok, t.successSurface) >= 5.5, profile);
    QVERIFY2(KaTheme::contrastRatio(t.warn, t.warnSurface) >= 5.5, profile);
    QVERIFY2(KaTheme::contrastRatio(t.danger, t.dangerSurface) >= 5.5, profile);
    QVERIFY2(KaTheme::contrastRatio(t.accentDeep, t.accentWash) >= 5.5, profile);
    // Neutral says nothing by colour; the stock pair sits at 5.4:1 (design §5.1), AA is 4.5.
    QVERIFY2(KaTheme::contrastRatio(t.inkMuted, t.altRow) >= 4.5, profile);
    QVERIFY2(KaTheme::contrastRatio(t.warn, t.surface) >= 7.0, "warn on white");
  }
  QCOMPARE(KaTheme::tokens().warn, QColor(0x7A, 0x4A, 0x00));
  QCOMPARE(KaTheme::tokens().warnSurface, QColor(0xFF, 0xF4, 0xDB));
}

void TestChipTheme::chip_oneShapeAllTones() {
  const QString text = QStringLiteral("저장됨");
  const QImage reference = renderChip(text, Tone::Ok, QStringLiteral("check"));
  QCOMPARE(reference.height(), 22);
  // The pill fills its full height, reaches the left edge at mid height and rounds the corner.
  QVERIFY(reference.pixelColor(reference.width() / 2, 0).alpha() > 0);
  QVERIFY(reference.pixelColor(reference.width() / 2, 21).alpha() > 0);
  QVERIFY(reference.pixelColor(0, 11).alpha() > 0);
  QCOMPARE(reference.pixelColor(0, 0).alpha(), 0);
  QImage sheet(qMax(reference.width() + 16, 240), 5 * 30 + 36, QImage::Format_ARGB32);
  sheet.fill(Qt::white);
  QPainter composer(&sheet);
  int row = 0;
  for (const Tone tone : kTones) {
    const QImage image = renderChip(text, tone, QStringLiteral("check"));
    QVERIFY2(sameAlphaMask(reference, image), qPrintable(KaChip::toneName(tone)));
    QCOMPARE(image.pixelColor(image.width() - 5, 11), KaChip::toneSurface(tone));
    composer.drawImage(8, 8 + row++ * 30, image);
  }
  // Coordinate readout in the mono stack (IBM Plex Mono when registered) for the same capture.
  QFont mono;
  mono.setFamilies(KaTheme::monoFontStack());
  mono.setPixelSize(12);
  composer.setFont(mono);
  const QString xy = QStringLiteral("X 147,999.305  Y 98,157.620");
  composer.drawText(QRect(8, 5 * 30 + 10, sheet.width() - 16, 20), Qt::AlignLeft | Qt::AlignVCenter, xy);
  qInfo().noquote() << "mono readout" << KaTheme::monoFontFamily() << "width" << QFontMetrics(mono).horizontalAdvance(xy);
  composer.end();
  const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
  if (!output.isEmpty() && QDir(output).exists()) {
    const QString path = QDir(output).filePath(QStringLiteral("chip-tones.png"));
    QVERIFY2(sheet.save(path), qPrintable(path));
    qInfo().noquote() << "Automatic chip render; not a portable field screenshot:" << path;
  }
}

void TestChipTheme::chip_paintChipMatchesWidget() {
  const QString text = QStringLiteral("저장됨");
  KaChip chip(text, Tone::Ok);
  chip.setGlyph(QStringLiteral("check"));
  chip.resize(chip.sizeHint());
  const QImage widget = chip.grab().toImage().convertToFormat(QImage::Format_ARGB32);
  const QImage painted = renderChip(text, Tone::Ok, QStringLiteral("check"));
  QCOMPARE(widget.size(), painted.size());
  const QPoint fill(widget.width() - 5, 11);
  QCOMPARE(widget.pixelColor(fill), KaChip::toneSurface(Tone::Ok));
  QCOMPARE(painted.pixelColor(fill), KaChip::toneSurface(Tone::Ok));
  const QRect textZone(24, 0, widget.width() - 32, 22);
  QVERIFY(inkPixels(widget, textZone, KaChip::toneInk(Tone::Ok)) > 0);
  QVERIFY(inkPixels(painted, textZone, KaChip::toneInk(Tone::Ok)) > 0);
}

void TestChipTheme::chip_hasGlyphAndText() {
  const QString text = QStringLiteral("저장 안 됨");
  const QRect glyphZone(8, 4, 14, 14);
  for (const Tone tone : {Tone::Ok, Tone::Warn, Tone::Danger}) {
    // No id given: the tone's own mark appears, never a bare coloured pill.
    const QImage image = renderChip(text, tone, QString());
    const QRect textZone(24, 0, image.width() - 32, 22);
    QVERIFY2(inkPixels(image, glyphZone, KaChip::toneInk(tone)) >= 3, qPrintable(KaChip::toneName(tone)));
    QVERIFY2(inkPixels(image, textZone, KaChip::toneInk(tone)) >= 10, qPrintable(KaChip::toneName(tone)));
  }
  // Accent and neutral may be text only: 16 px narrower, text starts at 8.
  QVERIFY(KaChip::defaultGlyph(Tone::Accent).isEmpty() && KaChip::defaultGlyph(Tone::Neutral).isEmpty());
  const QFontMetrics fm(KaChip::chipFont(qApp->font()));
  QCOMPARE(KaChip::sizeForText(fm, text, true).width() - KaChip::sizeForText(fm, text, false).width(), 16);
  const QImage neutral = renderChip(text, Tone::Neutral, QString());
  QVERIFY(inkPixels(neutral, QRect(8, 0, neutral.width() - 16, 22), KaChip::toneInk(Tone::Neutral)) >= 10);
  // A built-in mark also draws when KaIcons has no glyph for the id.
  const QImage builtIn = renderChip(text, Tone::Accent, QStringLiteral("dot"));
  QVERIFY(inkPixels(builtIn, glyphZone, KaChip::toneInk(Tone::Accent)) >= 3);
}

void TestChipTheme::chip_widgetPropertiesAndSize() {
  KaChip chip(QStringLiteral("저장됨"), Tone::Ok);
  QCOMPARE(chip.property("kaChip").toString(), QStringLiteral("true"));
  QCOMPARE(chip.property("kaTone").toString(), QStringLiteral("ok"));
  QCOMPARE(chip.property("kaGlyph").toString(), QStringLiteral("true"));
  QVERIFY(chip.hasGlyph());
  QCOMPARE(chip.sizeHint().height(), 22);
  chip.setTone(Tone::Warn);
  QCOMPARE(chip.property("kaTone").toString(), QStringLiteral("warn"));
  KaChip count(QStringLiteral("3개 선택"), Tone::Neutral);
  QVERIFY(!count.hasGlyph());
  QCOMPARE(count.property("kaGlyph").toString(), QStringLiteral("false"));
  QCOMPARE(count.property("kaTone").toString(), QStringLiteral("neutral"));
  for (const Tone tone : kTones) QCOMPARE(KaChip::toneFromName(KaChip::toneName(tone)), tone);
}

void TestChipTheme::qss_newTokensResolve() {
  const QString raw = KaTheme::embeddedStyleSheet();
  for (const char* token : {"@chipHeight@", "@chipRadius@", "@uiFont@", "@monoFont@"})
    QVERIFY2(raw.contains(QLatin1String(token)), token);
  const QString resolved = KaTheme::resolvedStyleSheet(raw);
  QVERIFY(KaTheme::unresolvedTokens(resolved).isEmpty());
  const QRegularExpression chipRule(
      QStringLiteral("QLabel\\[kaChip=\"true\"\\]\\s*\\{[^}]*min-height:\\s*22px;[^}]*max-height:\\s*22px;"
                     "[^}]*border-radius:\\s*11px;[^}]*font-size:\\s*12px;[^}]*font-weight:\\s*700;"));
  QVERIFY2(chipRule.match(resolved).hasMatch(), "chip rule gives only size and font");
  QVERIFY(resolved.contains(QStringLiteral("QLabel[kaChip=\"true\"][kaGlyph=\"false\"] { padding-left: 8px; }")));
  QVERIFY(KaTheme::unresolvedTokens(KaTheme::resolvedStyleSheet(QStringLiteral("@warn@ @warnSurface@"))).isEmpty());
}

void TestChipTheme::qss_windowFaceStillLeadsWithMalgun() {
  // Companion of test_theme.cpp chromeFontIsFieldKorean: the window rule is untouched.
  const QString qss = KaTheme::embeddedStyleSheet();
  const QRegularExpression face(QStringLiteral("QMainWindow,[\\s\\S]*?font-family:\\s*\"Malgun Gothic\""));
  QVERIFY(face.match(qss).hasMatch());
}

void TestChipTheme::qss_lateFontOverrideIsLast() {
  const QString qss = KaTheme::embeddedStyleSheet();
  const int uiOverride = qss.indexOf(QStringLiteral("font-family: @uiFont@"));
  const int monoOverride = qss.indexOf(QStringLiteral("font-family: @monoFont@"));
  QVERIFY(uiOverride > 0 && monoOverride > 0);
  // Every literal family rule sits above the overrides, so the later rule wins.
  QVERIFY(qss.lastIndexOf(QStringLiteral("font-family: \"Malgun Gothic\"")) < uiOverride);
  QVERIFY(qss.lastIndexOf(QStringLiteral("font-family: \"Consolas\"")) < monoOverride);
  const QRegularExpression window(
      QStringLiteral("QMainWindow, QDialog, QMessageBox, QInputDialog, QColorDialog, QDialogButtonBox, "
                     "QWidget#startPage,\\s*QLabel#recentEmptyHint \\{ font-family: @uiFont@; \\}"));
  QVERIFY2(window.match(qss).hasMatch(), "the override repeats the window selector list");
  QVERIFY(qss.contains(QStringLiteral("QLabel#xyReadout, QLabel#kaMeasureValue { font-family: @monoFont@; }")));
}

QTEST_MAIN(TestChipTheme)
#include "test_chip_theme.moc"
