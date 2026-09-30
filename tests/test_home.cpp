// Home page and new-survey entry (package D2): quiet recovery note, local-only readiness,
// name validation, origin hint without auto-switching. No network, isolated settings.
#include <QtTest>

#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTemporaryDir>

#include <cmath>
#include <limits>

#include <qgsapplication.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgspointxy.h>
#include <qgsproject.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectorlayer.h>

#include "app/KaHomeConnectionCard.h"
#include "app/KaHomeRecentCard.h"
#include "app/KaHomeStatus.h"
#include "app/KaNewSurveyDialog.h"
#include "app/KaRecoverySnapshots.h"
#include "app/KaStartPage.h"
#include "core/LayerOps.h"
#include "core/RecentSurveys.h"
#include "core/VworldSettings.h"

namespace {

QString touch(const QString& path) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) return {};
  file.write("fixture");
  return QFileInfo(path).absoluteFilePath();
}

}  // namespace

class TestHome : public QObject {
  Q_OBJECT
  QTemporaryDir m_dir;

private slots:
  void cleanup() {
    QSettings st = RecentSurveys::userSettings();
    st.clear();
    VworldSettings::resetScope();
  }

  void nameProblem_explainsUnusableNames_data() {
    QTest::addColumn<QString>("name");
    QTest::addColumn<bool>("usable");
    QTest::newRow("korean") << QStringLiteral("병산동") << true;
    QTest::newRow("spaces-trimmed") << QStringLiteral("  광령리 2차  ") << true;
    QTest::newRow("empty") << QStringLiteral("   ") << false;
    QTest::newRow("con") << QStringLiteral("CON") << false;
    QTest::newRow("aux-lower") << QStringLiteral("aux") << false;
    QTest::newRow("nul-ext") << QStringLiteral("nul.txt") << false;
    QTest::newRow("com1") << QStringLiteral("COM1") << false;
    QTest::newRow("lpt9-space-ext") << QStringLiteral("LPT9 .gpkg") << false;
    QTest::newRow("com10-ok") << QStringLiteral("COM10") << true;
    QTest::newRow("content-ok") << QStringLiteral("CONTENT") << true;
    QTest::newRow("trailing-dot") << QStringLiteral("조사.") << false;
    QTest::newRow("slash") << QStringLiteral("a/b") << false;
    QTest::newRow("colon") << QStringLiteral("구역:1") << false;
    QTest::newRow("tab") << QStringLiteral("a\tb") << false;
    QTest::newRow("too-long") << QString(81, QLatin1Char('x')) << false;
  }
  void nameProblem_explainsUnusableNames() {
    QFETCH(QString, name);
    QFETCH(bool, usable);
    const QString problem = KaNewSurveyDialog::nameProblem(name);
    QCOMPARE(problem.isEmpty(), usable);
  }

  void suggestedWorkCrs_followsTmZones() {
    QCOMPARE(KaNewSurveyDialog::suggestedWorkCrs(126.98), QStringLiteral("EPSG:5186"));  // 서울
    QCOMPARE(KaNewSurveyDialog::suggestedWorkCrs(126.53), QStringLiteral("EPSG:5186"));  // 제주
    QCOMPARE(KaNewSurveyDialog::suggestedWorkCrs(128.73), QStringLiteral("EPSG:5187"));  // 안동
    QCOMPARE(KaNewSurveyDialog::suggestedWorkCrs(129.36), QStringLiteral("EPSG:5187"));  // 포항
    QVERIFY(KaNewSurveyDialog::suggestedWorkCrs(125.0).isEmpty());   // 서부원점 구역
    QVERIFY(KaNewSurveyDialog::suggestedWorkCrs(130.9).isEmpty());   // 울릉(동해원점)
    QVERIFY(KaNewSurveyDialog::suggestedWorkCrs(std::numeric_limits<double>::quiet_NaN()).isEmpty());
    QVERIFY(KaNewSurveyDialog::regionHint().contains(QStringLiteral("5186")));
    QVERIFY(KaNewSurveyDialog::regionHint().contains(QStringLiteral("5187")));
  }

