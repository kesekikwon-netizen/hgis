// Main-window shell helpers (Strata evaluation, package I): reason tooltips on grey chips
// (F039), startup failure notices (F075/F078), coordinate-grid corner bar (F087), crash dump
// retention (F113), opt-in map room (F149) and the place-search busy text (F223).
#include <QtTest>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFontMetrics>
#include <QProgressDialog>
#include <QSignalSpy>
#include <QSplitter>
#include <QTemporaryDir>
#include <QToolButton>
#include <QVBoxLayout>
#include <memory>

#include "app/KaCrashGuard.h"
#include "app/KaShellFocus.h"
#include "app/KaShellGridControls.h"
#include "app/KaShellStartup.h"
#include "app/KaShellUi.h"

class TestShell : public QObject {
  Q_OBJECT
private slots:
  void crashPrune_keepsNewestAndLeavesOtherFiles();
  void crashDump_skipsIndirectHeap();
  void reason_actionGetsReasonAndGetsOriginalBack();
  void reason_newTooltipWhileDisabledBecomesOriginal();
  void reason_chipFollowsItsAction();
  void searchProgress_saysWhatIsSearched();
  void startup_missingQgisExplainsAndOffersLogs();
  void startup_missingProvidersNamesTheEffect();
  void grid_kindIsExplicitAndSurvivesValueEdits();
  void grid_valueEditsAreDebouncedAndQuiet();
  void grid_hiddenGridStaysQuietAndDetailsStayNarrow();
  void focus_defaultLeftWidthIsRatioCapped();
  void focus_leftPanelFoldsAndComesBack();
  void focus_mapFocusHidesRibbonUntilToggledBack();
};

void TestShell::crashPrune_keepsNewestAndLeavesOtherFiles() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  auto touch = [&](const QString& name) {
    QFile f(dir.filePath(name));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("x");
  };
  for (int i = 1; i <= 8; ++i) touch(QStringLiteral("crash-20260901-0000%1.dmp").arg(i, 2, 10, QLatin1Char('0')));
  for (int i = 1; i <= 25; ++i) touch(QStringLiteral("crash-20260901-0000%1.log").arg(i, 2, 10, QLatin1Char('0')));
  touch(QStringLiteral("session.log"));
  touch(QStringLiteral("other.dmp"));
  QCOMPARE(KaCrashGuard::pruneCrashFiles(dir.path(), 5, 20), 3 + 5);
  const QDir d(dir.path());
  QCOMPARE(d.entryList({QStringLiteral("crash-*.dmp")}, QDir::Files).size(), 5);
  QCOMPARE(d.entryList({QStringLiteral("crash-*.log")}, QDir::Files).size(), 20);
  QVERIFY2(d.exists(QStringLiteral("crash-20260901-000008.dmp")), "newest dump stays");
  QVERIFY2(!d.exists(QStringLiteral("crash-20260901-000003.dmp")), "oldest dumps go");
  QVERIFY2(d.exists(QStringLiteral("session.log")) && d.exists(QStringLiteral("other.dmp")),
           "only crash-* files are pruned");
  QCOMPARE(KaCrashGuard::pruneCrashFiles(dir.path(), 5, 20), 0);
  QCOMPARE(KaCrashGuard::pruneCrashFiles(QString()), 0);
}

void TestShell::crashDump_skipsIndirectHeap() {
#ifdef Q_OS_WIN
  const unsigned long flags = KaCrashGuard::miniDumpTypeFlags();
  QVERIFY2((flags & 0x00000040UL) == 0, "MiniDumpWithIndirectlyReferencedMemory must stay off");
  QVERIFY2((flags & 0x00001000UL) != 0, "thread info keeps the dump useful");
#else
  QCOMPARE(KaCrashGuard::miniDumpTypeFlags(), 0UL);
#endif
}

