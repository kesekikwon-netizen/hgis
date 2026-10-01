// F147: the one-row ribbon folds groups into 「더 많은 작업」 only when that
// gains room, keeps 조사·내보내기·기록 first, and never clips the overflow button.
// The widget test rebuilds the production chip list from MainWindowRibbon.cpp.
#include <QtTest>
#include <algorithm>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QFrame>
#include <QHash>
#include <QLayout>
#include <QMainWindow>
#include <QMenu>
#include <QRegularExpression>
#include <QToolBar>
#include <QToolButton>
#include "app/KaAppBar.h"
#include "app/KaBeginnerRibbon.h"
#include "app/KaTheme.h"

namespace {

struct Chip {
  qsizetype at = 0;
  QString group;
  QString text;
};

struct RibbonSource {
  QStringList groups;
  QStringList priority;
  QStringList pinned;
  QList<Chip> chips;
};

QStringList literalsIn(const QString& text) {
  QStringList out;
  static const QRegularExpression literal(QStringLiteral(R"re(QStringLiteral\("([^"]+)"\))re"));
  for (auto it = literal.globalMatch(text); it.hasNext();) out << it.next().captured(1);
  return out;
}

QStringList listArgument(const QString& body, const QString& call) {
  const qsizetype at = body.indexOf(call + QStringLiteral("({"));
  if (at < 0) return {};
  const qsizetype end = body.indexOf(QStringLiteral("})"), at);
  return literalsIn(body.mid(at, end - at));
}

// Groups, keep priority, pinned groups and every chip of MainWindow::buildMenus,
// in source order.
RibbonSource readProductionRibbon() {
  RibbonSource out;
  QFile file(QStringLiteral("src/app/MainWindowRibbon.cpp"));
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return out;
  const QString src = QString::fromUtf8(file.readAll());
  const qsizetype start = src.indexOf(QStringLiteral("void MainWindow::buildMenus"));
  if (start < 0) return out;
  const qsizetype end = src.indexOf(QStringLiteral("\nvoid MainWindow::"), start + 10);
  const QString body = src.mid(start, end > start ? end - start : -1);
  static const QRegularExpression group(
      QStringLiteral(R"re(ribbon->addGroup\(QStringLiteral\("(\w+)"\),\s*QStringLiteral\("[^"]+"\)\))re"));
  for (auto it = group.globalMatch(body); it.hasNext();) out.groups << it.next().captured(1);
  out.priority = listArgument(body, QStringLiteral("setKeepPriority"));
  out.pinned = listArgument(body, QStringLiteral("setPinned"));
  static const QRegularExpression icon(QStringLiteral(
      R"re(addIcon\(\s*QStringLiteral\("(\w+)"\),\s*QStringLiteral\("\w+"\),\s*QStringLiteral\("([^"]+)"\))re"));
  for (auto it = icon.globalMatch(body); it.hasNext();) {
    const auto m = it.next();
    out.chips.append({m.capturedStart(), m.captured(1), m.captured(2)});
  }
  static const QRegularExpression widget(QStringLiteral(R"re(ribbon->addWidget\(QStringLiteral\("(\w+)"\),\s*(\w+)\))re"));
  for (auto it = widget.globalMatch(body); it.hasNext();) {
    const auto m = it.next();
    const QRegularExpression setText(QRegularExpression::escape(m.captured(2)) +
                                     QStringLiteral(R"re(->setText\(QStringLiteral\("([^"]+)"\)\))re"));
    QString text;
    for (auto t = setText.globalMatch(body.left(m.capturedStart())); t.hasNext();) text = t.next().captured(1);
    out.chips.append({m.capturedStart(), m.captured(1), text.isEmpty() ? m.captured(2) : text});
  }
  std::sort(out.chips.begin(), out.chips.end(), [](const Chip& a, const Chip& b) { return a.at < b.at; });
  return out;
}

bool onRibbon(KaBeginnerRibbon* ribbon, const QString& id) {
  QFrame* frame = ribbon->group(id);
  return frame && frame->parentWidget() == ribbon && frame->isVisible();
}

// The production ribbon (groups, chips, keep order) and the app bar in a main window of the given
// width, shown and laid out. Returns the ribbon, owned by `window`.
KaBeginnerRibbon* showProductionRibbon(QMainWindow& window, const RibbonSource& source, int windowWidth) {
  window.setAttribute(Qt::WA_DontShowOnScreen);
  auto* toolbar = new QToolBar(&window);
  toolbar->setObjectName(QStringLiteral("mainToolbar"));
  toolbar->setIconSize(QSize(20, 20));
  toolbar->setMovable(false);
  auto* appBar = new KaAppBar(toolbar);
  appBar->setRegionWidget(new QWidget);
  auto* ribbon = new KaBeginnerRibbon(toolbar);
  for (const QString& id : source.groups) ribbon->addGroup(id, id);
  ribbon->setKeepPriority(source.priority);
  ribbon->setPinned(source.pinned);
  for (const Chip& chip : source.chips) {
    auto* button = new QToolButton(ribbon);
    button->setText(chip.text);
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    ribbon->addWidget(chip.group, button);
  }
  toolbar->addWidget(ribbon);
  toolbar->addWidget(appBar);
  window.addToolBar(toolbar);
  window.setCentralWidget(new QWidget(&window));
  window.resize(windowWidth, 768);
  window.show();
  QCoreApplication::processEvents();
  QCoreApplication::processEvents();
  return ribbon;
}

}  // namespace

class TestRibbonOverflow : public QObject {
  Q_OBJECT
private slots:
  void initTestCase();
  void plan_skipsHopelessPinnedFold();
  void plan_pinnedFoldsUnpinnedWhenThatMakesRoom();
  void plan_keepGroupsComeBeforePinned();
  void plan_everythingFitsWithoutOverflow();
  void labels_shrinkToOneFloorAndKeepWordingInTip();
  void productionRibbon_data();
  void productionRibbon();
  void mockupRibbonFitsAt1920();
};

void TestRibbonOverflow::initTestCase() {
#ifdef Q_OS_WIN
  const QDir windows(qEnvironmentVariable("WINDIR", QStringLiteral("C:/Windows")));
  for (const QString& file : {QStringLiteral("malgun.ttf"), QStringLiteral("malgunbd.ttf")})
    QFontDatabase::addApplicationFont(windows.filePath(QStringLiteral("Fonts/") + file));
#endif
  KaTheme::apply(qApp);
}

// The 1280 case of the evaluation: 배경 지도 (483) cannot fit even after folding
// 정합 and 기타, so they must stay on the ribbon instead of folding for nothing.
void TestRibbonOverflow::plan_skipsHopelessPinnedFold() {
  const QStringList priority = {QStringLiteral("survey"), QStringLiteral("out"), QStringLiteral("record"),
                                QStringLiteral("basemap"), QStringLiteral("align"), QStringLiteral("fetch"),
                                QStringLiteral("more")};
  const QHash<QString, int> widths = {{QStringLiteral("survey"), 243}, {QStringLiteral("out"), 303},
                                      {QStringLiteral("record"), 303}, {QStringLiteral("basemap"), 483},
                                      {QStringLiteral("align"), 63},   {QStringLiteral("fetch"), 183},
                                      {QStringLiteral("more"), 63}};
  const QStringList plan = KaBeginnerRibbon::planGroups(
      priority, {QStringLiteral("basemap"), QStringLiteral("fetch")}, widths, 1008, 76);
  QVERIFY(plan.contains(QStringLiteral("survey")));
  QVERIFY(plan.contains(QStringLiteral("out")));
  QVERIFY(plan.contains(QStringLiteral("record")));
  QVERIFY2(plan.contains(QStringLiteral("align")), "정합 must not fold for a pinned group that cannot fit anyway");
  QVERIFY(!plan.contains(QStringLiteral("basemap")));
  int used = 76;
  for (const QString& id : plan) used += widths.value(id);
  QVERIFY2(used <= 1008, "the overflow button keeps its own room");
}

void TestRibbonOverflow::plan_pinnedFoldsUnpinnedWhenThatMakesRoom() {
  const QHash<QString, int> widths = {{QStringLiteral("survey"), 200}, {QStringLiteral("align"), 100},
                                      {QStringLiteral("more"), 100}, {QStringLiteral("basemap"), 250}};
  const QStringList plan = KaBeginnerRibbon::planGroups(
      {QStringLiteral("survey"), QStringLiteral("align"), QStringLiteral("more"), QStringLiteral("basemap")},
      {QStringLiteral("basemap")}, widths, 530, 30);
  QCOMPARE(plan, QStringList({QStringLiteral("survey"), QStringLiteral("basemap")}));
}

void TestRibbonOverflow::plan_keepGroupsComeBeforePinned() {
  const QHash<QString, int> widths = {{QStringLiteral("survey"), 200}, {QStringLiteral("fetch"), 250},
                                      {QStringLiteral("record"), 250}};
  const QStringList plan = KaBeginnerRibbon::planGroups(
      {QStringLiteral("survey"), QStringLiteral("fetch"), QStringLiteral("record")}, {QStringLiteral("fetch")},
      widths, 560, 30);
  QCOMPARE(plan, QStringList({QStringLiteral("survey"), QStringLiteral("record")}));
}

void TestRibbonOverflow::plan_everythingFitsWithoutOverflow() {
  const QHash<QString, int> widths = {{QStringLiteral("survey"), 200}, {QStringLiteral("out"), 300}};
  const QStringList all = {QStringLiteral("survey"), QStringLiteral("out")};
  QCOMPARE(KaBeginnerRibbon::planGroups(all, {}, widths, 500, 80), all);
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

void TestRibbonOverflow::productionRibbon_data() {
  QTest::addColumn<int>("windowWidth");
  QTest::newRow("1366x768 at 125%") << 1093;
  QTest::newRow("1280") << 1280;
  QTest::newRow("1536") << 1536;
}

void TestRibbonOverflow::productionRibbon() {
  QFETCH(int, windowWidth);
  const RibbonSource source = readProductionRibbon();
  QVERIFY2(source.groups.size() >= 5, "run from the source tree: MainWindowRibbon.cpp groups");
  QVERIFY2(source.chips.size() >= 20, qPrintable(QStringLiteral("parsed %1 chips").arg(source.chips.size())));
  QMainWindow window;
  KaBeginnerRibbon* ribbon = showProductionRibbon(window, source, windowWidth);

  auto* overflow = ribbon->findChild<QToolButton*>(QStringLiteral("ribbonOverflow"));
  QVERIFY(overflow);
  QString placed;
  int used = 0;
  bool anyFolded = false;
  for (const QString& id : source.groups) {
    const bool shown = onRibbon(ribbon, id);
    placed += QStringLiteral("%1:%2 ").arg(id, shown ? QStringLiteral("in") : QStringLiteral("out"));
    if (shown) {
      used += ribbon->group(id)->sizeHint().width();
      QVERIFY2(ribbon->group(id)->geometry().right() < ribbon->width(), qPrintable(id + QStringLiteral(" is clipped")));
    } else {
      anyFolded = true;
      auto* menu = ribbon->findChild<QMenu*>(QStringLiteral("ribbonOverflowGroup_") + id);
      QVERIFY2(menu && menu->menuAction()->isVisible(), qPrintable(id + QStringLiteral(" is unreachable")));
    }
  }
  const QString where = QStringLiteral("window %1, ribbon %2: %3").arg(windowWidth).arg(ribbon->width()).arg(placed);
  QVERIFY2(onRibbon(ribbon, QStringLiteral("survey")) && onRibbon(ribbon, QStringLiteral("out")), qPrintable(where));
  if (windowWidth >= 1280)
    QVERIFY2(onRibbon(ribbon, QStringLiteral("record")), qPrintable(where));
  QCOMPARE(overflow->isVisible(), anyFolded);
  if (anyFolded) {
    QVERIFY2(overflow->geometry().right() < ribbon->width(), qPrintable(QStringLiteral("overflow clipped: ") + where));
    // Nothing folded for nothing: whatever room is left is smaller than every folded group.
    const auto margins = ribbon->layout()->contentsMargins();
    const int room = ribbon->width() - margins.left() - margins.right() - used - overflow->sizeHint().width();
    for (const QString& id : source.groups)
      if (!onRibbon(ribbon, id))
        QVERIFY2(ribbon->group(id)->sizeHint().width() > room, qPrintable(id + QStringLiteral(" folded with room left: ") + where));
  }
  qInfo().noquote() << where;
}

// Mockup chips (tile 32, label 13 px, width = label + 8) at the 1920 x 1080 screen of the field PC:
// the 1904 px window keeps all seven groups on the ribbon, none folded into 「더 많은 작업」.
void TestRibbonOverflow::mockupRibbonFitsAt1920() {
  const RibbonSource source = readProductionRibbon();
  QVERIFY2(source.groups.size() >= 5, "run from the source tree: MainWindowRibbon.cpp groups");
  QMainWindow window;
  KaBeginnerRibbon* ribbon = showProductionRibbon(window, source, 1904);
  auto* overflow = ribbon->findChild<QToolButton*>(QStringLiteral("ribbonOverflow"));
  QVERIFY(overflow);
  int folded = 0;
  QString placed;
  for (const QString& id : source.groups) {
    const bool shown = onRibbon(ribbon, id);
    placed += QStringLiteral("%1:%2 ").arg(id, shown ? QStringLiteral("in") : QStringLiteral("out"));
    if (!shown) ++folded;
  }
  const QString where = QStringLiteral("window 1904, ribbon %1, needs %2: %3")
                            .arg(ribbon->width()).arg(ribbon->sizeHint().width()).arg(placed);
  QVERIFY2(folded == 0, qPrintable(where));
  QVERIFY2(!overflow->isVisible(), qPrintable(where));
  qInfo().noquote() << where;
}

QTEST_MAIN(TestRibbonOverflow)
#include "test_ribbon_overflow.moc"
