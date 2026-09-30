// P3 home: recent rows (state chip, dots, header, click), continue badge, 작업 순서, 연결 상태,
// compact hero. No network; isolated settings; probe off. KA_HGIS_QA_OUTPUT_DIR saves home-1366.png.
#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTemporaryDir>

#include <qgsapplication.h>

#include "app/KaChip.h"
#include "app/KaHomeConnectionCard.h"
#include "app/KaHomeGuideCard.h"
#include "app/KaHomeRecentCard.h"
#include "app/KaRecoverySnapshots.h"
#include "app/KaStartPage.h"
#include "app/KaTheme.h"
#include "core/RecentSurveys.h"
#include "core/SurveyFacts.h"

namespace {

QString touch(const QString& path) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) return {};
  file.write("fixture");
  return QFileInfo(path).absoluteFilePath();
}

QTableWidget* table(KaStartPage& page) { return page.findChild<QTableWidget*>(QStringLiteral("recentSurveyList")); }

}  // namespace

class TestHomeCards : public QObject {
  Q_OBJECT
  QTemporaryDir m_dir;

private slots:
  void cleanup() { RecentSurveys::userSettings().clear(); }

  void recentRows_showStateChipAndThreeDots() {
    const QString saved = touch(m_dir.filePath(QStringLiteral("a/저장됨.gpkg")));
    const QString unsaved = touch(m_dir.filePath(QStringLiteral("a/미저장.gpkg")));
    const QString gone = touch(m_dir.filePath(QStringLiteral("a/없음.gpkg")));
    const QString copy = touch(m_dir.filePath(QStringLiteral("a/복구사본/조사복구_1/복구조사.gpkg")));
    {
      QSettings st = RecentSurveys::userSettings();
      RecentSurveys::remember(st, gone, QStringLiteral("없음"));  // remember() needs the file; drop it after
      RecentSurveys::remember(st, unsaved, QStringLiteral("미저장"));
      RecentSurveys::remember(st, saved, QStringLiteral("저장됨"));
      KaRecoverySnapshots::rememberUnsaved(st, unsaved, copy);
      SurveyFacts::rememberCounts(st, saved, 1, 2);
      SurveyFacts::rememberCheck(st, saved, 0, 1);
    }
    QVERIFY(QFile::remove(gone));
    KaStartPage page;
    auto* list = table(page);
    QVERIFY(list && list->rowCount() == 3);  // newest first: 저장됨, 미저장, 없음
    QVERIFY(list->item(0, 1)->text() == QStringLiteral("저장됨") &&
            list->item(0, 1)->data(KaHomeRecentCard::kToneRole).toString() == QStringLiteral("ok"));
    QVERIFY(list->item(1, 1)->text() == QStringLiteral("저장 안 됨") &&
            list->item(1, 1)->data(KaHomeRecentCard::kToneRole).toString() == QStringLiteral("warn"));
    QVERIFY(list->item(2, 1)->text() == QStringLiteral("원본 없음") &&
            list->item(2, 1)->data(KaHomeRecentCard::kToneRole).toString() == QStringLiteral("danger"));
    const QList<int> dots = list->item(0, 2)->data(KaHomeRecentCard::kDotsRole).value<QList<int>>();
    QCOMPARE(dots, (QList<int>{KaHomeRecentCard::DotOk, KaHomeRecentCard::DotOk, KaHomeRecentCard::DotWarn}));
    QVERIFY(list->item(0, 0)->data(KaHomeRecentCard::kThumbRole).toString() == QStringLiteral("survey_thumb") &&
            list->item(0, 2)->toolTip().contains(QStringLiteral("구역 1 · 유구 2")));
    page.resize(1366, 740);
    page.show();
    QVERIFY(QTest::qWaitForWindowExposed(&page));
    QTRY_VERIFY(page.findChild<QLabel*>(QStringLiteral("startConnState")));  // showEvent refresh ran
    auto* right = page.findChild<QScrollArea*>(QStringLiteral("startRightColumn"));
    QTRY_VERIFY(right->widget()->height() > right->viewport()->height());  // both cards: the column scrolls, never squeezes
    const QString out = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");  // three chips, dots, both cards
    if (!out.isEmpty()) QVERIFY(page.grab().save(QDir(out).filePath(QStringLiteral("home-1366.png"))));
  }

