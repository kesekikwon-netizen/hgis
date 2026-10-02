// KaToolbarFit keeps a tool bar on one row without the » extension button: when the buttons do not fit it
// hides their labels first, then shrinks the icons from 20 to 16 px, and gives both back (in the reverse
// order) when the bar gets room again. The drawing sub-toolbar (#subToolbar) uses it.
#include <QtTest>
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFontDatabase>
#include <QLabel>
#include <QLayout>
#include <QMainWindow>
#include <QPixmap>
#include <QToolBar>
#include <QToolButton>
#include "app/KaTheme.h"
#include "app/KaToolbarFit.h"

namespace {

// The bar sits in the top area of a window like #subToolbar in the main window: the window's width is the
// bar's width, and dragging the window's edge is what resizes it.
struct Host {
  QMainWindow window;
  QToolBar* bar = new QToolBar(&window);
  Host() {
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.addToolBar(Qt::TopToolBarArea, bar);
    window.setCentralWidget(new QWidget);
  }
};

// A bar like #subToolbar (MainWindowRibbon.cpp): 20 px icons, the label beside the icon.
void configureBar(QToolBar& bar) {
  bar.setObjectName(QStringLiteral("subToolbar"));
  bar.setIconSize(QSize(20, 20));
  bar.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  bar.setMovable(false);
}

// A caption label, checkable tools with a picture and a Korean name, then two tools without a picture.
void addTools(QToolBar& bar, int tools) {
  QPixmap picture(32, 32);
  picture.fill(Qt::darkGray);
  bar.addWidget(new QLabel(QStringLiteral("  그리기 › ")));
  for (int i = 0; i < tools; ++i)
    bar.addAction(QIcon(picture), QStringLiteral("도구 이름 %1번").arg(i + 1))->setCheckable(true);
  bar.addSeparator();
  bar.addAction(QStringLiteral("폴리곤 묶기"));
  bar.addAction(QStringLiteral("닫기"));
}

void fillBar(QToolBar& bar, int tools) {
  configureBar(bar);
  addTools(bar, tools);
}

bool extShown(QToolBar& bar) {
  auto* ext = bar.findChild<QToolButton*>(QStringLiteral("qt_toolbar_ext_button"));
  return ext && ext->isVisible();
}

// The width the bar needs when its buttons are drawn at this stage.
int needed(QToolBar& bar, Qt::ToolButtonStyle style, int icon) {
  bar.setToolButtonStyle(style);
  bar.setIconSize(QSize(icon, icon));
  bar.layout()->invalidate();
  return bar.sizeHint().width();
}

// A window resize reaches the offscreen platform window a little after the call, and the bar re-lays out
// in a posted event.
void settle() {
  QTest::qWait(20);
  QCoreApplication::processEvents();
}

void expectStage(Host& host, int width, Qt::ToolButtonStyle style, int icon) {
  QToolBar& bar = *host.bar;
  host.window.resize(width, 200);
  settle();
  const QString where = QStringLiteral("window %1, bar %2: style %3 icon %4 needs %5, extension %6")
                            .arg(width).arg(bar.width()).arg(bar.toolButtonStyle()).arg(bar.iconSize().width())
                            .arg(bar.sizeHint().width()).arg(extShown(bar));
  QVERIFY2(bar.width() == width, qPrintable(where));  // the window gives the bar all of its width
  QVERIFY2(bar.toolButtonStyle() == style && bar.iconSize() == QSize(icon, icon), qPrintable(where));
  QVERIFY2(!extShown(bar), qPrintable(where));
  for (QAction* action : bar.actions()) {
    QWidget* button = bar.widgetForAction(action);
    QVERIFY2(action->isSeparator() || (button && button->isVisible()), qPrintable(action->text() + QStringLiteral(": ") + where));
  }
}

}  // namespace

class TestToolbarFit : public QObject {
  Q_OBJECT
private slots:
  void initTestCase();
  void toolbarFit_hidesTextThenShrinksIcons();
  void toolbarFit_neverShowsTheExtensionButtonWhileResizing();
  void toolbarFit_followsActionsAddedAndShownLater();
  void toolbarFit_checkedToolKeepsTheStage();
  void toolbarFit_longerTextRefits();
  void toolbarFit_installTwiceFitsOnce();
};

void TestToolbarFit::initTestCase() {
#ifdef Q_OS_WIN
  const QDir windows(qEnvironmentVariable("WINDIR", QStringLiteral("C:/Windows")));
  for (const QString& file : {QStringLiteral("malgun.ttf"), QStringLiteral("malgunbd.ttf")})
    QFontDatabase::addApplicationFont(windows.filePath(QStringLiteral("Fonts/") + file));
#endif
  KaTheme::apply(qApp);
}

