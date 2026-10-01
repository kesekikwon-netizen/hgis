// The one-row ribbon never folds into 「더 많은 작업」: it draws every group at the biggest size that fits
// the window (tile 56 down to 34 with labels, 32 with labels, then 32 · 24 · 20 with the labels hidden),
// and the size follows the window width in one direction only. The widget tests rebuild the production
// chip list from MainWindowRibbon.cpp (ribbon_fixture.h).
#include <QtTest>
#include <climits>
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFontDatabase>
#include <QMainWindow>
#include <QRegularExpression>
#include <QToolButton>
#include "app/KaBeginnerRibbon.h"
#include "app/KaTheme.h"
#include "ribbon_fixture.h"

namespace fx = RibbonFixture;

class TestRibbonOverflow : public QObject {
  Q_OBJECT
private slots:
  void initTestCase();
  void looks_followTheSpecTable();
  void chooseLook_picksFirstThatFits();
  void labels_shrinkToOneFloorAndKeepWordingInTip();
  void chipTooltip_alwaysHasTheName();
  void productionRibbon_data();
  void productionRibbon();
  void sizeOnlyShrinksAsWindowNarrows();
  void ribbonMinimumWidthIsSmallestLook();
  void lookWidths_matchTheLayoutAtEverySize();
  void sizeChangeIsLoggedOnce();
  void firstVisibleSizeIsLoggedAtTheDefault();
};

void TestRibbonOverflow::initTestCase() {
#ifdef Q_OS_WIN
  const QDir windows(qEnvironmentVariable("WINDIR", QStringLiteral("C:/Windows")));
  for (const QString& file : {QStringLiteral("malgun.ttf"), QStringLiteral("malgunbd.ttf")})
    QFontDatabase::addApplicationFont(windows.filePath(QStringLiteral("Fonts/") + file));
#endif
  KaTheme::apply(qApp);
}

void TestRibbonOverflow::looks_followTheSpecTable() {
  const QList<RibbonLook> looks = KaBeginnerRibbon::looks();
  QList<int> tiles;
  for (int tile = 56; tile >= 34; tile -= 2) tiles << tile;
  tiles << 32 << 32 << 24 << 20;
  QCOMPARE(looks.size(), tiles.size());
  for (int i = 0; i < looks.size(); ++i) {
    QCOMPARE(looks.at(i).tile, tiles.at(i));
    QCOMPARE(looks.at(i).labels, i <= 12);  // labels stay through the first tile 32, then hide
    QCOMPARE(looks.at(i).chipWidth, i <= 12 ? 0 : (i == 13 ? 40 : i == 14 ? 30 : 26));
    // glyph = qRound(tile * 18 / 32.0); the spec table writes 12 for tile 20 (11.25 would round to 11)
    QCOMPARE(looks.at(i).glyph, tiles.at(i) == 20 ? 12 : qRound(tiles.at(i) * 18 / 32.0));
  }
  QCOMPARE(looks.first().glyph, 32);
  QCOMPARE(looks.at(4).glyph, 27);  // tile 48
  QCOMPARE(looks.at(12).glyph, 18);
  QCOMPARE(looks.at(14).glyph, 14);
}

void TestRibbonOverflow::chooseLook_picksFirstThatFits() {
  const QList<int> widths = {1700, 1600, 1500, 1265, 1100, 900, 780};
  QCOMPARE(KaBeginnerRibbon::chooseLook(widths, 1904), 0);
  QCOMPARE(KaBeginnerRibbon::chooseLook(widths, 1550), 2);
  QCOMPARE(KaBeginnerRibbon::chooseLook(widths, 1265), 3);
  QCOMPARE(KaBeginnerRibbon::chooseLook(widths, 1000), 5);
  QCOMPARE(KaBeginnerRibbon::chooseLook(widths, 500), 6);  // nothing fits: the smallest
}