  void recentRows_missingOriginalShowsDashAndNoFacts() {
    const QString gone = m_dir.filePath(QStringLiteral("b/사라짐.gpkg"));
    touch(gone);
    {
      QSettings st = RecentSurveys::userSettings();
      RecentSurveys::remember(st, gone, QStringLiteral("사라짐"));
      SurveyFacts::rememberCounts(st, gone, 3, 3);
    }
    QVERIFY(QFile::remove(gone));
    KaStartPage page;
    auto* list = table(page);
    QVERIFY(list && list->rowCount() == 1);
    QCOMPARE(list->item(0, 1)->text(), QStringLiteral("원본 없음"));
    QCOMPARE(list->item(0, 1)->data(KaHomeRecentCard::kToneRole).toString(), QStringLiteral("danger"));
    QVERIFY(list->item(0, 2)->data(KaHomeRecentCard::kDotsRole).value<QList<int>>().isEmpty());  // 「—」
    QVERIFY(list->item(0, 0)->data(KaHomeRecentCard::kThumbRole).toString() == QStringLiteral("missing") &&
            list->item(0, 3)->text() == QStringLiteral("찾을 수 없음"));
  }

  void recentRows_remotePathIsNotProbed() {
    QVector<RecentSurveys::Item> items;
    items.push_back({QStringLiteral("서버"), QStringLiteral("//server/share/조사.gpkg"), 1, true});
    items.push_back({QStringLiteral("없음"), m_dir.filePath(QStringLiteral("x.gpkg")), 1, false});
    const QString local = touch(m_dir.filePath(QStringLiteral("c/로컬.gpkg")));
    items.push_back({QStringLiteral("로컬"), local, 1, true});
    QCOMPARE(KaStartPage::probeCandidates(items), QStringList{local});
    QVERIFY(RecentSurveys::isRemotePath(QStringLiteral("\\\\server\\share\\x.gpkg")) && !RecentSurveys::isRemotePath(local));
  }

  void recentRows_headerAndRowMetrics() {
    const QString survey = touch(m_dir.filePath(QStringLiteral("d/광령리.gpkg")));
    {
      QSettings st = RecentSurveys::userSettings();
      RecentSurveys::remember(st, survey, QStringLiteral("광령리"));
    }
    KaStartPage page;
    page.resize(1600, 900);
    page.show();
    QVERIFY(QTest::qWaitForWindowExposed(&page));
    auto* list = table(page);
    QVERIFY(list);
    QVERIFY(!list->horizontalHeader()->isHidden());
    QCOMPARE(list->horizontalHeaderItem(2)->text(), QStringLiteral("구역 · 유구 · 검수"));
    QCOMPARE(list->rowHeight(0), KaHomeRecentCard::kRowHeight);
    for (int column = 0; column < 4; ++column) QVERIFY(!list->item(0, column)->text().contains(QStringLiteral("열기")));
    QSignalSpy opened(&page, &KaStartPage::recentOpened);
    QTest::mouseClick(list->viewport(), Qt::LeftButton, {}, list->visualItemRect(list->item(0, 0)).center());
    QTRY_COMPARE(opened.count(), 1);
    QCOMPARE(opened.first().first().toString(), survey);
  }

  void recentRows_filterMatchesStatusWord() {
    const QString a = touch(m_dir.filePath(QStringLiteral("e/하나.gpkg")));
    const QString b = touch(m_dir.filePath(QStringLiteral("e/둘.gpkg")));
    const QString copy = touch(m_dir.filePath(QStringLiteral("e/복구사본/조사복구_1/복구조사.gpkg")));
    {
      QSettings st = RecentSurveys::userSettings();
      RecentSurveys::remember(st, a, QStringLiteral("하나"));
      RecentSurveys::remember(st, b, QStringLiteral("둘"));
      KaRecoverySnapshots::rememberUnsaved(st, b, copy);
    }
    KaStartPage page;
    auto* list = table(page);
    auto* filter = page.findChild<QLineEdit*>(QStringLiteral("startRecentFilter"));
    QVERIFY(list && filter && list->rowCount() == 2);
    filter->setText(QStringLiteral("저장 안 됨"));
    QVERIFY(!list->isRowHidden(0) && list->isRowHidden(1));  // 「둘」 (newest, row 0) carries the recovery copy
    filter->clear();
    QVERIFY(!list->isRowHidden(0) && !list->isRowHidden(1));
  }