void TestToolbarFit::toolbarFit_hidesTextThenShrinksIcons() {
  Host host;
  QToolBar& bar = *host.bar;
  fillBar(bar, 14);
  const int withText = needed(bar, Qt::ToolButtonTextBesideIcon, 20);
  const int iconsOnly = needed(bar, Qt::ToolButtonIconOnly, 20);
  const int small = needed(bar, Qt::ToolButtonIconOnly, 16);
  QVERIFY2(withText > iconsOnly + 100 && iconsOnly > small + 30, "fixture: the three stages need clearly different widths");
  bar.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  bar.setIconSize(QSize(20, 20));
  KaToolbarFit::install(&bar);
  host.window.show();

  expectStage(host, withText + 30, Qt::ToolButtonTextBesideIcon, 20);  // room: the labels show, icon 20
  if (QTest::currentTestFailed()) return;
  expectStage(host, withText - 1, Qt::ToolButtonIconOnly, 20);  // short by a pixel: labels hide first
  if (QTest::currentTestFailed()) return;
  expectStage(host, iconsOnly, Qt::ToolButtonIconOnly, 20);
  if (QTest::currentTestFailed()) return;
  expectStage(host, iconsOnly - 1, Qt::ToolButtonIconOnly, 16);  // still short: icons shrink
  if (QTest::currentTestFailed()) return;
  expectStage(host, small, Qt::ToolButtonIconOnly, 16);
  if (QTest::currentTestFailed()) return;
  expectStage(host, iconsOnly, Qt::ToolButtonIconOnly, 20);  // room again: back in the reverse order
  if (QTest::currentTestFailed()) return;
  expectStage(host, withText + 30, Qt::ToolButtonTextBesideIcon, 20);
}

// Dragging a window edge resizes the bar a few pixels at a time: at no width above the last stage's need
// may the » button show, and the stage only moves one way as the bar narrows (and back as it widens).
void TestToolbarFit::toolbarFit_neverShowsTheExtensionButtonWhileResizing() {
  Host host;
  QToolBar& bar = *host.bar;
  fillBar(bar, 14);
  const int small = needed(bar, Qt::ToolButtonIconOnly, 16);
  const int withText = needed(bar, Qt::ToolButtonTextBesideIcon, 20);
  bar.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  bar.setIconSize(QSize(20, 20));
  KaToolbarFit::install(&bar);
  host.window.show();
  int stage = 0;  // 0 labels, 1 icons only, 2 small icons
  for (int width = withText + 60; width >= small; width -= 13) {
    host.window.resize(width, 200);
    settle();
    const int now = bar.toolButtonStyle() != Qt::ToolButtonIconOnly ? 0 : bar.iconSize().width() == 20 ? 1 : 2;
    QVERIFY2(bar.width() == width && now >= stage && !extShown(bar), qPrintable(QStringLiteral("width %1 (bar %2) stage %3 after %4, extension %5").arg(width).arg(bar.width()).arg(now).arg(stage).arg(extShown(bar))));
    stage = now;
  }
  QCOMPARE(stage, 2);
  for (int width = small; width <= withText + 60; width += 13) {
    host.window.resize(width, 200);
    settle();
    const int now = bar.toolButtonStyle() != Qt::ToolButtonIconOnly ? 0 : bar.iconSize().width() == 20 ? 1 : 2;
    QVERIFY2(bar.width() == width && now <= stage && !extShown(bar), qPrintable(QStringLiteral("width %1 (bar %2) stage %3 after %4, extension %5").arg(width).arg(bar.width()).arg(now).arg(stage).arg(extShown(bar))));
    stage = now;
  }
  QCOMPARE(stage, 0);
}

// #subToolbar is cleared and refilled whenever another tool group opens, and is hidden in between.
void TestToolbarFit::toolbarFit_followsActionsAddedAndShownLater() {
  int small = 0;
  int withText = 0;
  {
    Host probe;
    fillBar(*probe.bar, 14);
    small = needed(*probe.bar, Qt::ToolButtonIconOnly, 16);
    withText = needed(*probe.bar, Qt::ToolButtonTextBesideIcon, 20);
  }
  Host host;
  QToolBar& bar = *host.bar;
  configureBar(bar);
  KaToolbarFit::install(&bar);
  host.window.resize(small + 10, 200);
  host.window.show();
  settle();
  addTools(bar, 14);  // filled after install, while the bar is already narrow
  settle();
  QVERIFY2(bar.toolButtonStyle() == Qt::ToolButtonIconOnly && bar.iconSize() == QSize(16, 16) && !extShown(bar),
           qPrintable(QStringLiteral("needs %1 of %2").arg(bar.sizeHint().width()).arg(bar.width())));
  bar.clear();
  bar.hide();
  addTools(bar, 14);
  host.window.resize(withText + 20, 200);
  bar.show();
  settle();
  QVERIFY2(bar.toolButtonStyle() == Qt::ToolButtonTextBesideIcon && bar.iconSize() == QSize(20, 20) && !extShown(bar),
           qPrintable(QStringLiteral("needs %1 of %2").arg(bar.sizeHint().width()).arg(bar.width())));
}

