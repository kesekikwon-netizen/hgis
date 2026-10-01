// Theme contracts added by the Strata evaluation: semantic tokens (F192), the
// embedded-first loader (F193), hover lighter than selection (F092) and the
// opt-in 고대비/큰 글씨 profile (F153/F154). The stock look stays the default.
// Rendered checks live in test_theme_render.cpp.
#include <QtTest>
#include <cmath>
#include <QApplication>
#include <QAction>
#include <QDir>
#include <QFontDatabase>
#include <QMenu>
#include <QRegularExpression>
#include <QSettings>
#include <QTemporaryDir>
#include "app/KaTheme.h"
#include "app/KaThemeOptions.h"

namespace {

double luminance(const QColor& c) {
  const auto lin = [](double v) { return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
  return 0.2126 * lin(c.redF()) + 0.7152 * lin(c.greenF()) + 0.0722 * lin(c.blueF());
}

double contrast(const QColor& a, const QColor& b) {
  const double x = luminance(a), y = luminance(b);
  return (qMax(x, y) + 0.05) / (qMin(x, y) + 0.05);
}

}  // namespace

class TestThemeOptions : public QObject {
  Q_OBJECT
private slots:
  void initTestCase();
  void cleanup();
  void semanticTokensKeepLegacyAliases();
  void stateWashesStepLightToStrong();
  void highContrastIsOptInAndStronger();
  void largeTextOnlyGrowsBodyText();
  void displayMenuSavesAndRestyles();
  void loaderPrefersEmbeddedAndRejectsUnknownTokens();
  void sheetHygiene();

private:
  QTemporaryDir m_settings;
};

void TestThemeOptions::initTestCase() {
#ifdef Q_OS_WIN
  const QDir windows(qEnvironmentVariable("WINDIR", QStringLiteral("C:/Windows")));
  for (const QString& file : {QStringLiteral("malgun.ttf"), QStringLiteral("malgunbd.ttf")})
    QFontDatabase::addApplicationFont(windows.filePath(QStringLiteral("Fonts/") + file));
#endif
  // The display menu saves its ticks: keep them in a throwaway folder.
  QVERIFY(m_settings.isValid());
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
  QCoreApplication::setOrganizationName(QStringLiteral("ka-hgis-theme-tests"));
  QCoreApplication::setApplicationName(QStringLiteral("theme-options"));
  qunsetenv("KA_HGIS_QSS_FROM_DISK");
  KaTheme::apply(qApp);
}

void TestThemeOptions::cleanup() {
  KaTheme::setDisplayOptions(qApp, KaTheme::DisplayOptions());
}

void TestThemeOptions::semanticTokensKeepLegacyAliases() {
  const auto& t = KaTheme::tokens();
  QCOMPARE(t.accent, QColor(0x1F, 0x6F, 0xB2));
  QCOMPARE(t.sky1, t.accent);
  QCOMPARE(t.sky2, t.accentHover);
  QCOMPARE(t.sky3, t.accentDeep);
  QCOMPARE(t.sky4, t.desk);
  QCOMPARE(t.bevelDark, t.borderStrong);
  QCOMPARE(t.glossMiddle, t.altRow);
  QCOMPARE(t.hoverBottom, t.hover);
  QCOMPARE(t.selectedTop, t.selected);
  QCOMPARE(t.pressedTop, t.pressed);
  QCOMPARE(t.focusRing, t.accent);
  QCOMPARE(t.stripe, QColor(0xF2, 0xF6, 0xFA));  // the layer list's stripes stay as they were
}

// F092: hover < selected < pressed, and every wash keeps both text inks readable.
void TestThemeOptions::stateWashesStepLightToStrong() {
  for (const bool high : {false, true}) {
    KaTheme::DisplayOptions options;
    options.highContrast = high;
    const KaTheme::Tokens t = KaTheme::tokensFor(options);
    QVERIFY(luminance(t.hover) > luminance(t.selected));
    QVERIFY(luminance(t.selected) > luminance(t.pressed));
    for (const QColor& wash : {t.hover, t.selected, t.pressed, t.stripe, t.altRow}) {
      QVERIFY(contrast(t.ink, wash) >= 4.5);
      QVERIFY(contrast(t.inkMuted, wash) >= 4.5);
    }
    QVERIFY(contrast(t.inkDisabled, t.disabledSurface) >= 4.5);
    // Ribbon inks: group names on the toolbar, the chosen tool's label on the toolbar and on the hover wash.
    QVERIFY2(contrast(t.ribbonGroupInk, t.surface) >= 4.5, "ribbon group names");
    QVERIFY2(contrast(t.ribbonActiveInk, t.surface) >= 4.5, "chosen ribbon label");
    QVERIFY2(contrast(t.ribbonActiveInk, t.hover) >= 4.5, "chosen ribbon label on hover");
    QVERIFY2(contrast(t.ink, t.progressFill) >= 4.5, "progress text on the chunk");
    QVERIFY2(contrast(t.progressFill, t.surface) >= 3.0, "progress chunk against the groove");
  }
  const QString sheet = KaTheme::embeddedStyleSheet();
  QVERIFY2(QRegularExpression(QStringLiteral(R"re(::item:selected:hover[^{]*\{\s*background:\s*@selected@)re"))
               .match(sheet).hasMatch(), "a hovered selected row keeps the selection colour");
  QVERIFY2(!QRegularExpression(QStringLiteral(R"re(::item:hover\s*\{[^}]*@pressed)re")).match(sheet).hasMatch(),
           "row hover must not use the pressed wash");
}

void TestThemeOptions::highContrastIsOptInAndStronger() {
  QCOMPARE(KaTheme::displayOptions(), KaTheme::DisplayOptions());
  const KaTheme::Tokens stock = KaTheme::tokens();
  QCOMPARE(stock.border, QColor(0xDC, 0xE3, 0xEA));
  KaTheme::DisplayOptions high;
  high.highContrast = true;
  KaTheme::setDisplayOptions(qApp, high);
  const KaTheme::Tokens t = KaTheme::tokens();  // a copy: the active set changes below
  QVERIFY(contrast(t.border, t.surface) >= 3.0);
  QVERIFY(contrast(t.inkMuted, t.surface) >= 7.0);
  QCOMPARE(t.accent, stock.accent);
  QCOMPARE(t.surface, stock.surface);
  QVERIFY(qApp->styleSheet().contains(t.border.name()));
  KaTheme::setDisplayOptions(qApp, KaTheme::DisplayOptions());
  QCOMPARE(KaTheme::tokens().border, stock.border);
  QVERIFY(!qApp->styleSheet().contains(t.border.name()));
}

void TestThemeOptions::largeTextOnlyGrowsBodyText() {
  QCOMPARE(KaTheme::uiFontSize(), 13);
  KaTheme::DisplayOptions large;
  large.largeText = true;
  KaTheme::setDisplayOptions(qApp, large);
  QCOMPARE(KaTheme::uiFontSize(), 15);
  QCOMPARE(qApp->font().pixelSize(), 15);
  QVERIFY(qApp->styleSheet().contains(QStringLiteral("font-size: 15px")));
  // Chip size, ribbon labels and the 10 px layer list are fixed user decisions.
  QCOMPARE(KaTheme::buttonMetrics().ribbonFontSize, 13);
  QCOMPARE(KaTheme::buttonMetrics().ribbonChipWidth, 40);
  QVERIFY(qApp->styleSheet().contains(QStringLiteral("font-size: 10px")));
  KaTheme::setDisplayOptions(qApp, KaTheme::DisplayOptions());
  QCOMPARE(qApp->font().pixelSize(), 13);
}

void TestThemeOptions::displayMenuSavesAndRestyles() {
  QCOMPARE(KaTheme::savedDisplayOptions(), KaTheme::DisplayOptions());
  QWidget host;
  QMenu* menu = KaTheme::createDisplayOptionsMenu(&host);
  auto* contrast = menu->findChild<QAction*>(QStringLiteral("actionHighContrast"));
  auto* large = menu->findChild<QAction*>(QStringLiteral("actionLargeText"));
  QVERIFY(contrast && large && contrast->isCheckable() && large->isCheckable());
  QVERIFY(!contrast->isChecked() && !large->isChecked());
  contrast->trigger();
  QVERIFY(KaTheme::displayOptions().highContrast);
  QVERIFY(KaTheme::savedDisplayOptions().highContrast);
  contrast->trigger();
  QVERIFY(!KaTheme::displayOptions().highContrast);
  QCOMPARE(KaTheme::savedDisplayOptions(), KaTheme::DisplayOptions());
}

void TestThemeOptions::loaderPrefersEmbeddedAndRejectsUnknownTokens() {
  QVERIFY(!KaTheme::styleSheetFromDiskRequested());
  QCOMPARE(KaTheme::loadStyleSheet(), KaTheme::embeddedStyleSheet());
  const QString partial = KaTheme::resolvedStyleSheet(QStringLiteral("QLabel { color: @ink@; border-color: @notAToken@; }"));
  QVERIFY(partial.contains(KaTheme::tokens().ink.name()));
  QCOMPARE(KaTheme::unresolvedTokens(partial), QStringList{QStringLiteral("notAToken")});
  QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("unknown tokens notAToken")));
  QCOMPARE(KaTheme::completeStyleSheet(QStringLiteral("QLabel { color: @notAToken@; }")),
           KaTheme::resolvedStyleSheet(KaTheme::embeddedStyleSheet()));
  QVERIFY(KaTheme::unresolvedTokens(KaTheme::applicationStyleSheet()).isEmpty());
}