void TestShell::reason_actionGetsReasonAndGetsOriginalBack() {
  QAction action(QStringLiteral("선택"), nullptr);
  action.setToolTip(QStringLiteral("그린 도형을 선택합니다 (Ctrl+1)"));
  const QString why = KaShellUi::needSurveyReason();
  QVERIFY(why.contains(QStringLiteral("새 조사")));
  KaShellUi::setEnabledWithReason(&action, false, why);
  QVERIFY(!action.isEnabled());
  QCOMPARE(action.toolTip(), QStringLiteral("그린 도형을 선택합니다 (Ctrl+1)\n") + why);
  KaShellUi::setEnabledWithReason(&action, false, why);  // repeated sync must not stack lines
  QCOMPARE(action.toolTip(), QStringLiteral("그린 도형을 선택합니다 (Ctrl+1)\n") + why);
  KaShellUi::setEnabledWithReason(&action, true, why);
  QVERIFY(action.isEnabled());
  QCOMPARE(action.toolTip(), QStringLiteral("그린 도형을 선택합니다 (Ctrl+1)"));
  KaShellUi::setEnabledWithReason(static_cast<QAction*>(nullptr), false, why);  // no crash
}

void TestShell::reason_newTooltipWhileDisabledBecomesOriginal() {
  QToolButton button;
  button.setToolTip(QStringLiteral("주변유적 500M·1000M 표시하기"));
  KaShellUi::setEnabledWithReason(&button, false, KaShellUi::needSurveyReason());
  button.setToolTip(QStringLiteral("새 설명"));
  KaShellUi::setEnabledWithReason(&button, true, KaShellUi::needSurveyReason());
  QVERIFY(button.isEnabled());
  QCOMPARE(button.toolTip(), QStringLiteral("새 설명"));
}

void TestShell::reason_chipFollowsItsAction() {
  QWidget host;
  auto* action = new QAction(QStringLiteral("GeoTIFF"), &host);
  action->setToolTip(QStringLiteral("현재 지도를 GeoTIFF로 저장합니다"));
  auto* chip = new QToolButton(&host);
  chip->setDefaultAction(action);
  KaShellUi::setEnabledWithReason(action, false, KaShellUi::mapTabOnlyReason());
  QVERIFY(!chip->isEnabled());
  QVERIFY(chip->toolTip().contains(QStringLiteral("「지도」 탭")));
  KaShellUi::setEnabledWithReason(action, true, KaShellUi::mapTabOnlyReason());
  QVERIFY(chip->isEnabled());
  QCOMPARE(chip->toolTip(), QStringLiteral("현재 지도를 GeoTIFF로 저장합니다"));
}

void TestShell::searchProgress_saysWhatIsSearched() {
  QWidget parent;
  std::unique_ptr<QProgressDialog> dialog(KaShellUi::createSearchProgress(&parent, QStringLiteral("안동시 법흥동")));
  QVERIFY(dialog);
  QCOMPARE(dialog->windowTitle(), QStringLiteral("위치 검색"));
  QCOMPARE(dialog->windowModality(), Qt::NonModal);
  QVERIFY(dialog->labelText().contains(QStringLiteral("「안동시 법흥동」 위치를 찾는 중")));
  QVERIFY2(!dialog->labelText().contains(QStringLiteral("자료 자료")) &&
               !dialog->labelText().contains(QStringLiteral("내려받")),
           "search is not a download");
  const QString longText = KaShellUi::searchProgressText(QString(80, QChar(0xAC00)));
  QVERIFY2(longText.contains(QChar(0x2026)) && longText.size() < 80, "long queries are shortened");
}

void TestShell::startup_missingQgisExplainsAndOffersLogs() {
  const KaUserError::Spec spec = KaShellStartup::missingQgisSpec(QStringLiteral("E:/Strata"));
  QVERIFY(spec.what.contains(QStringLiteral("QGIS")));
  QVERIFY(spec.why.contains(QDir::toNativeSeparators(QStringLiteral("E:/Strata/apps/qgis-dev"))));
  QVERIFY(spec.how.contains(QStringLiteral("폴더 전체")));
  QVERIFY(spec.how.contains(QStringLiteral("조사 파일은 변경하지 않았습니다")));
  // The one extra button is KaUserError's own 「로그 폴더 열기」, not a second action.
  QVERIFY(spec.actionLabel.isEmpty());
  const QString before = KaUserError::logFolder();
  KaUserError::setLogFolder(QString());
  qputenv("KA_HGIS_LOG_DIR", QByteArray("C:/ka-hgis-shell-test-logs"));
  KaShellStartup::ensureLogFolder();
  QCOMPARE(KaUserError::logFolder(), QStringLiteral("C:/ka-hgis-shell-test-logs"));
  KaUserError::setLogFolder(QStringLiteral("D:/kept"));
  KaShellStartup::ensureLogFolder();
  QCOMPARE(KaUserError::logFolder(), QStringLiteral("D:/kept"));
  KaUserError::setLogFolder(before);
  qunsetenv("KA_HGIS_LOG_DIR");
}

