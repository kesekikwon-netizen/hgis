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
#include <QPixmap>
#include <QToolBar>
#include <QToolButton>
#include "app/KaTheme.h"
#include "app/KaToolbarFit.h"

namespace {

// A bar like #subToolbar (MainWindowRibbon.cpp): 20 px icons, the label beside the icon.
void configureBar(QToolBar& bar) {
  bar.setObjectName(QStringLiteral("subToolbar"));
  bar.setIconSize(QSize(20, 20));
  bar.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  bar.setMovable(false);
}

// A caption label, tools with a picture and a Korean name, then two tools without a picture.
void addTools(QToolBar& bar, int tools) {
  QPixmap picture(32, 32);
  picture.fill(Qt::darkGray);
  bar.addWidget(new QLabel(QStringLiteral("  그리기 › ")));
  for (int i = 0; i < tools; ++i) bar.addAction(QIcon(picture), QStringLiteral("도구 이름 %1번").arg(i + 1));
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

void expectStage(QToolBar& bar, int width, Qt::ToolButtonStyle style, int icon) {
  bar.resize(width, bar.sizeHint().height());
  QCoreApplication::processEvents();
  const QString where = QStringLiteral("width %1: style %2 icon %3 needs %4, extension %5")
                            .arg(width).arg(bar.toolButtonStyle()).arg(bar.iconSize().width())
                            .arg(bar.sizeHint().width()).arg(extShown(bar));
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
  QToolBar bar;
  fillBar(bar, 14);
  bar.setAttribute(Qt::WA_DontShowOnScreen);
  const int withText = needed(bar, Qt::ToolButtonTextBesideIcon, 20);
  const int iconsOnly = needed(bar, Qt::ToolButtonIconOnly, 20);
  const int small = needed(bar, Qt::ToolButtonIconOnly, 16);
  QVERIFY2(withText > iconsOnly + 100 && iconsOnly > small + 30, "fixture: the three stages need clearly different widths");
  bar.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  bar.setIconSize(QSize(20, 20));
  KaToolbarFit::install(&bar);
  bar.show();

  expectStage(bar, withText + 30, Qt::ToolButtonTextBesideIcon, 20);  // room: the labels show, icon 20
  if (QTest::currentTestFailed()) return;
  expectStage(bar, withText - 1, Qt::ToolButtonIconOnly, 20);  // short by a pixel: labels hide first
  if (QTest::currentTestFailed()) return;
  expectStage(bar, iconsOnly, Qt::ToolButtonIconOnly, 20);
  if (QTest::currentTestFailed()) return;
  expectStage(bar, iconsOnly - 1, Qt::ToolButtonIconOnly, 16);  // still short: icons shrink
  if (QTest::currentTestFailed()) return;
  expectStage(bar, small, Qt::ToolButtonIconOnly, 16);
  if (QTest::currentTestFailed()) return;
  expectStage(bar, iconsOnly, Qt::ToolButtonIconOnly, 20);  // room again: back in the reverse order
  if (QTest::currentTestFailed()) return;
  expectStage(bar, withText + 30, Qt::ToolButtonTextBesideIcon, 20);
}

// Dragging a window edge resizes the bar a pixel at a time: at no width above the last stage's need may
// the » button show, and the stage only moves one way as the bar narrows (and back as it widens).
void TestToolbarFit::toolbarFit_neverShowsTheExtensionButtonWhileResizing() {
  QToolBar bar;
  fillBar(bar, 14);
  bar.setAttribute(Qt::WA_DontShowOnScreen);
  const int small = needed(bar, Qt::ToolButtonIconOnly, 16);
  const int withText = needed(bar, Qt::ToolButtonTextBesideIcon, 20);
  bar.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  bar.setIconSize(QSize(20, 20));
  KaToolbarFit::install(&bar);
  bar.show();
  int stage = 0;  // 0 labels, 1 icons only, 2 small icons
  for (int width = withText + 60; width >= small; width -= 7) {
    bar.resize(width, bar.sizeHint().height());
    QCoreApplication::processEvents();
    const int now = bar.toolButtonStyle() != Qt::ToolButtonIconOnly ? 0 : bar.iconSize().width() == 20 ? 1 : 2;
    QVERIFY2(now >= stage && !extShown(bar), qPrintable(QStringLiteral("width %1 stage %2 after %3, extension %4").arg(width).arg(now).arg(stage).arg(extShown(bar))));
    stage = now;
  }
  QCOMPARE(stage, 2);
  for (int width = small; width <= withText + 60; width += 7) {
    bar.resize(width, bar.sizeHint().height());
    QCoreApplication::processEvents();
    const int now = bar.toolButtonStyle() != Qt::ToolButtonIconOnly ? 0 : bar.iconSize().width() == 20 ? 1 : 2;
    QVERIFY2(now <= stage && !extShown(bar), qPrintable(QStringLiteral("width %1 stage %2 after %3, extension %4").arg(width).arg(now).arg(stage).arg(extShown(bar))));
    stage = now;
  }
  QCOMPARE(stage, 0);
}

// #subToolbar is cleared and refilled whenever another tool group opens, and is hidden in between.
void TestToolbarFit::toolbarFit_followsActionsAddedAndShownLater() {
  int small = 0;
  int withText = 0;
  {
    QToolBar probe;
    fillBar(probe, 14);
    small = needed(probe, Qt::ToolButtonIconOnly, 16);
    withText = needed(probe, Qt::ToolButtonTextBesideIcon, 20);
  }
  QToolBar bar;
  bar.setAttribute(Qt::WA_DontShowOnScreen);
  configureBar(bar);
  KaToolbarFit::install(&bar);
  bar.resize(small + 10, 60);
  bar.show();
  QCoreApplication::processEvents();
  addTools(bar, 14);  // filled after install, while the bar is already narrow
  QCoreApplication::processEvents();
  QVERIFY2(bar.toolButtonStyle() == Qt::ToolButtonIconOnly && bar.iconSize() == QSize(16, 16) && !extShown(bar),
           qPrintable(QStringLiteral("needs %1 of %2").arg(bar.sizeHint().width()).arg(bar.width())));
  bar.clear();
  bar.hide();
  addTools(bar, 14);
  bar.resize(withText + 20, 60);
  bar.show();
  QCoreApplication::processEvents();
  QVERIFY2(bar.toolButtonStyle() == Qt::ToolButtonTextBesideIcon && bar.iconSize() == QSize(20, 20) && !extShown(bar),
           qPrintable(QStringLiteral("needs %1 of %2").arg(bar.sizeHint().width()).arg(bar.width())));
}

void TestToolbarFit::toolbarFit_installTwiceFitsOnce() {
  QToolBar bar;
  fillBar(bar, 3);
  KaToolbarFit::install(&bar);
  KaToolbarFit::install(&bar);
  KaToolbarFit::install(nullptr);
  QCOMPARE(bar.findChildren<QObject*>(QStringLiteral("kaToolbarFit"), Qt::FindDirectChildrenOnly).size(), 1);
}

QTEST_MAIN(TestToolbarFit)
#include "test_toolbar_fit.moc"