  void dialog_keepsDefaultOriginAndOnlySuggests() {
    KaNewSurveyDialog dialog(QStringLiteral("EPSG:5187"), 126.98);
    QCOMPARE(dialog.windowTitle(), QStringLiteral("새 조사"));
    QCOMPARE(dialog.workCrs(), QStringLiteral("EPSG:5187"));  // never switched by the hint
    auto* suggestion = dialog.findChild<QLabel*>(QStringLiteral("newSurveyCrsSuggestion"));
    QVERIFY(suggestion && !suggestion->isHidden());
    QVERIFY(suggestion->text().contains(QStringLiteral("추천")));
    QVERIFY(suggestion->text().contains(QStringLiteral("5186")));
    auto* tip = dialog.findChild<QLabel*>(QStringLiteral("newSurveyTip"));
    QVERIFY(tip && !tip->text().contains(QStringLiteral("도면만들기")));
    QVERIFY(tip->text().contains(QStringLiteral("상태줄")));
    auto* ok = dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
    auto* name = dialog.findChild<QLineEdit*>();  // the only line edit: the survey name
    auto* error = dialog.findChild<QLabel*>(QStringLiteral("newSurveyNameError"));
    QVERIFY(ok && name && error);
    QVERIFY(!ok->isEnabled());
    QVERIFY(error->isHidden());  // empty field: no red text yet
    name->setText(QStringLiteral("NUL"));
    QVERIFY(!ok->isEnabled());
    QVERIFY(!error->isHidden());
    QVERIFY(error->text().contains(QStringLiteral("예약")));
    name->setText(QStringLiteral("병산동"));
    QVERIFY(ok->isEnabled());
    QVERIFY(error->isHidden());
    QCOMPARE(dialog.surveyName(), QStringLiteral("병산동"));
    auto* central = dialog.findChild<QPushButton*>(QStringLiteral("newSurveyCrs5186"));
    QVERIFY(central);
    central->click();
    QCOMPARE(dialog.workCrs(), QStringLiteral("EPSG:5186"));
    QVERIFY(!suggestion->text().contains(QStringLiteral("추천")));
    central->click();  // an exclusive pair never ends with neither origin chosen
    QCOMPARE(dialog.workCrs(), QStringLiteral("EPSG:5186"));

    KaNewSurveyDialog unknown(QStringLiteral("EPSG:5186"), std::numeric_limits<double>::quiet_NaN());
    QCOMPARE(unknown.workCrs(), QStringLiteral("EPSG:5186"));
    QVERIFY(unknown.findChild<QLabel*>(QStringLiteral("newSurveyCrsSuggestion"))->isHidden());
  }

  void recoveryNotes_rememberForgetAndLookup() {
    QSettings st(m_dir.filePath(QStringLiteral("notes.ini")), QSettings::IniFormat);
    const QString survey = touch(m_dir.filePath(QStringLiteral("노트/조사A.gpkg")));
    const QString copy = touch(m_dir.filePath(QStringLiteral("노트/복구사본/조사복구_1/복구조사.gpkg")));
    QVERIFY(KaRecoverySnapshots::unsavedSnapshotFor(st, survey).isEmpty());
    KaRecoverySnapshots::rememberUnsaved(st, survey, copy);
    QCOMPARE(KaRecoverySnapshots::unsavedSnapshotFor(st, survey), QDir::cleanPath(copy));
    // Windows paths compare without case and separator differences.
    QCOMPARE(KaRecoverySnapshots::unsavedSnapshotFor(st, QDir::toNativeSeparators(survey.toUpper())),
             QDir::cleanPath(copy));
    KaRecoverySnapshots::forgetUnsaved(st, survey);
    QVERIFY(KaRecoverySnapshots::unsavedSnapshotFor(st, survey).isEmpty());
    // A copy that was removed from the folder is not offered.
    KaRecoverySnapshots::rememberUnsaved(st, survey, m_dir.filePath(QStringLiteral("없음.gpkg")));
    QVERIFY(KaRecoverySnapshots::unsavedSnapshotFor(st, survey).isEmpty());
    for (int i = 0; i < 20; ++i)
      KaRecoverySnapshots::rememberUnsaved(st, m_dir.filePath(QStringLiteral("s%1.gpkg").arg(i)), copy);
    QVERIFY(st.value(QStringLiteral("Survey/UnsavedRecovery")).toStringList().size() <= 12);
  }

