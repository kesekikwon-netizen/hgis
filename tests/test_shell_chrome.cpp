// P5 shell-chrome: status-bar snap/unsaved chips, the 「조사 열림」 tab badge and the drawing
// guide band with undo/redo (the splitter and rail inset are in test_shell_focus.cpp). No QGIS.
// KA_HGIS_QA_OUTPUT_DIR saves shell-chrome.png (band over a host, rail moved down, badge, chips).
#include <QtTest>

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QLabel>
#include <QSignalSpy>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include "app/KaChip.h"
#include "app/KaDrawGuideBand.h"
#include "app/KaIcons.h"
#include "app/KaLayerOpacityRail.h"
#include "app/KaStatusBar.h"
#include "app/KaSurveyBadge.h"
#include "app/KaTheme.h"

namespace {
using Tone = KaChip::Tone;
}  // namespace

class TestShellChrome : public QObject {
  Q_OBJECT
 private slots:
  void statusBar_existingApiUnchanged() {
    KaStatusBar bar;
    auto* xy = bar.findChild<QLabel*>(QStringLiteral("xyReadout"));
    QVERIFY(xy);
    bar.setCoordinate(147999.305, 98157.62);
    QVERIFY(xy->text().startsWith(QStringLiteral("X ")) && xy->text().contains(QStringLiteral("999")));
    bar.clearCoordinate();
    QCOMPARE(xy->text(), QStringLiteral("X —    Y —"));
    QVERIFY(xy->isVisibleTo(&bar));
    bar.setInstrumentsVisible(false, false);
    QVERIFY(!xy->isVisibleTo(&bar));
    QVERIFY(bar.findChild<QToolButton*>(QStringLiteral("crsButton")));
    QVERIFY(bar.findChild<QLabel*>(QStringLiteral("uploadCrsChip")));
    QVERIFY(bar.findChild<QToolButton*>(QStringLiteral("renderToggle")));
    QVERIFY(bar.findChild<QComboBox*>(QStringLiteral("scaleCombo")));
    QVERIFY(bar.findChild<QToolButton*>(QStringLiteral("crsButton"))->text().contains(QStringLiteral("5186")));
  }

  void statusBar_snapChipTextAndTone() {
    KaStatusBar bar;
    KaChip* snap = bar.snapChip();
    QVERIFY(snap);
    QCOMPARE(snap->objectName(), QStringLiteral("snapChip"));
    QVERIFY2(!snap->isVisibleTo(&bar), "unknown until the window reports a state");
    bar.setSnapState(true);
    QVERIFY(snap->isVisibleTo(&bar));
    QCOMPARE(snap->text(), QStringLiteral("자석 켬"));
    QCOMPARE(snap->tone(), Tone::Accent);
    QCOMPARE(snap->glyph(), QStringLiteral("snap"));
    QVERIFY(snap->hasGlyph());
    bar.setSnapState(false);
    QCOMPARE(snap->text(), QStringLiteral("자석 끔"));
    QCOMPARE(snap->tone(), Tone::Neutral);
    QVERIFY(!bar.snapState());
    QVERIFY(snap->toolTip().contains(QStringLiteral("자석 설정")));
  }

  void statusBar_unsavedChipHiddenAtZero() {
    KaStatusBar bar;
    KaChip* chip = bar.unsavedChip();
    QVERIFY(chip);
    QCOMPARE(chip->objectName(), QStringLiteral("unsavedChip"));
    QVERIFY(!chip->isVisibleTo(&bar));
    bar.setUnsavedCount(0, false);
    QVERIFY(!chip->isVisibleTo(&bar));
    bar.setUnsavedCount(3, false);
    QVERIFY(chip->isVisibleTo(&bar));
    QCOMPARE(chip->text(), QStringLiteral("저장 안 됨 3건"));
    QCOMPARE(chip->tone(), Tone::Warn);
    QVERIFY(chip->hasGlyph());
    bar.setUnsavedCount(0, true);
    QVERIFY(chip->isVisibleTo(&bar));
    QCOMPARE(chip->text(), QStringLiteral("저장 안 됨"));
    EditBufferSummary::Summary summary;
    summary.features = 5;
    summary.projectDirty = true;
    bar.setUnsavedCount(summary);
    QCOMPARE(chip->text(), QStringLiteral("저장 안 됨 5건"));
    bar.setUnsavedCount(0, false);
    QVERIFY(!chip->isVisibleTo(&bar));
  }