// F151/F154: one shrink floor for every chip; a label still too wide keeps its words in the tip.
void TestRibbonOverflow::labels_shrinkToOneFloorAndKeepWordingInTip() {
  const auto& metrics = KaTheme::buttonMetrics();
  QVERIFY(metrics.ribbonMinFontSize >= 11 && metrics.ribbonMinFontSize <= metrics.ribbonFontSize);
  QToolButton fits;
  fits.setText(QStringLiteral("그리기"));
  KaBeginnerRibbon::applyTwoLine(&fits);
  QCOMPARE(fits.font().pixelSize(), metrics.ribbonFontSize);
  // The widest production labels keep full size: the chip grows with its label.
  for (const QString& label : {QStringLiteral("검수·제출"), QStringLiteral("다른 이름")}) {
    QToolButton tight;
    tight.setText(label);
    KaBeginnerRibbon::applyTwoLine(&tight);
    QCOMPARE(tight.font().pixelSize(), metrics.ribbonFontSize);
    QVERIFY(tight.styleSheet().isEmpty());
  }
  QToolButton tooLong;
  tooLong.setText(QStringLiteral("국토지리정보원 수치지형도"));
  tooLong.setToolTip(QStringLiteral("받습니다"));
  KaBeginnerRibbon::applyTwoLine(&tooLong);
  QCOMPARE(tooLong.font().pixelSize(), metrics.ribbonMinFontSize);
  QVERIFY(tooLong.toolTip().startsWith(tooLong.text()));
  QVERIFY(tooLong.toolTip().contains(QStringLiteral("받습니다")));
}

// With the labels hidden the tooltip is the only place a chip shows its name, and an action that changes
// its own tooltip later (the 「먼저 조사를 여세요」 reasons) must not take the name away again.
void TestRibbonOverflow::chipTooltip_alwaysHasTheName() {
  KaBeginnerRibbon ribbon;
  ribbon.addGroup(QStringLiteral("survey"), QStringLiteral("조사"));
  auto* action = new QAction(QStringLiteral("열기"), &ribbon);
  action->setToolTip(QStringLiteral("저장한 조사를 엽니다"));
  QToolButton* chip = ribbon.addAction(QStringLiteral("survey"), action);
  QVERIFY2(chip->toolTip().contains(QStringLiteral("열기")), qPrintable(chip->toolTip()));
  QVERIFY(chip->toolTip().contains(QStringLiteral("저장한 조사를 엽니다")));
  action->setToolTip(QStringLiteral("조사를 먼저 여세요"));
  QVERIFY2(chip->toolTip().contains(QStringLiteral("열기")), qPrintable(chip->toolTip()));
  QVERIFY(chip->toolTip().contains(QStringLiteral("조사를 먼저 여세요")));
  QCOMPARE(chip->text(), QStringLiteral("열기"));
}

void TestRibbonOverflow::productionRibbon_data() {
  QTest::addColumn<int>("windowWidth");
  QTest::newRow("1904 (1920 screen)") << 1904;
  QTest::newRow("1536") << 1536;
  QTest::newRow("1366") << 1366;
  QTest::newRow("1280") << 1280;
  QTest::newRow("1366x768 at 125%") << 1093;
  QTest::newRow("1024") << 1024;
}

void TestRibbonOverflow::productionRibbon() {
  QFETCH(int, windowWidth);
  const fx::RibbonSource source = fx::readProductionRibbon();
  QVERIFY2(source.groups.size() >= 5, "run from the source tree: MainWindowRibbon.cpp groups");
  QVERIFY2(source.chips.size() >= 20, qPrintable(QStringLiteral("parsed %1 chips").arg(source.chips.size())));
  QMainWindow window;
  KaBeginnerRibbon* ribbon = fx::showProductionRibbon(window, source, windowWidth);
  auto* bar = qobject_cast<QToolBar*>(ribbon->parentWidget());
  QVERIFY(bar);
  // The toolbar grows and shrinks with the ribbon's size, so no chip is cut off at the bottom.
  QTRY_VERIFY2(ribbon->height() >= ribbon->sizeHint().height() && bar->height() >= ribbon->height(),
               qPrintable(QStringLiteral("ribbon %1 needs %2, toolbar %3").arg(ribbon->height()).arg(ribbon->sizeHint().height()).arg(bar->height())));
  fx::verifyWhole(ribbon, source);
  if (QTest::currentTestFailed()) return;
  const RibbonLook look = ribbon->look();
  const QString where = fx::describe(ribbon, source);
  if (windowWidth >= 1904) {
    // The spare room is below one biggest chip (64 px): big windows fill it with bigger icons.
    QVERIFY2(look.labels && look.tile > 32, qPrintable(where));
    QFrame* last = ribbon->group(source.groups.last().first);
    QVERIFY2(ribbon->width() - last->x() - last->width() < 64, qPrintable(where));
  }
  if (windowWidth <= 1093) {
    QVERIFY2(!look.labels && (look.tile == 32 || look.tile == 24 || look.tile == 20), qPrintable(where));
    for (QToolButton* chip : ribbon->tabButtons()) QCOMPARE(chip->toolButtonStyle(), Qt::ToolButtonIconOnly);
  }
  qInfo().noquote() << "window" << windowWidth << where;
}