  void continueCard_showsSaveBadge() {
    const QString survey = touch(m_dir.filePath(QStringLiteral("f/조사.gpkg")));
    const QString copy = touch(m_dir.filePath(QStringLiteral("f/복구사본/조사복구_1/복구조사.gpkg")));
    {
      QSettings st = RecentSurveys::userSettings();
      RecentSurveys::remember(st, survey, QStringLiteral("조사"));
    }
    KaStartPage page;
    auto* badge = page.findChild<KaChip*>(QStringLiteral("startContinueBadge"));
    QVERIFY(badge);
    QCOMPARE(badge->text(), QStringLiteral("저장됨"));
    QCOMPARE(badge->tone(), KaChip::Tone::Ok);
    {
      QSettings st = RecentSurveys::userSettings();
      KaRecoverySnapshots::rememberUnsaved(st, survey, copy);
    }
    page.reload();
    QCOMPARE(badge->text(), QStringLiteral("저장 안 됨"));
    QCOMPARE(badge->tone(), KaChip::Tone::Warn);
  }

  void guide_stepsFollowFacts() {
    KaHomeGuideCard card;  // constructed with no target and no facts
    QCOMPARE(card.doneCount(), 0);
    QVERIFY(card.nextStep() == 1 && card.submitLine().contains(QStringLiteral("아직 검수하지 않았습니다")));
    QVERIFY(card.findChild<KaChip*>(QStringLiteral("startGuideBasis"))->isHidden());
    SurveyFacts::Facts facts;
    facts.known = true;
    facts.areas = 1;
    facts.features = 2;
    card.setFacts(QStringLiteral("광령리1"), facts);
    QCOMPARE(card.doneCount(), 2);
    QCOMPARE(card.nextStep(), 3);
    QCOMPARE(card.findChildren<QLabel*>(QStringLiteral("startStepDone")).size(), 2);
    QCOMPARE(card.findChild<KaChip*>(QStringLiteral("startGuideBasis"))->text(), QStringLiteral("광령리1 기준"));
    facts.packagedAtMs = 1;
    card.setFacts(QStringLiteral("광령리1"), facts);
    QCOMPARE(card.doneCount(), 3);
    QCOMPARE(card.nextStep(), 0);
    QCOMPARE(card.findChildren<QLabel*>(QStringLiteral("startStepDone")).size(), 3);
  }

  void guide_submitReadyLine() {
    KaHomeGuideCard card;
    SurveyFacts::Facts facts;
    facts.known = true;
    facts.errors = 0;
    facts.warnings = 2;
    facts.checkedAtMs = 1;
    card.setFacts(QStringLiteral("광령리1"), facts);
    QCOMPARE(card.submitLine(), QStringLiteral("제출 준비: 오류 0 · 경고 2"));
    auto* errors = card.findChild<KaChip*>(QStringLiteral("startSubmitErrors"));
    auto* warnings = card.findChild<KaChip*>(QStringLiteral("startSubmitWarnings"));
    QVERIFY(errors && warnings);
    QVERIFY(errors->text() == QStringLiteral("오류 0") && warnings->text() == QStringLiteral("경고 2"));
    QCOMPARE(errors->tone(), KaChip::Tone::Ok);
    QCOMPARE(warnings->tone(), KaChip::Tone::Warn);
    QVERIFY(card.findChild<QLabel*>(QStringLiteral("startSubmitUnchecked"))->isHidden());
    facts.errors = 3;
    card.setFacts(QStringLiteral("광령리1"), facts);
    QCOMPARE(errors->tone(), KaChip::Tone::Danger);
  }