void TestShell::startup_missingProvidersNamesTheEffect() {
  QVERIFY(KaShellStartup::missingProviders({QStringLiteral("wms"), QStringLiteral("ogr"),
                                            QStringLiteral("gdal"), QStringLiteral("memory")})
              .isEmpty());
  const QStringList missing =
      KaShellStartup::missingProviders({QStringLiteral("ogr"), QStringLiteral("gdal")});
  QCOMPARE(missing, QStringList{QStringLiteral("wms")});
  const KaUserError::Spec spec = KaShellStartup::missingProvidersSpec(missing, QStringLiteral("E:/Strata/apps/qgis-dev"));
  QVERIFY(spec.what.contains(QStringLiteral("배경 지도")));
  QVERIFY(!spec.what.contains(QStringLiteral("GPKG")));
  QVERIFY(spec.why.contains(QStringLiteral("wms")));
  QVERIFY(spec.actionLabel.isEmpty());
}

void TestShell::grid_kindIsExplicitAndSurvivesValueEdits() {
  KaShellGridControls grid;
  QSignalSpy spy(&grid, &KaShellGridControls::settingsChanged);
  QVERIFY(!grid.settings().enabled);
  grid.enabledCheck()->setChecked(true);
  QCOMPARE(spy.count(), 1);
  QCOMPARE(spy.takeFirst().at(0).toBool(), true);
  QVERIFY2(!grid.settings().geographic, "metre grid is the default kind");
  grid.kindCombo()->setCurrentIndex(1);
  QCOMPARE(spy.count(), 1);
  QCOMPARE(spy.takeFirst().at(0).toBool(), true);
  QVERIFY(grid.settings().geographic);
  QVERIFY2(!grid.stepSpin()->isEnabled() && !grid.rotationSpin()->isEnabled(),
           "경위도 grid picks its own spacing and is not rotated");
  grid.widthSpin()->setValue(2.0);
  QTRY_COMPARE(spy.count(), 1);
  QVERIFY2(grid.settings().geographic, "a value edit must not switch the grid kind");
  QCOMPARE(grid.settings().lineWidth, 2.0);
  grid.setColor(QColor(0xD9, 0x2B, 0x2B));
  QCOMPARE(grid.settings().color, QColor(0xD9, 0x2B, 0x2B));
}

void TestShell::grid_valueEditsAreDebouncedAndQuiet() {
  KaShellGridControls grid;
  grid.enabledCheck()->setChecked(true);
  QSignalSpy spy(&grid, &KaShellGridControls::settingsChanged);
  grid.stepSpin()->setValue(25.0);
  grid.stepSpin()->setValue(30.0);
  grid.rotationSpin()->setValue(15.0);
  QCOMPARE(spy.count(), 0);
  QVERIFY(grid.applyPending());
  QTRY_COMPARE(spy.count(), 1);
  QCOMPARE(spy.takeFirst().at(0).toBool(), false);
  QCOMPARE(grid.settings().stepMeters, 30.0);
  QCOMPARE(grid.settings().rotationDeg, 15.0);
  QCOMPARE(grid.stepMeters(), 30.0);
}

void TestShell::grid_hiddenGridStaysQuietAndDetailsStayNarrow() {
  KaShellGridControls grid;
  QSignalSpy spy(&grid, &KaShellGridControls::settingsChanged);
  grid.stepSpin()->setValue(50.0);
  QVERIFY(!grid.applyPending());
  QCOMPARE(spy.count(), 0);
  QCOMPARE(grid.stepMeters(), 50.0);  // trench snapping still reads the spacing
  grid.enabledCheck()->setChecked(true);
  grid.adjustSize();
  // A 1366 laptop map is about 690 px wide with a 176 px scale bar at the left, so these
  // controls + the 원점 button + bar margins must stay under about 500 px (uncapped: 440 here).
  QVERIFY2(grid.sizeHint().width() < 400, qPrintable(QString::number(grid.sizeHint().width())));
  // The caps still leave room for the values people type (four-digit spacing, 360.0°).
  const QFontMetrics fm(grid.stepSpin()->font());
  QVERIFY(grid.stepSpin()->maximumWidth() >= fm.height() * 7 && grid.rotationSpin()->maximumWidth() >= fm.height() * 6);
}