void TestRibbonOverflow::sizeOnlyShrinksAsWindowNarrows() {
  const fx::RibbonSource source = fx::readProductionRibbon();
  QVERIFY2(source.groups.size() >= 5, "run from the source tree: MainWindowRibbon.cpp groups");
  QVERIFY2(source.chips.size() >= 20, qPrintable(QStringLiteral("parsed %1 chips").arg(source.chips.size())));
  const fx::LogCapture capture;  // the walk crosses about 30 sizes; their log lines are not what it checks
  QMainWindow window;
  KaBeginnerRibbon* ribbon = fx::showProductionRibbon(window, source, 1904);
  int tile = INT_MAX;
  bool hidden = false;
  QList<int> narrowing;  // the size at each width, as its place in looks()
  QList<int> widening;
  for (int width = 1904; width >= 1024; width -= 30) {
    window.resize(width, 768);
    fx::settle();
    const RibbonLook look = ribbon->look();
    QVERIFY2(look.tile <= tile && !(hidden && look.labels), qPrintable(fx::describe(ribbon, source)));
    narrowing << fx::lookIndex(look);
    tile = look.tile;
    hidden = hidden || !look.labels;
    fx::verifyWhole(ribbon, source);
    if (QTest::currentTestFailed()) return;
  }
  // And back: widening never moves to a smaller size in looks(). The tile alone does not say it: tile 32
  // is one size with the labels and another without.
  int index = INT_MAX;  // the first width, already narrower than the last one above, sets the start
  for (int width = 1024; width <= 1904; width += 30) {
    window.resize(width, 768);
    fx::settle();
    const int now = fx::lookIndex(ribbon->look());
    QVERIFY2(now <= index, qPrintable(fx::describe(ribbon, source)));
    widening << now;
    index = now;
  }
  qInfo() << "sizes visited (place in looks()), narrowing:" << narrowing << "widening:" << widening;
  // Each size is about 50 px wide in window width, so a 30 px step lands in every one of them. Tile 32 with
  // labels (12) is the one the tile alone cannot tell from its neighbour.
  QVERIFY2(narrowing.contains(12) && widening.contains(12), "fixture: the walk crosses tile 32 with labels both ways");
  QVERIFY(ribbon->look().labels);
}

void TestRibbonOverflow::ribbonMinimumWidthIsSmallestLook() {
  const fx::RibbonSource source = fx::readProductionRibbon();
  KaBeginnerRibbon ribbon;
  ribbon.setAttribute(Qt::WA_DontShowOnScreen);
  fx::fillRibbon(&ribbon, source);
  const int minimum = ribbon.minimumSizeHint().width();
  QVERIFY(minimum > 0 && minimum < 1024);
  ribbon.resize(minimum, ribbon.sizeHint().height());
  ribbon.show();
  fx::settle();
  QCOMPARE(ribbon.look().tile, 20);
  QVERIFY(!ribbon.look().labels);
  QCOMPARE(ribbon.lookWidths().last(), minimum);
  fx::verifyWhole(&ribbon, source);
}

// The widths the ribbon plans with are the widths the layout really takes: at every size the last group
// ends exactly at lookWidths()[i], and a ribbon that wide chooses that size (or an equal bigger one).
void TestRibbonOverflow::lookWidths_matchTheLayoutAtEverySize() {
  const fx::RibbonSource source = fx::readProductionRibbon();
  QVERIFY2(source.groups.size() >= 5, "run from the source tree: MainWindowRibbon.cpp groups");
  QVERIFY2(source.chips.size() >= 20, qPrintable(QStringLiteral("parsed %1 chips").arg(source.chips.size())));
  const QList<RibbonLook> looks = KaBeginnerRibbon::looks();
  KaBeginnerRibbon probe;
  fx::fillRibbon(&probe, source);
  const QList<int> widths = probe.lookWidths();
  QCOMPARE(widths.size(), looks.size());
  for (int i = 1; i < widths.size(); ++i) QVERIFY2(widths.at(i) <= widths.at(i - 1), "smaller sizes are never wider");
  for (int i = 0; i < widths.size(); ++i) {
    KaBeginnerRibbon ribbon;
    ribbon.setAttribute(Qt::WA_DontShowOnScreen);
    fx::fillRibbon(&ribbon, source);
    ribbon.setFixedSize(widths.at(i), ribbon.sizeHint().height() + 40);
    ribbon.show();
    fx::settle();
    const int chosen = KaBeginnerRibbon::chooseLook(widths, widths.at(i));
    QCOMPARE(ribbon.look().tile, looks.at(chosen).tile);
    QCOMPARE(ribbon.look().labels, looks.at(chosen).labels);
    QFrame* last = ribbon.group(source.groups.last().first);
    QVERIFY2(last->x() + last->width() <= widths.at(i), qPrintable(QStringLiteral("size %1 overflows").arg(i)));
    QCOMPARE(ribbon.lookWidths().at(chosen), widths.at(chosen));
    QCOMPARE(last->x() + last->width() + 1, widths.at(chosen));  // + the 1 px right margin of the row
  }
}

