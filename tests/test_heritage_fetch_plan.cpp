#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <qgsapplication.h>
#include <qgslayertree.h>
#include <qgslayertreegroup.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

#include "core/HeritageFetchPlan.h"
#include "core/HeritagePledge.h"
#include "core/HeritageRecentDownloads.h"

// F120 / F169 / F174 core rules, without the intranet or WebEngine.
class HeritageFetchPlanTest : public QObject {
  Q_OBJECT
  static QString writeFile(const QString& path, const QByteArray& bytes) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size()) return {};
    return QFileInfo(path).absoluteFilePath();
  }

private slots:
  void followUpsKeepOrderAndNeverRepeatTheFirstCity() {
    const HeritageCity first{QStringLiteral("경상북도"), QStringLiteral("안동시")};
    const QList<HeritageCity> picked = {
        {QStringLiteral("경북"), QStringLiteral("예천군")},
        {QStringLiteral("경상북도"), QStringLiteral("안동시")},
        {QStringLiteral("경상북도"), QStringLiteral("의성군")},
        {QStringLiteral("경상북도"), QStringLiteral("예천군")},
        {QStringLiteral("경상북도"), QString()},
    };
    const QList<HeritageCity> out = HeritageFetchPlanning::normalizedFollowUps(first, picked);
    QCOMPARE(out.size(), 2);
    QCOMPARE(out.at(0).display(), QStringLiteral("경상북도 예천군"));
    QCOMPARE(out.at(1).display(), QStringLiteral("경상북도 의성군"));
    const HeritageCity sejong{QStringLiteral("세종특별자치시"), QStringLiteral("세종특별자치시")};
    QCOMPARE(sejong.display(), QStringLiteral("세종특별자치시"));
  }

  void nextCityGoesNextToTheCurrentOne() {
    const HeritageCity next{QStringLiteral("경상북도"), QStringLiteral("예천군")};
    QCOMPARE(HeritageFetchPlanning::siblingRoot(QStringLiteral("C:/data/주변유적/경상북도 안동시/원본"), next),
             QStringLiteral("C:/data/주변유적/경상북도 예천군/원본"));
    QCOMPARE(HeritageFetchPlanning::siblingRoot(QStringLiteral("C:/data/downloads"), next),
             QStringLiteral("C:/data/downloads/경상북도 예천군"));
  }

  void recentDownloadIsReusedOnlyWhileFreshAndComplete() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString root = temp.filePath(QStringLiteral("주변유적/경상북도 안동시/원본"));
    const QString zip = writeFile(QDir(root).filePath(QStringLiteral("a/지정유산.zip")), "PK-fixture");
    QVERIFY(!zip.isEmpty());
    const QDateTime now = QDateTime::currentDateTime();
    QVERIFY(!HeritageRecentDownloads::latest(root, 30, now).isValid());
    QVERIFY(HeritageRecentDownloads::record(root, HeritageDataset::DesignatedHeritage, {zip},
                                            now.addDays(-2)));
    QCOMPARE(HeritageRecentDownloads::find(root, HeritageDataset::DesignatedHeritage, 30, now),
             QStringList{zip});
    QVERIFY(HeritageRecentDownloads::find(root, HeritageDataset::SurfaceSurveyArea, 30, now).isEmpty());
    QVERIFY(HeritageRecentDownloads::latest(root, 30, now).isValid());
    // Too old: not offered.
    QVERIFY(HeritageRecentDownloads::find(root, HeritageDataset::DesignatedHeritage, 1, now).isEmpty());
    // A file that disappeared or became empty is never reused.
    QFile(zip).resize(0);
    QVERIFY(HeritageRecentDownloads::find(root, HeritageDataset::DesignatedHeritage, 30, now).isEmpty());
    QVERIFY(!HeritageRecentDownloads::latest(root, 30, now).isValid());
    // The default root follows the download layout of the confirm flow.
    QVERIFY(HeritageRecentDownloads::defaultRoot({QStringLiteral("경상북도"), QStringLiteral("안동시")})
                .endsWith(QStringLiteral("주변유적/경상북도 안동시/원본")));
  }

  void pledgeHighlightsQuoteOnlyTheText() {
    const QString terms = QStringLiteral(
        "국가유산 공간정보 원본자료 사용 서약서\n"
        "본인은 제공받은 자료를 신청한 목적 외에는 사용하지 않겠습니다. "
        "자료를 제3자에게 제공하거나 외부로 반출하지 않겠습니다.\n"
        "위 사항을 위반할 경우 관련 법령에 따른 책임을 지겠습니다.");
    const QStringList lines = HeritagePledge::highlights(terms);
    QCOMPARE(lines.size(), 2);
    QVERIFY(terms.contains(lines.at(0)));
    QVERIFY(terms.contains(lines.at(1)));
    QVERIFY(lines.at(1).contains(QStringLiteral("제3자")));
    const QString notice = HeritagePledge::noticeAfterAgreement(terms);
    QVERIFY(notice.contains(QStringLiteral("receipts")));
    QVERIFY(notice.contains(lines.at(0)));
    QVERIFY(HeritagePledge::noticeAfterAgreement(QString()).contains(QStringLiteral("원문을 확인")));
    QVERIFY(HeritagePledge::preDisclosure().contains(QStringLiteral("자동으로 동의")));
  }

  void checklistGuidanceFollowsIntranetLayers() {
    QgsProject project;
    QVERIFY(HeritagePledge::checklistPasses(&project));
    auto* plain = new QgsVectorLayer(QStringLiteral("Point?crs=EPSG:5186"), QStringLiteral("참조"),
                                     QStringLiteral("memory"));
    project.addMapLayer(plain);
    QVERIFY(HeritagePledge::checklistPasses(&project));
    // Projects saved before the layer property: a layer inside a heritage kind group.
    auto* legacy = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                      QStringLiteral("국가지정유산"), QStringLiteral("memory"));
    project.addMapLayer(legacy, false);
    auto* group = project.layerTreeRoot()->addGroup(QStringLiteral("참조 지도"))->addGroup(QStringLiteral("지정유산"));
    group->addLayer(legacy);
    QVERIFY(!HeritagePledge::checklistPasses(&project));
    // The legacy layer itself carries no tag: only its group says it is intranet data.
    QVERIFY(!HeritagePledge::isIntranetLayer(legacy));
    QCOMPARE(HeritagePledge::checklistStateKey(), QStringLiteral("HERITAGE_PLEDGE_SCOPE"));
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(
      qEnvironmentVariable("QGIS_PREFIX_PATH", "D:/OSGeo4W/apps/qgis-dev"), true);
  QgsApplication::initQgis();
  QStandardPaths::setTestModeEnabled(true);
  HeritageFetchPlanTest test;
  const int result = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis();
  return result;
}
#include "test_heritage_fetch_plan.moc"