// Checking a tool changes no button width, so the bar keeps its stage. Fitting again would try the bigger
// stages first, and every try sets the style and the icon size of the bar and re-lays out all its buttons.
void TestToolbarFit::toolbarFit_checkedToolKeepsTheStage() {
  Host host;
  QToolBar& bar = *host.bar;
  fillBar(bar, 14);
  const int iconsOnly = needed(bar, Qt::ToolButtonIconOnly, 20);
  bar.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  bar.setIconSize(QSize(20, 20));
  KaToolbarFit::install(&bar);
  host.window.show();
  expectStage(host, iconsOnly + 5, Qt::ToolButtonIconOnly, 20);  // just short of the labels
  if (QTest::currentTestFailed()) return;

  QSignalSpy styleChanges(&bar, &QToolBar::toolButtonStyleChanged);
  QSignalSpy iconChanges(&bar, &QToolBar::iconSizeChanged);
  int checkable = 0;
  for (QAction* action : bar.actions()) {
    if (!action->isCheckable()) continue;
    action->setChecked(true);
    ++checkable;
  }
  QCOMPARE(checkable, 14);
  settle();
  for (QAction* action : bar.actions())
    if (action->isCheckable()) action->setChecked(false);
  settle();
  QVERIFY2(styleChanges.isEmpty() && iconChanges.isEmpty(),
           qPrintable(QStringLiteral("%1 style and %2 icon size changes while 14 tools were checked and unchecked")
                          .arg(styleChanges.size()).arg(iconChanges.size())));
  expectStage(host, iconsOnly + 5, Qt::ToolButtonIconOnly, 20);
}

// A change that does move a button width (a longer name on a tool without a picture, whose text shows at
// every stage) still fits again.
void TestToolbarFit::toolbarFit_longerTextRefits() {
  const QString longer = QStringLiteral("닫고 나가기");
  int iconsOnly = 0;
  {
    Host probe;
    fillBar(*probe.bar, 14);
    iconsOnly = needed(*probe.bar, Qt::ToolButtonIconOnly, 20);
    const int small = needed(*probe.bar, Qt::ToolButtonIconOnly, 16);
    probe.bar->actions().last()->setText(longer);
    const int longerIconsOnly = needed(*probe.bar, Qt::ToolButtonIconOnly, 20);
    const int longerSmall = needed(*probe.bar, Qt::ToolButtonIconOnly, 16);
    QVERIFY2(iconsOnly > small && longerIconsOnly > iconsOnly + 5 && longerSmall <= iconsOnly + 5,
             qPrintable(QStringLiteral("fixture: the longer name needs %1 at icon 20 and %2 at icon 16, the old one %3 and %4")
                            .arg(longerIconsOnly).arg(longerSmall).arg(iconsOnly).arg(small)));
  }
  Host host;
  QToolBar& bar = *host.bar;
  fillBar(bar, 14);
  bar.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  bar.setIconSize(QSize(20, 20));
  KaToolbarFit::install(&bar);
  host.window.show();
  expectStage(host, iconsOnly + 5, Qt::ToolButtonIconOnly, 20);
  if (QTest::currentTestFailed()) return;
  bar.actions().last()->setText(longer);  // the last tool is 「닫기」
  settle();
  expectStage(host, iconsOnly + 5, Qt::ToolButtonIconOnly, 16);
}

void TestToolbarFit::toolbarFit_installTwiceFitsOnce() {
  Host host;
  fillBar(*host.bar, 3);
  KaToolbarFit::install(host.bar);
  KaToolbarFit::install(host.bar);
  KaToolbarFit::install(nullptr);
  QCOMPARE(host.bar->findChildren<QObject*>(QStringLiteral("kaToolbarFit"), Qt::FindDirectChildrenOnly).size(), 1);
}

QTEST_MAIN(TestToolbarFit)
#include "test_toolbar_fit.moc"