  void connection_fiveRowsAndSetupSignal() {
    const QString fakeSecret = QStringLiteral("SECRET-KEY-12345");
    KaHomeStatus::Inputs in;
    in.accounts = AccountStatus::snapshot([](AccountStatus::Source s) { return s == AccountStatus::Source::VworldKey; });
    in.crsDatabase = true;
    in.folderPath = m_dir.path();
    in.folder = KaHomeStatus::Folder::Writable;
    KaHomeConnectionCard card;
    card.setInputs(in);
    const auto states = card.findChildren<QLabel*>(QStringLiteral("startConnState"));
    QCOMPARE(states.size(), 5);
    QVERIFY(states.at(0)->text() == QStringLiteral("설정됨") && states.at(1)->text() == QStringLiteral("설정 필요"));
    const auto names = card.findChildren<QLabel*>(QStringLiteral("startConnName"));
    QCOMPARE(names.at(0)->text(), QStringLiteral("VWorld 키"));
    QVERIFY(names.at(3)->text() == QStringLiteral("수치지형도 계정") && names.at(4)->text() == QStringLiteral("문화재 인트라넷"));
    for (auto* label : card.findChildren<QLabel*>())
      QVERIFY(!label->text().contains(fakeSecret) && !label->text().contains(QStringLiteral("연결됨")));
    const auto buttons = card.findChildren<QPushButton*>(QStringLiteral("startConnectionSetup"));
    QCOMPARE(buttons.size(), 5);
    QSignalSpy configure(&card, &KaHomeConnectionCard::configureRequested);
    buttons.at(3)->click();
    QCOMPARE(configure.count(), 1);
    QCOMPARE(configure.first().first().value<AccountStatus::Source>(), AccountStatus::Source::TopographicAccount);
  }

  void connection_problemsOnlyWhenNotOk() {
    KaHomeStatus::Inputs in;
    in.accounts = AccountStatus::snapshot([](AccountStatus::Source) { return true; });
    in.crsDatabase = true;
    in.folder = KaHomeStatus::Folder::Writable;
    KaHomeConnectionCard card;
    card.setInputs(in);
    QCOMPARE(card.findChildren<QLabel*>(QStringLiteral("startConnProblem")).size(), 0);
    in.crsDatabase = false;
    in.folder = KaHomeStatus::Folder::ReadOnly;
    card.setInputs(in);
    const auto problems = card.findChildren<QLabel*>(QStringLiteral("startConnProblem"));
    QCOMPARE(problems.size(), 2);
    QVERIFY(problems.at(0)->text().contains(QStringLiteral("proj.db")) && problems.at(1)->text().contains(QStringLiteral("쓸 수 없음")));
  }

  void hero_compactUnder820() {
    QVERIFY(KaStartPage::heroHeightFor(740) == 184 && KaStartPage::heroHeightFor(900) == 236);
    KaStartPage page;
    page.resize(1366, 740);
    page.show();
    QVERIFY(QTest::qWaitForWindowExposed(&page));
    auto* hero = page.findChild<QWidget*>(QStringLiteral("startHero"));
    auto* right = page.findChild<QScrollArea*>(QStringLiteral("startRightColumn"));
    QVERIFY(hero && right);
    QTRY_COMPARE(hero->height(), 184);
    QTRY_COMPARE(right->width(), 340);
    page.resize(1600, 900);
    QTRY_COMPARE(hero->height(), 236);
    QTRY_COMPARE(right->width(), 400);
    QTRY_VERIFY(right->geometry().right() <= page.width() - 20);  // the row re-lays out after the width change
    const QString out = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    if (!out.isEmpty()) QVERIFY(page.grab().save(QDir(out).filePath(QStringLiteral("home-1600-empty.png"))));
  }
};

int main(int argc, char** argv) {
  if (qEnvironmentVariableIsEmpty("KA_HGIS_LOG_DIR")) qputenv("KA_HGIS_LOG_DIR", QDir::tempPath().toUtf8() + "/ka-hgis-test-logs");
  qputenv("KA_HGIS_HOME_PROBE", "0");  // rows come from records only; no GDAL open in these tests
  QgsApplication app(argc, argv, true);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH", "A:/OSGeo4W/apps/qgis-dev"), true);
  QgsApplication::initQgis();
  QStandardPaths::setTestModeEnabled(true);
  QTemporaryDir settings;  // RecentSurveys, recovery notes and facts never touch the user's ini
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
  KaTheme::apply(&app);  // the captures show the Strata chrome (fonts, QSS), not the Fusion default
  TestHomeCards test;
  const int result = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis();
  return result;
}

#include "test_home_cards.moc"
