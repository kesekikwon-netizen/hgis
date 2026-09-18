#include <QtTest>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QWebEngineDownloadRequest>
#include <QWebEngineProfile>
#include <QWebEngineUrlRequestInfo>
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineView>
#include <atomic>

#include "app/KaHeritageBrowser.h"
#include "core/KaPortableRuntime.h"

namespace {
// Offline regressions never send requests to the portal or use a stored account.
class BlockPortal final : public QWebEngineUrlRequestInterceptor {
public:
  std::atomic<int> homeRequests{0};
  void interceptRequest(QWebEngineUrlRequestInfo& info) override {
    if (info.requestUrl().host() != QStringLiteral("intranet.gis-heritage.go.kr")) return;
    if (info.requestUrl().path() == QStringLiteral("/ngis/common/main.do")) ++homeRequests;
    info.block(true);
  }
};
const QString errorHtml = QStringLiteral(
    "<html><body><nav>지정유산 현상변경허용기준 매장유산유존지역</nav>"
    "<div style='display:none'><label><input type='checkbox'>위 서약서에 동의합니다</label></div>"
    "<div id='tabContentDiv'><h1>찾을 수 없는 페이지 입니다.</h1></div></body></html>");

QString wholeDownloadFixture(const QString& mode, bool cityValid, bool conflictingGlobal) {
  const bool bjd = QStringLiteral("BURE").contains(mode);
  return QStringLiteral(
      "<html><body><button onclick='outside++'>전체다운로드</button>"
      "<div id='tabContentDiv'><form id='searchForm'><input id='mode' value='%1'>"
      "<select id='%2' name='%2'><option value='47'>검증 시도</option></select>"
      "<select id='%3' name='%3'><option value='%4'>검증 시군</option></select>"
      "<button style='display:none' onclick='hiddenClicks++;return false'>전체다운로드</button>"
      "<button id='actualWholeDownload' onclick='requests++;return false'>전체다운로드</button>"
      "</form></div><script>var requests=0,outside=0,hiddenClicks=0,globalCalls=0;%5</script></body></html>")
      .arg(mode, bjd ? QStringLiteral("bjdCd") : QStringLiteral("codedetaCd0"),
           bjd ? QStringLiteral("bjdCd1") : QStringLiteral("codeCdSg0"),
           cityValid ? QStringLiteral("4711") : QString(),
           conflictingGlobal ? QStringLiteral("function searchGisChaRirList(){globalCalls++;throw new Error('unrelated-map-handler');}")
                             : QString());
}
}

class HeritageBrowserTest : public QObject {
  Q_OBJECT
private slots:
  void wholeDownloadUsesOnlyTheVisibleFormButton_data() {
    QTest::addColumn<QString>("mode");
    QTest::addColumn<bool>("cityValid");
    QTest::addColumn<bool>("conflictingGlobal");
    for (const auto& mode : {"S", "P", "B", "U", "R", "E"})
      QTest::newRow(mode) << QString::fromLatin1(mode) << true << true;
    QTest::newRow("outside-and-hidden") << QStringLiteral("S") << true << false;
    QTest::newRow("empty-sgg") << QStringLiteral("S") << false << true;
    QTest::newRow("empty-bjd-sgg") << QStringLiteral("B") << false << true;
  }

