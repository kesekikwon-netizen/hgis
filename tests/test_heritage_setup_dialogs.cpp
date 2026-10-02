#include <QtTest>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QUuid>
#include <QCheckBox>
#include <QFile>
#include <QLabel>
#include "app/KaHeritageSetupDialogs.h"
#include "core/HeritageIntranetSettings.h"
#include "core/HeritageRecentDownloads.h"
#include "core/HeritageRegionResolver.h"

class HeritageSetupDialogsTest : public QObject {
  Q_OBJECT
  QString settingsDirectory;
private slots:
  void initTestCase() {
#ifdef Q_OS_WIN
    const int fontId = QFontDatabase::addApplicationFont(QDir(qEnvironmentVariable("WINDIR")).filePath(QStringLiteral("Fonts/malgun.ttf")));
    QVERIFY(fontId >= 0);
    QApplication::setFont(QFont(QFontDatabase::applicationFontFamilies(fontId).first(), 9));
#endif
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("ka-hgis-qa-heritage"));
    QCoreApplication::setApplicationName(QStringLiteral("setup-") + QUuid::createUuid().toString(QUuid::WithoutBraces));
    settingsDirectory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QVERIFY(!QFileInfo::exists(settingsDirectory));
    QString error;
    QVERIFY2(HeritageIntranetSettings::saveCredentials({}, &error), qPrintable(error));
  }
  void missingAccountCanBeSavedAndCancelledWithoutChanges() {
    KaHeritageAccountDialog dialog;
    dialog.show();
    QTest::qWait(20);
    QDir().mkpath(QStringLiteral("build/qa/heritage-startup-20260915/dialogs"));
    QVERIFY(dialog.grab().save(QStringLiteral("build/qa/heritage-startup-20260915/dialogs/account.png")));
    auto* id = dialog.findChild<QLineEdit*>(QStringLiteral("heritageAccountUsername")); QVERIFY(id);
    auto* pw = dialog.findChild<QLineEdit*>(QStringLiteral("heritageAccountPassword")); QVERIFY(pw);
    QCOMPARE(pw->echoMode(), QLineEdit::Password);
    auto* buttons = dialog.findChild<QDialogButtonBox*>(); QVERIFY(buttons);
    QSignalSpy accepted(&dialog, &QDialog::accepted);
    id->setText(QStringLiteral("fixture-user"));
    buttons->button(QDialogButtonBox::Save)->click();
    QCOMPARE(accepted.count(), 0);
    QVERIFY(!HeritageIntranetSettings::hasCredentials());
    pw->setText(QStringLiteral(" test;@\\password "));
    buttons->button(QDialogButtonBox::Save)->click();
    QCOMPARE(accepted.count(), 1);
    QCOMPARE(HeritageIntranetSettings::credentials().password, pw->text());
    KaHeritageAccountDialog cancelled;
    cancelled.findChild<QLineEdit*>(QStringLiteral("heritageAccountPassword"))->setText(QStringLiteral("unsaved"));
    cancelled.reject();
    QCOMPARE(HeritageIntranetSettings::credentials().password, pw->text());
  }
  void failedLookupRequiresExplicitCityAndResetsAcrossProvinces() {
    KaHeritageRegionDialog dialog({}, {}, QStringLiteral("좌표로 시·군을 찾지 못했습니다."));
    dialog.show();
    QTest::qWait(20);
    QVERIFY(dialog.grab().save(QStringLiteral("build/qa/heritage-startup-20260915/dialogs/region.png")));
    auto* sido = dialog.findChild<QComboBox*>(QStringLiteral("heritageSido")); QVERIFY(sido);
    auto* city = dialog.findChild<QComboBox*>(QStringLiteral("heritageCity")); QVERIFY(city);
    auto* ok = dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
    QVERIFY(!ok->isEnabled());
    sido->setCurrentIndex(sido->findData(QStringLiteral("경상북도")));
    QVERIFY(!ok->isEnabled());
    city->setCurrentIndex(city->findData(QStringLiteral("안동시")));
    QVERIFY(ok->isEnabled());
    QCOMPARE(dialog.city(), QStringLiteral("안동시"));
    sido->setCurrentIndex(sido->findData(QStringLiteral("경기도")));
    QVERIFY(dialog.city().isEmpty());
    QVERIFY(!ok->isEnabled());
  }
  void detectedRegionAndSejongCanBeConfirmed() {
    KaHeritageRegionDialog detected(QStringLiteral("경상북도"), QStringLiteral("안동시"));
    QCOMPARE(detected.sido(), QStringLiteral("경상북도"));
    QCOMPARE(detected.city(), QStringLiteral("안동시"));
    KaHeritageRegionDialog sejong(QStringLiteral("세종특별자치시"), QStringLiteral("세종시"));
    auto* ok = sejong.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
    QVERIFY(ok->isEnabled());
    QCOMPARE(sejong.sido(), QStringLiteral("세종특별자치시"));
    QCOMPARE(sejong.city(), QStringLiteral("세종시"));
  }
  // F120: neighbours of the 5 km scope are offered in the same single dialog; only the
  // resolved 시·군 is selected by default and each 시·군 stays its own request.
  void nearbyCitiesAreOfferedButOnlyResolvedIsDefault() {
    HeritageRegion region;
    region.sido = QStringLiteral("경상북도");
    region.city = QStringLiteral("안동시");
    region.nearby = {{QStringLiteral("경상북도"), QStringLiteral("예천군")},
                     {QStringLiteral("경상북도"), QStringLiteral("의성군")}};
    KaHeritageRegionDialog dialog(region);
    dialog.show();
    QTest::qWait(20);
    QDir().mkpath(QStringLiteral("build/qa/heritage-startup-20260915/dialogs"));
    QVERIFY(dialog.grab().save(QStringLiteral("build/qa/heritage-startup-20260915/dialogs/region-nearby.png")));
    const auto boxes = dialog.findChildren<QCheckBox*>(QStringLiteral("heritageNearby"));
    QCOMPARE(boxes.size(), 2);
    for (auto* box : boxes) QVERIFY(!box->isChecked());
    QVERIFY(dialog.plan().followUps.isEmpty());
    boxes.at(1)->setChecked(true);
    HeritageFetchPlan plan = dialog.plan();
    QCOMPARE(plan.followUps.size(), 1);
    QCOMPARE(plan.followUps.first().city, QStringLiteral("의성군"));
    QVERIFY(!plan.reuseRecent);
    // Picking the ticked neighbour as the main 시·군 does not request it twice.
    auto* city = dialog.findChild<QComboBox*>(QStringLiteral("heritageCity"));
    city->setCurrentIndex(city->findData(QStringLiteral("의성군")));
    QVERIFY(dialog.plan().followUps.isEmpty());
  }

  void nearbyLookupNoteIsShownNotHidden() {
    HeritageRegion region;
    region.sido = QStringLiteral("경상북도");
    region.city = QStringLiteral("안동시");
    region.nearbyNote = QStringLiteral("행정구역 서버에서 주변 5km 이웃 시·군을 확인하지 못했습니다.");
    KaHeritageRegionDialog dialog(region);
    auto* note = dialog.findChild<QLabel*>(QStringLiteral("heritageNearbyNote"));
    QVERIFY(note);
    QCOMPARE(note->text(), region.nearbyNote);
    QVERIFY(dialog.findChildren<QCheckBox*>(QStringLiteral("heritageNearby")).isEmpty());
    // Without neighbours or a note the dialog looks as before.
    KaHeritageRegionDialog plain(QStringLiteral("경상북도"), QStringLiteral("안동시"));
    QVERIFY(!plain.findChild<QWidget*>(QStringLiteral("heritageNearbyGroup")));
  }

  // F169: the automatic pledge agreement is disclosed before anything is fetched.
  void pledgeIsDisclosedInTheConfirmDialog() {
    KaHeritageRegionDialog dialog(QStringLiteral("경상북도"), QStringLiteral("안동시"));
    auto* note = dialog.findChild<QLabel*>(QStringLiteral("heritagePledgeNote"));
    QVERIFY(note);
    QVERIFY(note->text().contains(QStringLiteral("서약서")));
    QVERIFY(note->text().contains(QStringLiteral("receipts")));
  }

  // F174: a recent download can be reused, but only when the user ticks it.
  void recentDownloadIsOfferedNotForced() {
    const HeritageCity andong{QStringLiteral("경상북도"), QStringLiteral("안동시")};
    const QString root = HeritageRecentDownloads::defaultRoot(andong);
    QVERIFY(!root.isEmpty());
    {
      KaHeritageRegionDialog none(andong.sido, andong.city);
      auto* reuse = none.findChild<QCheckBox*>(QStringLiteral("heritageReuseRecent"));
      QVERIFY(reuse && reuse->isHidden());
      QVERIFY(!none.plan().reuseRecent);
    }
    const QString zip = QDir(root).filePath(QStringLiteral("fixture/지정유산.zip"));
    QVERIFY(QDir().mkpath(QFileInfo(zip).absolutePath()));
    QFile file(zip);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("PK-fixture");
    file.close();
    QVERIFY(HeritageRecentDownloads::record(root, HeritageDataset::DesignatedHeritage, {zip}));
    KaHeritageRegionDialog dialog(andong.sido, andong.city);
    auto* reuse = dialog.findChild<QCheckBox*>(QStringLiteral("heritageReuseRecent"));
    QVERIFY(reuse && !reuse->isHidden());
    QVERIFY(!reuse->isChecked());
    QVERIFY(!dialog.plan().reuseRecent);
    reuse->setChecked(true);
    QVERIFY(dialog.plan().reuseRecent);
    QVERIFY(root.contains(QStringLiteral("setup-")));  // unique test-mode location only
    QVERIFY(QDir(QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                     .filePath(QStringLiteral("주변유적"))).removeRecursively());
  }

  void clearingAccountKeepsPersonalEmptyOverride() {
    KaHeritageAccountDialog dialog;
    dialog.findChild<QLineEdit*>(QStringLiteral("heritageAccountUsername"))->clear();
    dialog.findChild<QLineEdit*>(QStringLiteral("heritageAccountPassword"))->clear();
    dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
    QCOMPARE(dialog.result(), int(QDialog::Accepted));
    QVERIFY(!HeritageIntranetSettings::hasCredentials());
    QVERIFY(QFileInfo::exists(QDir(settingsDirectory).filePath(QStringLiteral("heritage-account.ini"))));
  }
  void cleanupTestCase() {
    QVERIFY(settingsDirectory == QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
    QVERIFY(QFileInfo(settingsDirectory).fileName().startsWith(QStringLiteral("setup-")));
    QVERIFY(QDir(settingsDirectory).removeRecursively());
  }
};
QTEST_MAIN(HeritageSetupDialogsTest)
#include "test_heritage_setup_dialogs.moc"