// The session log gets a line when the chosen size changes and none while it stays. The ribbon may pass
// through a size on the way (a window resize reaches it in a few steps), so a step that changes the size
// adds lines whose last one is the size it ended on.
void TestRibbonOverflow::sizeChangeIsLoggedOnce() {
  const fx::RibbonSource source = fx::readProductionRibbon();
  QMainWindow window;
  KaBeginnerRibbon* ribbon = fx::showProductionRibbon(window, source, 1904);
  const fx::LogCapture capture;
  static const QRegularExpression format(
      QStringLiteral("^\\[ribbon\\] 크기 타일 (\\d+) · 글자 (보임|숨김) · 창 \\d+ · 가용 \\d+$"));
  RibbonLook before = ribbon->look();
  int stepsThatChanged = 0;
  for (int width : {1904, 1900, 1700, 1696, 1300, 1100, 1024, 1100, 1904}) {
    const qsizetype logged = fx::g_ribbonLog.size();
    window.resize(width, 768);
    fx::settle();
    const RibbonLook look = ribbon->look();
    const bool changed = look.tile != before.tile || look.labels != before.labels;
    stepsThatChanged += changed;
    const QString step = QStringLiteral("window %1: tile %2 labels %3, log: %4")
                             .arg(width).arg(look.tile).arg(look.labels).arg(fx::g_ribbonLog.join(QLatin1String(" | ")));
    QVERIFY2(changed == (fx::g_ribbonLog.size() > logged), qPrintable(step));
    if (changed) {
      const QRegularExpressionMatch last = format.match(fx::g_ribbonLog.last());
      QVERIFY2(last.hasMatch() && last.captured(1).toInt() == look.tile &&
                   (last.captured(2) == QStringLiteral("보임")) == look.labels,
               qPrintable(step));
    }
    before = look;
  }
  QVERIFY2(stepsThatChanged >= 5, "fixture: the widths cross several sizes");
  for (int i = 0; i < fx::g_ribbonLog.size(); ++i) {
    QVERIFY2(format.match(fx::g_ribbonLog.at(i)).hasMatch(), qPrintable(fx::g_ribbonLog.at(i)));
    if (i > 0) QVERIFY2(fx::g_ribbonLog.at(i) != fx::g_ribbonLog.at(i - 1), "a size is logged once, not repeated");
  }
}

// The first size the ribbon draws on screen is logged even when it is the size the ribbon starts at (tile 32
// with labels, looks()[12]); without that line a field report of a ribbon that never changed size has no size.
void TestRibbonOverflow::firstVisibleSizeIsLoggedAtTheDefault() {
  const fx::RibbonSource source = fx::readProductionRibbon();
  QVERIFY2(source.groups.size() >= 5, "run from the source tree: MainWindowRibbon.cpp groups");
  QVERIFY2(source.chips.size() >= 20, qPrintable(QStringLiteral("parsed %1 chips").arg(source.chips.size())));
  const fx::LogCapture capture;
  KaBeginnerRibbon ribbon;
  ribbon.setAttribute(Qt::WA_DontShowOnScreen);
  fx::fillRibbon(&ribbon, source);
  const QList<int> widths = ribbon.lookWidths();
  QVERIFY2(widths.at(11) > widths.at(12), "fixture: tile 32 with labels is the biggest size that fits its own width");
  ribbon.resize(widths.at(12), ribbon.sizeHint().height());
  ribbon.show();
  fx::settle();
  QVERIFY2(ribbon.look().tile == 32 && ribbon.look().labels, qPrintable(fx::describe(&ribbon, source)));
  QVERIFY2(fx::g_ribbonLog.size() == 1 && fx::g_ribbonLog.first().startsWith(QStringLiteral("[ribbon] 크기 타일 32 · 글자 보임")),
           qPrintable(QStringLiteral("%1 log lines: %2").arg(fx::g_ribbonLog.size()).arg(fx::g_ribbonLog.join(QLatin1String(" | ")))));
}

QTEST_MAIN(TestRibbonOverflow)
#include "test_ribbon_overflow.moc"