  void statusBar_chipsFollowTabRules() {
    KaStatusBar bar;
    bar.setSnapState(true);
    bar.setUnsavedCount(1, false);
    bar.setInstrumentsVisible(true, true);  // map tab with an open survey
    bar.setCrsChipsVisible(true);
    QVERIFY(bar.snapChip()->isVisibleTo(&bar) && bar.unsavedChip()->isVisibleTo(&bar));
    bar.setInstrumentsVisible(false, true);  // drawing tab: scale only, no snapping
    QVERIFY(!bar.snapChip()->isVisibleTo(&bar));
    QVERIFY(bar.unsavedChip()->isVisibleTo(&bar));
    bar.setCrsChipsVisible(false);  // home: no survey chips at all
    QVERIFY(!bar.unsavedChip()->isVisibleTo(&bar));
    bar.setInstrumentsVisible(true, true);
    bar.setCrsChipsVisible(true);
    QVERIFY(bar.snapChip()->isVisibleTo(&bar) && bar.unsavedChip()->isVisibleTo(&bar));
    bar.setUnsavedCount(0, false);
    QVERIFY2(!bar.unsavedChip()->isVisibleTo(&bar), "zero stays hidden on the map tab too");
  }

  void badge_hiddenWithoutSurvey() {
    KaSurveyBadge badge;
    QCOMPARE(badge.objectName(), QStringLiteral("surveyBadge"));
    QVERIFY(badge.isHidden());
    badge.setSurvey(QStringLiteral("광령리1"), false);
    QVERIFY(!badge.isHidden());
    badge.setSurvey(QString(), true);
    QVERIFY(badge.isHidden());
    QCOMPARE(badge.text(), QString());
    QCOMPARE(KaSurveyBadge::textFor(QStringLiteral("  "), false), QString());
  }

  void badge_textAndTone() {
    KaSurveyBadge badge;
    badge.setSurvey(QStringLiteral("광령리1"), false);
    QCOMPARE(badge.text(), QStringLiteral("조사 열림 · 광령리1"));
    QCOMPARE(badge.tone(), Tone::Ok);
    QCOMPARE(badge.glyph(), QStringLiteral("check"));
    badge.setSurvey(QStringLiteral("광령리1"), true);
    QCOMPARE(badge.text(), QStringLiteral("광령리1 · 저장 안 됨"));
    QCOMPARE(badge.tone(), Tone::Warn);
    QCOMPARE(badge.glyph(), QStringLiteral("warn"));
    QVERIFY(badge.unsaved() && badge.surveyName() == QStringLiteral("광령리1"));
    QCOMPARE(badge.sizeHint().height(), KaTheme::buttonMetrics().chipHeight);
    QVERIFY(badge.sizeHint().width() > 80);
  }

  void badge_clickEmits() {
    KaSurveyBadge badge;
    QSignalSpy clicked(&badge, &KaSurveyBadge::clicked);
    badge.setSurvey(QStringLiteral("광령리1"), false);
    badge.show();
    QTest::mouseClick(&badge, Qt::LeftButton);
    QCOMPARE(clicked.count(), 1);
    QTest::keyClick(&badge, Qt::Key_Space);
    QCOMPARE(clicked.count(), 2);
  }

  void band_collapsesWithoutTool() {
    QWidget host;
    host.resize(900, 600);
    host.show();
    QVERIFY(QTest::qWaitForWindowExposed(&host));
    KaDrawGuideBand band(&host);
    band.show();
    QVERIFY(!band.hasTool());
    QVERIFY(band.pill()->isHidden());
    QCOMPARE(band.size(), QSize(36 + 8 + 36, 36));
    QCOMPARE(band.pos(), QPoint(KaDrawGuideBand::kFoldedLeft, 12));
    QVERIFY2(band.x() >= 10 + 260, "folded buttons clear the opacity card at 10,10 (260 wide)");
    QCOMPARE(band.topInset(), 0);
    QVERIFY(!band.undoButton()->isEnabled() && !band.redoButton()->isEnabled());
  }