  void recoveryLayers_onlyEditedAndMemoryOnly() {
    QgsProject project;
    auto* scratch = new QgsVectorLayer(QStringLiteral("Point?crs=EPSG:5187"), QStringLiteral("임시"),
                                       QStringLiteral("memory"));
    auto* reference = new QgsVectorLayer(QStringLiteral("Point?crs=EPSG:5187"), QStringLiteral("참조"),
                                         QStringLiteral("memory"));
    LayerOps::markReferenceLayer(reference);
    const QString gpkg = m_dir.filePath(QStringLiteral("layers.gpkg"));
    QgsVectorFileWriter::SaveVectorOptions options;
    options.driverName = QStringLiteral("GPKG");
    options.layerName = QStringLiteral("survey_area");
    QCOMPARE(QgsVectorFileWriter::writeAsVectorFormatV3(scratch, gpkg, project.transformContext(), options),
             QgsVectorFileWriter::NoError);
    auto* onDisk = new QgsVectorLayer(gpkg + QStringLiteral("|layername=survey_area"),
                                      QStringLiteral("조사구역"), QStringLiteral("ogr"));
    QVERIFY(onDisk->isValid());
    project.addMapLayers({scratch, reference, onDisk});
    QStringList ids = KaRecoverySnapshots::layerIdsToCapture(&project);
    QVERIFY(ids.contains(scratch->id()));
    QVERIFY(!ids.contains(reference->id()));
    QVERIFY(!ids.contains(onDisk->id()));  // unchanged file layer: already on disk
    QVERIFY(onDisk->startEditing());
    QgsFeature feature(onDisk->fields());
    feature.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(200000, 450000)));
    QVERIFY(onDisk->addFeature(feature));
    ids = KaRecoverySnapshots::layerIdsToCapture(&project);
    QVERIFY(ids.contains(onDisk->id()));
    onDisk->rollBack();
  }

  void homeStatus_isLocalAndPointsToMenus() {
    KaHomeStatus::Inputs in;
    in.accounts = AccountStatus::snapshot(
        [](AccountStatus::Source source) { return source == AccountStatus::Source::VworldKey; });
    in.crsDatabase = false;
    in.folderPath = m_dir.filePath(QStringLiteral("없는폴더"));
    in.folder = KaHomeStatus::folderState(in.folderPath);
    QCOMPARE(in.folder, KaHomeStatus::Folder::Missing);
    const auto lines = KaHomeStatus::describe(in);
    QCOMPARE(lines.size(), in.accounts.size() + 2);
    QVERIFY(lines.first().ok);
    QVERIFY(lines.first().hint.isEmpty());
    for (int i = 1; i < in.accounts.size(); ++i) {
      QVERIFY(!lines.at(i).ok);
      QVERIFY2(lines.at(i).hint.contains(QStringLiteral("더보기")), qPrintable(lines.at(i).hint));
    }
    QVERIFY(!lines.at(lines.size() - 2).ok);  // proj.db
    QVERIFY(!lines.last().ok);                // folder
    QCOMPARE(KaHomeStatus::folderState(m_dir.path()), KaHomeStatus::Folder::Writable);
    QCOMPARE(KaHomeStatus::folderState(QString()), KaHomeStatus::Folder::None);
    QVERIFY(KaHomeStatus::crsDatabaseReady());

    KaHomeConnectionCard card;  // five account rows + one label per problem line (proj.db, folder)
    card.setInputs(in);
    int shown = 0;
    int warnings = 0;
    for (auto* label : card.findChildren<QLabel*>()) {
      if (!label->property("kaStatusOk").isValid()) continue;
      ++shown;
      if (!label->property("kaStatusOk").toBool()) ++warnings;
    }
    QCOMPARE(shown, lines.size());
    QCOMPARE(warnings, lines.size() - 1);
  }

  void startPage_recoveryNoteIsQuietAndClickOnly() {
    const QString survey = touch(m_dir.filePath(QStringLiteral("현장/광령리.gpkg")));
    const QString copy = touch(m_dir.filePath(QStringLiteral("현장/복구사본/조사복구_2/복구조사.gpkg")));
    {
      QSettings st = RecentSurveys::userSettings();
      RecentSurveys::remember(st, survey, QStringLiteral("광령리"));
      KaRecoverySnapshots::rememberUnsaved(st, survey, copy);
    }
    KaStartPage page;
    QSignalSpy opened(&page, &KaStartPage::recentOpened);
    QCoreApplication::processEvents();
    QCOMPARE(opened.count(), 0);  // nothing opens on its own
    auto* note = page.findChild<QLabel*>(QStringLiteral("startRecoveryNote"));
    QVERIFY(note && !note->isHidden());
    QVERIFY(note->text().contains(QStringLiteral("복구 사본")));
    auto* list = page.findChild<QTableWidget*>(QStringLiteral("recentSurveyList"));
    QVERIFY(list && list->rowCount() == 1 && list->columnCount() == 4);
    QCOMPARE(list->item(0, 1)->data(KaHomeRecentCard::kToneRole).toString(), QStringLiteral("warn"));
    QCOMPARE(list->item(0, 1)->text(), QStringLiteral("저장 안 됨"));
    QVERIFY(list->item(0, 1)->toolTip().contains(QStringLiteral("복구 사본")));
    emit note->linkActivated(QStringLiteral("open"));
    QCOMPARE(opened.count(), 1);
    QCOMPARE(opened.first().first().toString(), QDir::cleanPath(copy));
    emit note->linkActivated(QStringLiteral("dismiss"));
    QVERIFY(note->isHidden());
    QSettings st = RecentSurveys::userSettings();
    QVERIFY(KaRecoverySnapshots::unsavedSnapshotFor(st, survey).isEmpty());
    QVERIFY(QFileInfo::exists(copy));  // hiding the note never deletes the copy
  }

  void startPage_withoutCopyShowsNoNote() {
    const QString survey = touch(m_dir.filePath(QStringLiteral("현장2/병산동.gpkg")));
    {
      QSettings st = RecentSurveys::userSettings();
      RecentSurveys::remember(st, survey, QStringLiteral("병산동"));
    }
    KaStartPage page;
    auto* note = page.findChild<QLabel*>(QStringLiteral("startRecoveryNote"));
    QVERIFY(note && note->isHidden());
    auto* list = page.findChild<QTableWidget*>(QStringLiteral("recentSurveyList"));
    QVERIFY(list && list->rowCount() == 1);
    QCOMPARE(list->item(0, 1)->text(), QStringLiteral("저장됨"));
    QVERIFY(!list->item(0, 1)->toolTip().contains(QStringLiteral("복구")));
  }

  void startPage_setupRereadsConnectionRowsNextTurn() {
    VworldSettings::setScope({QStringLiteral("ka-hgis"), QStringLiteral("ka-hgis"), m_dir.filePath(QStringLiteral("keys"))});
    KaStartPage page;  // the fake key goes to files under m_dir, never to the registry
    const auto vworldState = [&page]() { return page.findChildren<QLabel*>(QStringLiteral("startConnState")).value(0); };
    page.refreshConnections();
    QTRY_VERIFY(vworldState());
    const QPointer<QLabel> before = vworldState();
    // What the window does after the 「설정」 dialog, still inside that row's own button click.
    VworldSettings::saveApiKey(QStringLiteral("TEST-KEY-NOT-REAL"));
    page.reload();
    page.refreshConnections();
    QVERIFY(!before.isNull());  // the row (and its emitting button) survive until the next turn
    QTRY_COMPARE(vworldState()->text(), QStringLiteral("설정됨"));
  }
};

int main(int argc, char** argv) {
  if (qEnvironmentVariableIsEmpty("KA_HGIS_LOG_DIR"))
    qputenv("KA_HGIS_LOG_DIR", QDir::temp().filePath(QStringLiteral("ka-hgis-test-logs")).toUtf8());
  QgsApplication app(argc, argv, true);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH", "A:/OSGeo4W/apps/qgis-dev"), true);
  QgsApplication::initQgis();
  QStandardPaths::setTestModeEnabled(true);
  QTemporaryDir settings;  // RecentSurveys and recovery notes never touch the user's ini
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
  TestHome test;
  const int result = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis();
  return result;
}

#include "test_home.moc"