void TestShell::focus_defaultLeftWidthIsRatioCapped() {
  QCOMPARE(KaShellFocus::defaultLeftPanelWidth(1093), 240);
  QCOMPARE(KaShellFocus::defaultLeftPanelWidth(1920), KaShellFocus::kMaxLeftWidth);
  QCOMPARE(KaShellFocus::defaultLeftPanelWidth(640), KaShellFocus::kMinLeftWidth);
  QVERIFY(KaShellFocus::defaultLeftPanelWidth(1366) <= KaShellFocus::kMaxLeftWidth);
}

void TestShell::focus_leftPanelFoldsAndComesBack() {
  QSplitter split(Qt::Horizontal);
  auto* left = new QWidget(&split);
  auto* map = new QWidget(&split);
  split.addWidget(left);
  split.addWidget(map);
  split.resize(1000, 400);
  KaShellFocus focus(&split);
  split.show();
  QVERIFY(QTest::qWaitForWindowExposed(&split));
  focus.applyDefaultWidthOnce();
  const int expected = KaShellFocus::defaultLeftPanelWidth(split.width());
  QVERIFY2(qAbs(split.sizes().value(0) - expected) <= split.handleWidth() + 2,
           qPrintable(QStringLiteral("%1 vs %2").arg(split.sizes().value(0)).arg(expected)));
  split.setSizes({300, 700});
  const int userLeft = split.sizes().value(0);
  focus.applyDefaultWidthOnce();  // once only: the user's width stays
  QCOMPARE(split.sizes().value(0), userLeft);
  QSignalSpy spy(&focus, &KaShellFocus::changed);
  focus.toggleLeftPanel();
  QVERIFY(focus.leftPanelCollapsed());
  QVERIFY(!left->isVisibleTo(&split));
  QVERIFY(spy.takeFirst().at(0).toString().contains(KaShellFocus::leftPanelKey()));
  focus.toggleLeftPanel();
  QVERIFY(!focus.leftPanelCollapsed());
  QVERIFY(left->isVisibleTo(&split));
  QVERIFY(qAbs(split.sizes().value(0) - userLeft) <= 2);
}

void TestShell::focus_mapFocusHidesRibbonUntilToggledBack() {
  QWidget window;
  auto* rows = new QVBoxLayout(&window);
  auto* ribbon = new QWidget(&window);
  ribbon->setMinimumHeight(40);
  auto* split = new QSplitter(Qt::Horizontal, &window);
  auto* left = new QWidget(split);
  split->addWidget(left);
  split->addWidget(new QWidget(split));
  rows->addWidget(ribbon);
  rows->addWidget(split, 1);
  window.resize(900, 500);
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  KaShellFocus focus(split);
  focus.markUserWidthRestored();
  const QList<int> before = split->sizes();
  focus.applyDefaultWidthOnce();
  QCOMPARE(split->sizes(), before);  // a restored layout wins over the ratio default
  focus.setChrome({ribbon});
  focus.toggleMapFocus();
  QVERIFY(focus.mapFocused());
  QVERIFY(!ribbon->isVisible());
  QVERIFY(!left->isVisible());
  focus.toggleMapFocus();
  QVERIFY(ribbon->isVisible());
  QVERIFY(left->isVisible());
  // A panel the user folded before stays folded after leaving map focus.
  focus.setLeftPanelCollapsed(true);
  focus.setMapFocused(true);
  focus.setMapFocused(false);
  QVERIFY(ribbon->isVisible());
  QVERIFY(focus.leftPanelCollapsed());
  // Closing the window brings everything back so the saved state never hides the ribbon.
  focus.setMapFocused(true);
  focus.restoreAll();
  QVERIFY(!focus.mapFocused() && !focus.leftPanelCollapsed());
  QVERIFY(ribbon->isVisible() && left->isVisible());
}

QTEST_MAIN(TestShell)
#include "test_shell.moc"