  void wholeDownloadUsesOnlyTheVisibleFormButton() {
    QFETCH(QString, mode);
    QFETCH(bool, cityValid);
    QFETCH(bool, conflictingGlobal);
    QTemporaryDir profile;
    BlockPortal blocker;
    KaHeritageBrowser browser;
    browser.m_profile->setPersistentStoragePath(profile.path());
    browser.m_profile->setUrlRequestInterceptor(&blocker);
    auto* view = browser.addPage();
    QSignalSpy loaded(view, &QWebEngineView::loadFinished);
    view->setHtml(wholeDownloadFixture(mode, cityValid, conflictingGlobal));
    QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 10000);
    QString result;
    view->page()->runJavaScript(HeritageIntranetFlow::downloadAllScript(),
                               [&](const QVariant& value) { result = value.toString(); });
    QTRY_VERIFY_WITH_TIMEOUT(!result.isEmpty(), 5000);
    QString counters;
    view->page()->runJavaScript(QStringLiteral("JSON.stringify({requests:requests,outside:outside,hiddenClicks:hiddenClicks,globalCalls:globalCalls})"),
                               [&](const QVariant& value) { counters = value.toString(); });
    QTRY_VERIFY_WITH_TIMEOUT(!counters.isEmpty(), 5000);
    const auto counts = QJsonDocument::fromJson(counters.toUtf8()).object();
    QCOMPARE(counts.value(QStringLiteral("requests")).toInt(), cityValid ? 1 : 0);
    QCOMPARE(counts.value(QStringLiteral("outside")).toInt(), 0);
    QCOMPARE(counts.value(QStringLiteral("hiddenClicks")).toInt(), 0);
    QCOMPARE(counts.value(QStringLiteral("globalCalls")).toInt(), 0);
    QCOMPARE(result, cityValid ? QStringLiteral("all") : QStringLiteral("no-region"));
    browser.m_profile->setUrlRequestInterceptor(nullptr);
  }

  void wholeDownloadPollingClicksOnce() {
    QTemporaryDir profile;
    BlockPortal blocker;
    KaHeritageBrowser browser;
    browser.m_profile->setPersistentStoragePath(profile.path());
    browser.m_profile->setUrlRequestInterceptor(&blocker);
    auto* view = browser.addPage();
    QSignalSpy loaded(view, &QWebEngineView::loadFinished);
    view->setHtml(wholeDownloadFixture(QStringLiteral("S"), true, true));
    QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 10000);
    browser.setTarget(QStringLiteral("경상북도"), QStringLiteral("포항시"), {HeritageDataset::DesignatedHeritage});
    browser.m_stage = HeritageStage::Download;
    browser.m_running = true;
    QSignalSpy finished(&browser, &KaHeritageBrowser::allFinished);
    for (int tick = 0; tick < 8; ++tick) {
      browser.runCurrentStage();
      QTRY_VERIFY_WITH_TIMEOUT(!browser.m_scriptInFlight, 5000);
    }
    int requests = -1;
    view->page()->runJavaScript(QStringLiteral("requests"), [&](const QVariant& value) { requests = value.toInt(); });
    QTRY_VERIFY_WITH_TIMEOUT(requests >= 0, 5000);
    QCOMPARE(requests, 1);
    QCOMPARE(finished.size(), 0);
    browser.stop();
    browser.m_profile->setUrlRequestInterceptor(nullptr);
  }

  void rejectedAgreementReceiptDoesNotAdvance() {
    QTemporaryDir profile;
    KaHeritageBrowser browser;
    browser.m_profile->setPersistentStoragePath(profile.path());
    auto* view = browser.addPage();
    QSignalSpy loaded(view, &QWebEngineView::loadFinished);
    view->setHtml(QStringLiteral(
        "<html><body><div><p>검증용 서약문</p>"
        "<label><input type=checkbox>위 서약서에 동의합니다</label>"
        "<button>확인</button></div></body></html>"));
    QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 10000);
    connect(&browser, &KaHeritageBrowser::agreementAccepted, &browser,
            [&](const QDateTime&, const QString&) {
              browser.rejectDataset(QStringLiteral("서약서 영수증 저장 실패"));
            });
    QSignalSpy agreed(&browser, &KaHeritageBrowser::agreementAccepted);
    QSignalSpy completed(&browser, &KaHeritageBrowser::allFinished);
    browser.m_stage = HeritageStage::AgreeTerms;
    browser.m_running = true;
    browser.runCurrentStage();
    QTRY_COMPARE_WITH_TIMEOUT(agreed.count(), 1, 5000);
    QCOMPARE(browser.stage(), HeritageStage::Failed);
    QVERIFY(!browser.m_running);
    browser.runStage();
    QCOMPARE(browser.stage(), HeritageStage::Failed);
    QCOMPARE(completed.count(), 0);
  }

  void agreementExceptionStopsWithoutRetry() {
    QTemporaryDir profile;
    KaHeritageBrowser browser;
    browser.m_profile->setPersistentStoragePath(profile.path());
    auto* view=browser.addPage();
    QSignalSpy loaded(view, &QWebEngineView::loadFinished);
    view->setHtml(QStringLiteral("<html><body><div><label><input type=checkbox>위 서약서에 동의합니다</label>"
      "<button id=confirm>확인</button></div><script>var requests=0;document.getElementById('confirm').click=function(){requests++;throw new Error('fixture');};</script></body></html>"));
    QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 10000);
    browser.m_stage=HeritageStage::AgreeTerms;
    browser.m_running=true;
    browser.runCurrentStage();
    QTRY_VERIFY_WITH_TIMEOUT(!browser.m_scriptInFlight, 5000);
    QCOMPARE(browser.stage(), HeritageStage::Failed);
    browser.runStage();
    int requests=-1;
    view->page()->runJavaScript(QStringLiteral("requests"),[&](const QVariant& v){requests=v.toInt();});
    QTRY_COMPARE_WITH_TIMEOUT(requests, 1, 5000);
  }

  void openingADeferredDownloadPageSendsOneRequest() {
    QTemporaryDir profile;
    KaHeritageBrowser browser;
    browser.m_profile->setPersistentStoragePath(profile.path());
    auto* view = browser.addPage();
    QSignalSpy loaded(view, &QWebEngineView::loadFinished);
    view->setHtml(QStringLiteral("<html><body><script>var requests=0;function showAgreePopup(){requests++;}</script></body></html>"));
    QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 10000);
    browser.m_stage = HeritageStage::OpenDownloadPage;
    browser.m_running = true;
    for (int tick=0; tick<3; ++tick) {
      browser.runStage();
      QTRY_VERIFY_WITH_TIMEOUT(!browser.m_scriptInFlight, 5000);
    }
    int requests=-1;
    view->page()->runJavaScript(QStringLiteral("requests"),[&](const QVariant& v){requests=v.toInt();});
    QTRY_COMPARE_WITH_TIMEOUT(requests, 1, 5000);
    QCOMPARE(browser.stage(), HeritageStage::OpenDownloadPage);
    QSignalSpy success(&browser, &KaHeritageBrowser::allFinished);
    browser.m_waitTicks=1000;
    browser.runStage();
    QCOMPARE(browser.stage(), HeritageStage::Failed);
    QCOMPARE(success.size(), 0);
  }

  void hiddenFormsAndUnrelatedConfirmAreNeverUsed() {
    QTemporaryDir profile;
    KaHeritageBrowser browser;
    browser.m_profile->setPersistentStoragePath(profile.path());
    auto* view = browser.addPage();
    QSignalSpy loaded(view, &QWebEngineView::loadFinished);
    view->setHtml(QStringLiteral(R"HTML(<html><body>
      <button onclick='wrongConfirm++'>확인</button>
      <div style='display:none'><form id='searchForm'><input id='mode' value='S'>
        <select name='codedetaCd'><option>시도선택</option></select></form></div>
      <div id='agreement' style='display:none'>
        <p>원본자료 사용 서약서 — 제3자 양도 금지</p>
        <input type='checkbox' id='agree' style='display:none'><label for='agree'>위 서약서에 동의합니다</label>
        <button onclick='rightConfirm++'>확인</button></div>
      <script>var opened=0,wrongConfirm=0,rightConfirm=0;
        function tabContentAjax(){}
        function showAgreePopup(){opened++;document.getElementById('agreement').style.display='block';}
      </script></body></html>)HTML"));
    QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 15000);
    auto execute = [&](const QString& script) {
      QString value;
      QEventLoop loop;
      browser.runScript(script, [&](const QString& answer) { value=answer;loop.quit(); });
      QTimer::singleShot(5000, &loop, &QEventLoop::quit);
      if (value.isEmpty()) loop.exec();
      return value;
    };
    QCOMPARE(execute(HeritageIntranetFlow::downloadPageProbeScript()), QStringLiteral("not-yet"));
    QCOMPARE(execute(HeritageIntranetFlow::agreeTermsScript()), QStringLiteral("not-found"));
    QCOMPARE(execute(HeritageIntranetFlow::openDownloadPageScript()), QStringLiteral("opening"));
    QCOMPARE(execute(HeritageIntranetFlow::openDownloadPageScript()), QStringLiteral("already-open"));
    const auto receipt = QJsonDocument::fromJson(execute(HeritageIntranetFlow::agreeTermsScript()).toUtf8()).object();
    QCOMPARE(receipt.value("status").toString(), QStringLiteral("agreed"));
    QVERIFY(receipt.value("text").toString().contains(QStringLiteral("제3자 양도 금지")));
    QCOMPARE(execute(QStringLiteral("String(opened)+':'+String(wrongConfirm)+':'+String(rightConfirm)")), QStringLiteral("1:0:1"));
  }

  void downloadFormsUseTheirOwnMunicipalFields_data() {
    QTest::addColumn<QString>("mode");
    for (const auto& mode : {"B", "U", "R", "E"}) QTest::newRow(mode) << QString::fromLatin1(mode);
  }

  void downloadFormsUseTheirOwnMunicipalFields() {
    QFETCH(QString, mode);
    QTemporaryDir profile;
    BlockPortal blocker;
    KaHeritageBrowser browser;
    browser.m_profile->setPersistentStoragePath(profile.path());
    browser.m_profile->setUrlRequestInterceptor(&blocker);
    auto* view = browser.addPage();
    QSignalSpy loaded(view, &QWebEngineView::loadFinished);
    view->setHtml(QStringLiteral(
        "<html><body><form id='mapSearch'><select id=bjdcd1_newJbn name=bjdCdNewAddr>"
        "<option value='11'>서울특별시</option></select><button onclick='mapSearches++;return false'>검색</button></form>"
        "<div id=tabContentDiv><form id=searchForm><input id=mode name=mode value='%1'>"
        "<select id=bjdcd1_bcheF name=bjdCd onchange='regionChanges++'>"
        "<option value=''>시도선택</option><option value='47'>경상북도</option></select>"
        "<select id=bjdcd2_bcheF name=bjdCd1 onchange='regionChanges++'>"
        "<option value=''>시/군/구선택</option><option value='47111'>포항시 남구</option>"
        "<option value='47110'>포항시</option></select>"
        "<select name=bjdCd2><option value=''>읍/면/동선택</option></select>"
        "<select name=bjdCd3><option value=''>리선택</option></select>"
        "<button onclick='downloadSearches++;return false'>검색</button></form></div>"
        "<script>var mapSearches=0,downloadSearches=0,regionChanges=0;</script></body></html>").arg(mode));
    QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 15000);
    auto execute = [&](const QString& script) {
      QString value;
      QEventLoop loop;
      browser.runScript(script, [&](const QString& answer) { value=answer;loop.quit(); }, true);
      QTimer::singleShot(5000, &loop, &QEventLoop::quit);
      if (value.isEmpty()) loop.exec();
      return value;
    };
    QCOMPARE(execute(HeritageIntranetFlow::formHintScript()), QStringLiteral("codedeta"));
    QCOMPARE(execute(HeritageIntranetFlow::selectRegionScript(QStringLiteral("경상북도"),QStringLiteral("포항시"))), QStringLiteral("selected"));
    QCOMPARE(execute(HeritageIntranetFlow::verifyRegionScript()), QStringLiteral("ok:47|47110"));
    QCOMPARE(execute(HeritageIntranetFlow::searchScript()), QStringLiteral("searching"));
    const auto plan = QJsonDocument::fromJson(execute(HeritageIntranetFlow::buildDownloadUrlScript(mode)).toUtf8()).object();
    const auto params = plan.value(QStringLiteral("params")).toArray();
    QVERIFY(!params.isEmpty());
    QMap<QString,QString> values;
    for (const auto& param : params) values.insert(param.toArray().at(0).toString(), param.toArray().at(1).toString());
    QCOMPARE(values.value("bjdCd"), QStringLiteral("47"));
    QCOMPARE(values.value("bjdCd1"), QStringLiteral("47110"));
    QCOMPARE(values.value("tab"), mode);
    QCOMPARE(values.value("bjdCd2"), QString());
    QVERIFY(!values.contains("bjdCdNewAddr"));
    QCOMPARE(execute(QStringLiteral("String(mapSearches)+':'+String(downloadSearches)+':'+String(regionChanges)")), QStringLiteral("0:1:2"));
    browser.m_profile->setUrlRequestInterceptor(nullptr);
  }

  void rejectedImportCannotFinishDownload() {
    QTemporaryDir files;
    QFile received(files.filePath(QStringLiteral("received.zip")));
    QVERIFY(received.open(QIODevice::WriteOnly));
    received.write("received but invalid ZIP");
    received.close();
    KaHeritageBrowser browser;
    browser.setTarget(QStringLiteral("경상북도"), QStringLiteral("포항시"),
                      {HeritageDataset::DesignatedHeritage});
    browser.m_running = true;
    browser.m_stage = HeritageStage::Download;
    browser.m_downloadMode = QStringLiteral("all");
    browser.m_downloadStarted = true;
    browser.m_currentFiles = {received.fileName()};
    browser.m_idleTicks = 3;
    connect(&browser, &KaHeritageBrowser::datasetReady, &browser,
            [&](HeritageDataset, const QStringList&) { browser.rejectDataset(QStringLiteral("적재 실패")); });
    QSignalSpy success(&browser, &KaHeritageBrowser::allFinished);
    browser.runCurrentStage();
    QCOMPARE(success.count(), 0);
    QCOMPARE(browser.stage(), HeritageStage::Failed);
    QCOMPARE(browser.m_datasetIndex, 0);
  }

  void datasetRequestIsSentOnceAndWaitsForNewForm_data() {
    QTest::addColumn<QString>("oldResult");
    QTest::addColumn<bool>("designated");
    QTest::newRow("previous-dataset-count") << QStringLiteral("검색결과 8,302건") << false;
    QTest::newRow("no-result-yet") << QString() << false;
    QTest::newRow("designated-text-click") << QStringLiteral("검색결과 8,302건") << true;
  }

  void datasetRequestIsSentOnceAndWaitsForNewForm() {
    QFETCH(QString, oldResult);
    QFETCH(bool, designated);
    QTemporaryDir profile;
    BlockPortal blocker;
    KaHeritageBrowser browser;
    browser.m_profile->setPersistentStoragePath(profile.path());
    browser.m_profile->setUrlRequestInterceptor(&blocker);
    auto* view = browser.addPage();
    QSignalSpy loaded(view, &QWebEngineView::loadFinished);
    view->setHtml(QStringLiteral(
        "<html><body><ul><li>지정유산</li></ul>"
        "<button onclick=\"changeTab('S')\">지정유산</button>"
        "<div id='tabContentDiv'><form id='searchForm'>"
        "<select name='codedetaCd'><option>시도선택</option></select>"
        "<input id='mode' value='S'><span>%1</span></form></div>"
        "<script>var requests=0;function changeTab(code){requests++;"
        // A tab can change before its AJAX result arrives. Keep the old form
        // until the test explicitly delivers the response, without timing races.
        "document.getElementById('mode').value=code;}</script></body></html>").arg(oldResult));
    QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 15000);
    browser.setTarget(QStringLiteral("경상북도"), QStringLiteral("포항시"),
                      {designated ? HeritageDataset::DesignatedHeritage : HeritageDataset::BuriedHeritageArea});
    browser.m_stage = HeritageStage::SelectDataset;
    browser.m_running = true;
    for (int tick = 0; tick < 4; ++tick) {
      browser.runStage();
      QTRY_VERIFY_WITH_TIMEOUT(!browser.m_scriptInFlight, 5000);
    }
    int requests = -1;
    view->page()->runJavaScript(QStringLiteral("requests"),
                                [&](const QVariant& result) { requests = result.toInt(); });
    QTRY_VERIFY_WITH_TIMEOUT(requests >= 0, 5000);
    QCOMPARE(requests, 1);
    QCOMPARE(browser.stage(), HeritageStage::SelectDataset);
    QCOMPARE(browser.m_totalBeforeSearch, -1);
    if (!designated) {
      bool wrongResponse = false;
      view->page()->runJavaScript(QStringLiteral(
          "document.getElementById('tabContentDiv').innerHTML="
          "'<form id=searchForm><select name=codedetaCd><option>시도선택</option></select>"
          "<input id=mode value=R>검색결과 99건</form>';true"),
          [&](const QVariant&) { wrongResponse = true; });
      QTRY_VERIFY_WITH_TIMEOUT(wrongResponse, 5000);
      browser.runStage();
      QTRY_VERIFY_WITH_TIMEOUT(!browser.m_scriptInFlight, 5000);
      QCOMPARE(browser.stage(), HeritageStage::SelectDataset);
      QCOMPARE(browser.m_totalBeforeSearch, -1);
    }
    bool delivered = false;
    view->page()->runJavaScript(QStringLiteral(
        "document.getElementById('tabContentDiv').innerHTML="
        "'<form id=searchForm><select name=codedetaCd><option>시도선택</option></select>"
        "<input id=mode value=%1>검색결과 37건</form>';true").arg(designated ? "S" : "B"),
        [&](const QVariant&) { delivered = true; });
    QTRY_VERIFY_WITH_TIMEOUT(delivered, 5000);
    browser.runStage();
    QTRY_VERIFY_WITH_TIMEOUT(!browser.m_scriptInFlight, 5000);
    QCOMPARE(browser.stage(), HeritageStage::SelectRegion);
    QCOMPARE(browser.m_totalBeforeSearch, 37);
    browser.stop();
    browser.m_profile->setUrlRequestInterceptor(nullptr);
  }

  void normalRootDoesNotConsumeErrorRecovery() {
    QTemporaryDir profile;
    BlockPortal blocker;
    KaHeritageBrowser browser;
    browser.m_profile->setPersistentStoragePath(profile.path());
    browser.m_profile->setUrlRequestInterceptor(&blocker);
    auto* view = browser.addPage();
    QSignalSpy loaded(view, &QWebEngineView::loadFinished);
    view->setHtml(QStringLiteral("<html><body><nav>정상 로그인 셸</nav></body></html>"),
                  HeritageIntranetFlow::portalUrl());
    QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 15000);
    browser.m_stage = HeritageStage::DismissTutorial;
    browser.m_running = true;
    browser.runStage();
    QTRY_COMPARE_WITH_TIMEOUT(browser.stage(), HeritageStage::OpenDownloadPage, 5000);
    QCOMPARE(blocker.homeRequests.load(), 0);
    QVERIFY(!browser.m_homeRecovered);
    browser.stop();
    browser.m_profile->setUrlRequestInterceptor(nullptr);
  }

  void currentDesignatedFormNeedsNoReload() {
    QTemporaryDir profile;
    BlockPortal blocker;
    KaHeritageBrowser browser;
    browser.m_profile->setPersistentStoragePath(profile.path());
    browser.m_profile->setUrlRequestInterceptor(&blocker);
    auto* view = browser.addPage();
    QSignalSpy loaded(view, &QWebEngineView::loadFinished);
    view->setHtml(QStringLiteral(
        "<html><body><li>지정유산</li><ul><li id='tabS' class='taba1 on'>"
        "<a href=\"javascript:changeTab('S')\">지정유산</a></li></ul>"
        "<form id='searchForm'><select name='codedetaCd'><option>시도선택</option></select>"
        "<input id='mode' value='S'>검색결과 8,302건</form>"
        "<script>var requests=0;function changeTab(code){requests++;}</script></body></html>"));
    QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 15000);
    browser.setTarget(QStringLiteral("경상북도"), QStringLiteral("포항시"),
                      {HeritageDataset::DesignatedHeritage});
    browser.m_stage = HeritageStage::SelectDataset;
    browser.m_running = true;
    browser.runStage();
    QTRY_VERIFY_WITH_TIMEOUT(!browser.m_scriptInFlight, 5000);
    QCOMPARE(browser.stage(), HeritageStage::SelectRegion);
    QCOMPARE(browser.m_totalBeforeSearch, 8302);
    int requests = -1;
    view->page()->runJavaScript(QStringLiteral("requests"),
                                [&](const QVariant& result) { requests = result.toInt(); });
    QTRY_COMPARE_WITH_TIMEOUT(requests, 0, 5000);
    browser.stop();
    browser.m_profile->setUrlRequestInterceptor(nullptr);
  }

  void errorDocumentIsNotAReadyDownloadPage() {
    QWebEngineView view;
    QSignalSpy loaded(&view, &QWebEngineView::loadFinished);
    view.setHtml(errorHtml);
    QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 15000);
    QString result;
    view.page()->runJavaScript(HeritageIntranetFlow::downloadPageProbeScript(),
                              [&](const QVariant& value) { result = value.toString(); });
    QTRY_VERIFY_WITH_TIMEOUT(!result.isEmpty(), 5000);
    QCOMPARE(result, QStringLiteral("page-error"));
  }

  void errorRecoversOnceAndNeverBecomesSuccess_data() {
    QTest::addColumn<int>("stage");
    QTest::addColumn<bool>("loadFailure");
    QTest::newRow("open-page") << int(HeritageStage::OpenDownloadPage) << false;
    QTest::newRow("dataset-inner-404") << int(HeritageStage::SelectDataset) << false;
    QTest::newRow("region-inner-404") << int(HeritageStage::SelectRegion) << false;
    QTest::newRow("main-load-failed") << int(HeritageStage::SelectDataset) << true;
  }

  void errorRecoversOnceAndNeverBecomesSuccess() {
    QFETCH(int, stage);
    QFETCH(bool, loadFailure);
    QTemporaryDir profile;
    BlockPortal blocker;
    KaHeritageBrowser browser;
    browser.m_profile->setPersistentStoragePath(profile.path());
    browser.m_profile->setUrlRequestInterceptor(&blocker);
    auto* view = browser.addPage();
    QSignalSpy loaded(view, &QWebEngineView::loadFinished);
    view->setHtml(errorHtml);
    QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 15000);
    browser.setTarget(QStringLiteral("경상북도"), QStringLiteral("포항시"),
                      {HeritageDataset::DesignatedHeritage});
    browser.m_stage = HeritageStage(stage);
    browser.m_running = true;
    if (loadFailure) emit view->loadFinished(false);
    QSignalSpy failed(&browser, &KaHeritageBrowser::failed);
    QSignalSpy success(&browser, &KaHeritageBrowser::allFinished);
    browser.m_poll->start(50);
    QTRY_COMPARE_WITH_TIMEOUT(blocker.homeRequests.load(), 1, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!failed.isEmpty(), 10000);
    QCOMPARE(success.count(), 0);
    QCOMPARE(blocker.homeRequests.load(), 1);
    QCOMPARE(browser.stage(), HeritageStage::Failed);
    browser.stop();
    browser.m_profile->setUrlRequestInterceptor(nullptr);
  }

  void missingFormDoesNotNavigateIntoAFrame() {
    QTemporaryDir profile;
    BlockPortal blocker;
    KaHeritageBrowser browser;
    browser.m_profile->setPersistentStoragePath(profile.path());
    browser.m_profile->setUrlRequestInterceptor(&blocker);
    auto* view = browser.addPage();
    QSignalSpy loaded(view, &QWebEngineView::loadFinished);
    view->setHtml(QStringLiteral("<html><body><iframe src='https://intranet.gis-heritage.go.kr/ngis/common/main.do'></iframe></body></html>"));
    QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 15000);
    QString result;
    browser.runScript(QStringLiteral("'not-found'"),
                      [&](const QString& value) { result = value; }, true);
    QTRY_VERIFY_WITH_TIMEOUT(!result.isEmpty(), 5000);
    QCOMPARE(browser.m_tabs->count(), 1);
    QVERIFY(browser.m_pageReady);
    browser.m_profile->setUrlRequestInterceptor(nullptr);
  }

  void liveInspectWholeDownloadButton() {
    if (!qEnvironmentVariableIsSet("KA_HGIS_HERITAGE_INSPECT_WHOLE_BUTTON"))
      QSKIP("Read-only live button inspection is opt-in; no file request is permitted.");
    class BlockFileRequests final : public QWebEngineUrlRequestInterceptor {
    public:
      std::atomic<int> attemptedDownloads{0};
      void interceptRequest(QWebEngineUrlRequestInfo& info) override {
        if (info.requestUrl().path().contains(QStringLiteral("downloadFiles"), Qt::CaseInsensitive) ||
            info.requestUrl().path().contains(QStringLiteral("downShp"), Qt::CaseInsensitive)) {
          ++attemptedDownloads;
          info.block(true);
        }
      }
    } blocker;
    QTemporaryDir profile;
    KaHeritageBrowser browser;
    browser.m_profile->setPersistentStoragePath(profile.path());
    browser.m_profile->setCachePath(profile.filePath(QStringLiteral("cache")));
    browser.m_profile->setUrlRequestInterceptor(&blocker);
    const QDir qa(QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                     .filePath(QStringLiteral("주변유적-QA/경상북도 포항시")));
    browser.setDownloadRoot(qa.filePath(QStringLiteral("원본")));
    browser.setTarget(QStringLiteral("경상북도"), QStringLiteral("포항시"), {HeritageDataset::DesignatedHeritage});
    connect(&browser, &KaHeritageBrowser::stageChanged, &browser, [&](HeritageStage stage, const QString&) {
      if (stage == HeritageStage::CollectResults || stage == HeritageStage::Download) browser.m_poll->stop();
    });
    QSignalSpy failed(&browser, &KaHeritageBrowser::failed);
    QSignalSpy downloads(browser.m_profile, &QWebEngineProfile::downloadRequested);
    browser.start();
    QTRY_VERIFY_WITH_TIMEOUT(browser.stage() == HeritageStage::CollectResults || !failed.isEmpty(), 180000);
    QVERIFY2(failed.isEmpty(), "Live read-only inspection failed; do not retry automatically.");
    QTest::qWait(1500); // Let the one already-issued municipal search update its DOM.
    QString contract;
    browser.runScript(QStringLiteral(R"JS((function(){
      var out=[];
      function cleanPath(s){return String(s||'').split('?')[0].split('#')[0].replace(/;[^/\s]*/g,'');}
      function walk(w,n){if(n>6)return;try{
        var d=w.document,scope=d.getElementById('tabContentDiv');
        if(scope&&scope.querySelector('#searchForm')){
          var buttons=Array.from(scope.querySelectorAll('button,a,input[type=button],input[type=submit]'))
            .filter(e=>(e.innerText||e.value||'').replace(/\s+/g,'')==='전체다운로드')
            .map(e=>{
              var onclick=e.getAttribute('onclick')||'',href=e.getAttribute('href')||'';
              var names=Array.from((onclick+' '+href).matchAll(/([A-Za-z_$][\w$]*)\s*\(/g)).map(m=>m[1]);
              var functions={};Array.from(new Set(names)).forEach(name=>{
                if(typeof w[name]==='function')functions[name]=String(w[name]);
              });
              var ancestors=[],p=e.parentElement;
              for(var i=0;p&&i<5;i++,p=p.parentElement)ancestors.push({tag:p.tagName,id:p.id||''});
              return {text:(e.innerText||e.value||'').trim(),id:e.id||'',onclick:onclick,
                href:cleanPath(href),onclickFunction:typeof e.onclick==='function'?String(e.onclick):'',
                ancestors:ancestors,functions:functions};
            });
          out.push({path:cleanPath(w.location.pathname),buttons:buttons,
            globalSearchGisChaRirList:typeof w.searchGisChaRirList==='function'?String(w.searchGisChaRirList):''});
        }
        for(var i=0;i<w.frames.length;i++)walk(w.frames[i],n+1);
      }catch(e){}}walk(window,0);return JSON.stringify(out);})())JS"),
      [&](const QString& value) { contract = value; }, true);
    QTRY_VERIFY_WITH_TIMEOUT(!contract.isEmpty(), 10000);
    browser.stop();
    QCOMPARE(blocker.attemptedDownloads.load(), 0);
    QCOMPARE(downloads.size(), 0);
    const auto parsed = QJsonDocument::fromJson(contract.toUtf8());
    QVERIFY(parsed.isArray());
    QVERIFY(!parsed.array().isEmpty());
    const QString receipt = qa.filePath(QStringLiteral("receipts/qa-whole-download-button-contract.json"));
    QVERIFY(QDir().mkpath(QFileInfo(receipt).absolutePath()));
    QSaveFile file(receipt);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(parsed.toJson()) > 0);
    QVERIFY(file.commit());
    qInfo().noquote() << "Official button contract receipt:" << receipt;
    qInfo() << "Download requests=" << downloads.size() << "blocked download attempts=" << blocker.attemptedDownloads.load();
    browser.m_profile->setUrlRequestInterceptor(nullptr);
  }

  void liveDesignatedDownload() {
    const QString city = qEnvironmentVariable("KA_HGIS_HERITAGE_LIVE_CITY");
    const QString sido = qEnvironmentVariable("KA_HGIS_HERITAGE_LIVE_SIDO");
    if (city.isEmpty() || sido.isEmpty()) QSKIP("Live municipal download is opt-in.");
    QTemporaryDir profile;
    KaHeritageBrowser browser;
    browser.m_profile->setPersistentStoragePath(profile.path());
    browser.m_profile->setCachePath(profile.filePath(QStringLiteral("cache")));
    const QString root = QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                             .filePath(QStringLiteral("주변유적-QA/%1 %2/원본").arg(sido, city));
    browser.setDownloadRoot(root);
    const bool all = qEnvironmentVariableIsSet("KA_HGIS_HERITAGE_LIVE_ALL");
    const auto datasets = all ? HeritageStyle::allDatasets()
                              : QVector<HeritageDataset>{HeritageDataset::DesignatedHeritage};
    browser.setTarget(sido, city, datasets);
    QJsonArray manifest;
    connect(&browser, &KaHeritageBrowser::datasetReady, &browser,
            [&](HeritageDataset ds, const QStringList& files) {
              manifest.append(QJsonObject{{"dataset", int(ds)}, {"files", QJsonArray::fromStringList(files)}});
              const QString receipts = QDir(root).absoluteFilePath(QStringLiteral("../receipts"));
              QDir().mkpath(receipts);
              QSaveFile output(QDir(receipts).filePath(QStringLiteral("qa-manifest.json")));
              QVERIFY(output.open(QIODevice::WriteOnly));
              output.write(QJsonDocument(manifest).toJson());
              QVERIFY(output.commit());
            });
    QSignalSpy success(&browser, &KaHeritageBrowser::datasetReady);
    QSignalSpy failed(&browser, &KaHeritageBrowser::failed);
    QSignalSpy finished(&browser, &KaHeritageBrowser::allFinished);
    browser.start();
    QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty() || !failed.isEmpty(), 540000);
    if (!failed.isEmpty()) {
      QString shape;
      browser.runScript(QStringLiteral(R"JS((function(){var result=[];
        function walk(w,n){if(n>6)return;try{var d=w.document;
          var f=d.getElementById('searchForm');if(f)result.push({
            mode:d.getElementById('mode')?d.getElementById('mode').value:null,
            fields:Array.from(f.querySelectorAll('input,select')).map(e=>({id:e.id,name:e.name,
              options:e.options?Array.from(e.options).slice(0,4).map(o=>({value:o.value,text:o.text})):undefined}))});
          for(var i=0;i<w.frames.length;i++)walk(w.frames[i],n+1);
        }catch(e){}}walk(window,0);return JSON.stringify(result);})())JS"),
          [&](const QString& value) { shape = value; });
      QTRY_VERIFY_WITH_TIMEOUT(!shape.isEmpty(), 10000);
      QFile shapeFile(QStringLiteral("build/qa/heritage-failed-form.json"));
      QVERIFY(shapeFile.open(QIODevice::WriteOnly));
      shapeFile.write(shape.toUtf8());
    }
    browser.stop();
    QVERIFY2(failed.isEmpty(), "Live flow failed; read the diagnostic receipt, not credentials.");
    QCOMPARE(success.count(), datasets.size());
    for (const auto& event : success) {
      const QStringList files = event.at(1).toStringList();
      QVERIFY(!files.isEmpty());
      for (const QString& file : files) QVERIFY(QFileInfo(file).size() > 0);
    }
  }

  void liveInspectDatasetContract() {
    if (!qEnvironmentVariableIsSet("KA_HGIS_HERITAGE_INSPECT")) QSKIP("Live inspection is opt-in.");
    QTemporaryDir profile;
    KaHeritageBrowser browser;
    browser.m_profile->setPersistentStoragePath(profile.path());
    browser.m_profile->setCachePath(profile.filePath(QStringLiteral("cache")));
    browser.setDownloadRoot(QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                                .filePath(QStringLiteral("주변유적-QA/경상북도 포항시/원본")));
    browser.setTarget(QStringLiteral("경상북도"), QStringLiteral("포항시"),
                      {HeritageDataset::DesignatedHeritage});
    connect(&browser, &KaHeritageBrowser::stageChanged, &browser,
            [&](HeritageStage stage, const QString&) {
              if (stage == HeritageStage::SelectDataset) browser.m_poll->stop();
            });
    QSignalSpy failed(&browser, &KaHeritageBrowser::failed);
    browser.start();
    QTRY_VERIFY_WITH_TIMEOUT(browser.stage() == HeritageStage::SelectDataset || !failed.isEmpty(), 120000);
    QVERIFY(failed.isEmpty());
    bool formReady = false;
    for (int attempt = 0; attempt < 30 && !formReady; ++attempt) {
      QString hint;
      browser.runScript(HeritageIntranetFlow::formHintScript(),
                        [&](const QString& value) { hint = value; }, true);
      QTRY_VERIFY_WITH_TIMEOUT(!hint.isEmpty(), 5000);
      formReady = hint == QLatin1String("codedeta");
      if (!formReady) QTest::qWait(500);
    }
    QVERIFY(formReady);
    QString contract;
    browser.runScript(QStringLiteral(R"JS((function(){
      var out=[];function walk(w,n){if(n>6)return;try{
        var d=w.document,f=d.getElementById('searchForm');
        if(f){out.push({mode:d.getElementById('mode')?d.getElementById('mode').value:null,
          fields:Array.from(f.querySelectorAll('input,select')).map(e=>({id:e.id,name:e.name})),
          tabs:Array.from(d.querySelectorAll('a,li,button')).filter(e=>['지정유산','현상변경허용기준','매장유산유존지역','문화유적분포지도','지표조사구역','발굴조사구역'].includes((e.innerText||'').trim())).map(e=>({tag:e.tagName,id:e.id,cls:e.className,text:e.innerText,onclick:e.getAttribute('onclick'),href:e.getAttribute('href')})),
          changeTab:typeof w.changeTab==='function'?String(w.changeTab):'',
          tabContentAjax:typeof w.tabContentAjax==='function'?String(w.tabContentAjax):''});}
        for(var i=0;i<w.frames.length;i++)walk(w.frames[i],n+1);
      }catch(e){}}walk(window,0);return JSON.stringify(out);})())JS"),
      [&](const QString& value) { contract = value; });
    QTRY_VERIFY_WITH_TIMEOUT(!contract.isEmpty(), 10000);
    QVERIFY(contract != QLatin1String("[]"));
    QFile output(QStringLiteral("build/qa/heritage-tab-contract.json"));
    QVERIFY(output.open(QIODevice::WriteOnly));
    output.write(contract.toUtf8());
    browser.stop();
  }

  void liveInspectAgreement() {
    if (!qEnvironmentVariableIsSet("KA_HGIS_HERITAGE_INSPECT_AGREEMENT")) QSKIP("Live agreement inspection is opt-in.");
    QTemporaryDir profile;
    KaHeritageBrowser browser;
    browser.m_profile->setPersistentStoragePath(profile.path());
    browser.m_profile->setCachePath(profile.filePath(QStringLiteral("cache")));
    browser.setDownloadRoot(QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                                .filePath(QStringLiteral("주변유적-QA/경상북도 포항시/원본")));
    browser.setTarget(QStringLiteral("경상북도"), QStringLiteral("포항시"), {HeritageDataset::DesignatedHeritage});
    connect(&browser, &KaHeritageBrowser::stageChanged, &browser, [&](HeritageStage stage, const QString&) {
      if (stage == HeritageStage::OpenDownloadPage) browser.m_poll->stop();
    });
    QSignalSpy failed(&browser, &KaHeritageBrowser::failed);
    browser.start();
    QTRY_VERIFY_WITH_TIMEOUT(browser.stage() == HeritageStage::OpenDownloadPage || !failed.isEmpty(), 90000);
    QVERIFY(failed.isEmpty());
    QString opened;
    browser.runScript(HeritageIntranetFlow::openDownloadPageScript(), [&](const QString& r){opened=r;});
    QTRY_VERIFY_WITH_TIMEOUT(!opened.isEmpty(), 10000);
    QTest::qWait(2000);
    QString result;
    browser.runScript(QStringLiteral(R"JS((function(){var out=[];function walk(w,n){if(n>6)return;try{
      var d=w.document;
      function brief(e){var s=w.getComputedStyle(e);return {tag:e.tagName,id:e.id,name:e.name,display:s.display,visibility:s.visibility,
        text:(e.innerText||'').slice(0,400),onclick:e.getAttribute('onclick'),cls:e.className};}
      var boxes=Array.from(d.querySelectorAll('input[type=checkbox]')).map(e=>({id:e.id,checked:e.checked,
        ancestors:[e,e.parentElement,e.parentElement&&e.parentElement.parentElement].filter(Boolean).map(brief),
        labels:e.labels?Array.from(e.labels).map(brief):[]}));
      var confirms=Array.from(d.querySelectorAll('button,a,input[type=button],input[type=submit]'))
        .filter(e=>(e.innerText||e.value||'').trim()==='확인').map(e=>({element:brief(e),parent:brief(e.parentElement)}));
      if(boxes.length||confirms.length||typeof w.showAgreePopup==='function')out.push({path:w.location.pathname,boxes:boxes,confirms:confirms,
        showAgree:typeof w.showAgreePopup==='function'?String(w.showAgreePopup):''});
      for(var i=0;i<w.frames.length;i++)walk(w.frames[i],n+1);
    }catch(e){}}walk(window,0);return JSON.stringify(out);})())JS"),[&](const QString& r){result=r;});
    QTRY_VERIFY_WITH_TIMEOUT(!result.isEmpty(), 10000);
    QFile output(QStringLiteral("build/qa/heritage-agreement-contract.json"));
    QVERIFY(output.open(QIODevice::WriteOnly));output.write(result.toUtf8());
    browser.stop();
    QVERIFY(result!=QLatin1String("[]"));
  }
};

int main(int argc, char** argv) {
  QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
  KaPortableRuntime::applyWebEngineFlags();
  QApplication app(argc, argv);
  // 실제 앱과 같은 이름을 쓰면 WebEngine 프로필과 설정이 사용자의 실제 폴더에 쌓인다.
  QCoreApplication::setOrganizationName(QStringLiteral("ka-hgis-offline-tests"));
  QCoreApplication::setApplicationName(QStringLiteral("heritage-browser"));
  QStandardPaths::setTestModeEnabled(true);
  HeritageBrowserTest test;
  return QTest::qExec(&test, argc, argv);
}
#include "test_heritage_browser.moc"