  void band_showsIconTitleAndHint() {
    QWidget window;
    auto* rows = new QVBoxLayout(&window);
    rows->setContentsMargins(0, 0, 0, 0);
    auto* tabs = new QTabWidget(&window);
    auto* host = new QWidget(tabs);
    host->setStyleSheet(QStringLiteral("background: #DCE6EE;"));
    tabs->addTab(host, QStringLiteral("지도"));
    auto* badge = new KaSurveyBadge(tabs);
    tabs->setCornerWidget(badge, Qt::TopRightCorner);
    badge->setSurvey(QStringLiteral("광령리1"), true);
    auto* bar = new KaStatusBar(&window);
    bar->setSnapState(true);
    bar->setUnsavedCount(3, true);
    rows->addWidget(tabs, 1);
    rows->addWidget(bar);
    window.resize(900, 300);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    KaLayerOpacityRail rail(host);
    rail.setTarget(QStringLiteral("위성"), true);
    rail.show();  // created after the host was shown, like the band below
    KaDrawGuideBand band(host);
    connect(&band, &KaDrawGuideBand::topInsetChanged, &rail, &KaLayerOpacityRail::setTopInset);
    band.show();
    band.setTool(QStringLiteral("draw_poly"), QStringLiteral("그리기 · 유구 면"),
                 QStringLiteral("클릭으로 점을 찍고 우클릭으로 마칩니다 (Esc 취소)"));
    QVERIFY(band.hasTool() && !band.pill()->isHidden());
    QCOMPARE(band.pos(), QPoint(12, 12));
    QCOMPARE(band.height(), 36);
    auto* title = band.findChild<QLabel*>(QStringLiteral("drawGuideTitle"));
    auto* hint = band.findChild<QLabel*>(QStringLiteral("drawGuideHint"));
    auto* glyph = band.findChild<QLabel*>(QStringLiteral("drawGuideGlyph"));
    QCOMPARE(title->text(), QStringLiteral("그리기 · 유구 면"));
    QCOMPARE(hint->text(), QStringLiteral("— 클릭으로 점을 찍고 우클릭으로 마칩니다 (Esc 취소)"));
    QVERIFY(!glyph->pixmap().isNull() && !glyph->isHidden());
    QCOMPARE(band.undoButton()->x(), band.pill()->x() + band.pill()->width() + 8);
    QCOMPARE(band.redoButton()->x(), band.undoButton()->x() + 36 + 8);
    QCOMPARE(rail.pos(), QPoint(10, 10 + 56));
    QVERIFY2(!band.geometry().intersects(rail.geometry()), "the card sits below the band");
    QVERIFY(rail.isVisible() && band.isVisible());
    QTest::qWait(50);
    const QString out = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    if (!out.isEmpty()) QVERIFY(window.grab().save(QDir(out).filePath(QStringLiteral("shell-chrome.png"))));
    band.setTool(QStringLiteral("gps"), QStringLiteral("그리기 · 기준점"), QString());
    QVERIFY(hint->isHidden() && band.hasTool());
    band.setTool(QString(), QString(), QString());
    QVERIFY(!band.hasTool() && band.pill()->isHidden());
    QCOMPARE(rail.pos(), QPoint(10, 10));
  }

  void band_undoRedoFollowActions() {
    QWidget host;
    KaDrawGuideBand band(&host);
    QAction undo(KaIcons::icon(QStringLiteral("undo")), QStringLiteral("되돌리기"));
    undo.setToolTip(QStringLiteral("마지막 그리기·정점·삭제를 되돌립니다 (Ctrl+Z)"));
    QAction redo(QStringLiteral("다시 실행"));  // no icon: the band supplies the glyph
    undo.setEnabled(false);
    band.setActions(&undo, &redo);
    QVERIFY(!band.undoButton()->isEnabled() && band.redoButton()->isEnabled());
    undo.setEnabled(true);
    QVERIFY(band.undoButton()->isEnabled());
    QCOMPARE(band.undoButton()->toolTip(), undo.toolTip());
    QSignalSpy fired(&undo, &QAction::triggered);
    band.undoButton()->click();
    QCOMPARE(fired.count(), 1);
    QVERIFY(!band.redoButton()->icon().isNull());
    redo.setEnabled(false);  // a change re-copies the action's (null) icon; the glyph stays
    QVERIFY(!band.redoButton()->isEnabled() && !band.redoButton()->icon().isNull());
    QCOMPARE(band.undoButton()->size(), QSize(36, 36));
    QCOMPARE(band.undoButton()->toolButtonStyle(), Qt::ToolButtonIconOnly);
  }

  void band_emitsTopInset() {
    QWidget host;
    host.resize(600, 400);
    host.show();
    QVERIFY(QTest::qWaitForWindowExposed(&host));
    KaDrawGuideBand band(&host);
    band.show();
    QSignalSpy inset(&band, &KaDrawGuideBand::topInsetChanged);
    band.setTool(QStringLiteral("measure"), QStringLiteral("줄자"), QStringLiteral("두 점을 클릭하면 거리가 나옵니다"));
    QCOMPARE(inset.count(), 1);
    QCOMPARE(inset.last().at(0).toInt(), 56);
    QCOMPARE(band.topInset(), 56);
    band.hide();
    QCOMPARE(inset.count(), 2);
    QCOMPARE(inset.last().at(0).toInt(), 0);
    band.show();
    QCOMPARE(inset.count(), 3);
    QCOMPARE(inset.last().at(0).toInt(), 56);
    band.setTool(QString(), QString(), QString());
    QCOMPARE(inset.count(), 4);
    QCOMPARE(inset.last().at(0).toInt(), 0);
    band.setTool(QString(), QString(), QString());
    QCOMPARE(inset.count(), 4);  // unchanged: no repeat
  }
};

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  KaTheme::apply(&app);  // the capture shows the Strata chrome, not Fusion
  TestShellChrome test;
  return QTest::qExec(&test, argc, argv);
}

#include "test_shell_chrome.moc"