// F192/F195: no colour literals or phantom weights; 3D-era pressed patches gone.
void TestThemeOptions::sheetHygiene() {
  const QString sheet = KaTheme::embeddedStyleSheet();
  QVERIFY2(!QRegularExpression(QStringLiteral("#[0-9A-Fa-f]{6}\\b")).match(sheet).hasMatch(),
           "colours come from tokens, not #rrggbb literals");
  QVERIFY2(!sheet.contains(QStringLiteral("font-weight: 500")), "Malgun Gothic has no 500");
  QCOMPARE(sheet.count(QStringLiteral("font-weight: 600")), 1);  // the Segoe UI wordmark only
  QVERIFY2(!sheet.contains(QStringLiteral("margin-top: 0;")), "no pressed-state margin patches");
  QVERIFY(sheet.contains(QStringLiteral("QScrollBar::add-line, QScrollBar::sub-line { width: 0px; height: 0px;")));
  QVERIFY(sheet.contains(QStringLiteral("QProgressBar::chunk")));
  QVERIFY(KaTheme::unresolvedTokens(KaTheme::resolvedStyleSheet(sheet)).isEmpty());
  QVERIFY(!sheet.contains(QStringLiteral("url(")));
}

QTEST_MAIN(TestThemeOptions)
#include "test_theme_options.moc"
