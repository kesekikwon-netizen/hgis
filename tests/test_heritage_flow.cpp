#include <QtTest>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QFile>
#include <QTemporaryDir>

#include "core/HeritageIntranetFlow.h"
#include "core/HeritageIntranetSettings.h"
#include "core/HeritageStyle.h"

// 국가유산 인트라넷 자동화의 단계·스크립트. 사이트 접속 없이 검사한다.
class HeritageIntranetFlowTest : public QObject {
  Q_OBJECT

  // jsString 이 만든 리터럴이 실제로 그 문자열로 되읽히는지.
  static QString roundTrip(const QString& value) {
    const QString literal = HeritageIntranetFlow::jsString(value);
    QJsonParseError err{};
    const QJsonDocument doc =
        QJsonDocument::fromJson(QStringLiteral("[%1]").arg(literal).toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) return QStringLiteral("<invalid>");
    return doc.array().at(0).toString();
  }

private slots:
  void loginUsesTheRealFieldsAndThePagesOwnEncryption() {
    const QString js = HeritageIntranetFlow::loginScript(QStringLiteral("A0298"),
                                                         QStringLiteral("secret"));
    // 2026-09-11 실제 화면에서 확인한 것들
    QVERIFY(js.contains(QStringLiteral("getElementById('userId')")));
    QVERIFY(js.contains(QStringLiteral("getElementById('pwd')")));
    // 이 사이트는 브라우저에서 RSA 로 암호화한다. 우리가 암호화하지 않고 goLogin() 에 맡겨야 한다.
    QVERIFY(js.contains(QStringLiteral("goLogin()")));
    // 평문을 직접 POST 하지 않는다.
    QVERIFY(!js.contains(QStringLiteral("j_spring_security_check")));
    QVERIFY(!js.contains(QStringLiteral("USER_PW")));
    // 못 찾으면 못 찾았다고 말한다.
    QVERIFY(js.contains(QStringLiteral("no-form")));
    QVERIFY(js.contains(QStringLiteral("no-goLogin")));
  }

  void passwordWithQuotesAndBackslashesSurvives() {
    const QStringList nasty = {
        QStringLiteral("plain"),
        QStringLiteral("with'single"),
        QStringLiteral("with\"double"),
        QStringLiteral("back\\slash"),
        QStringLiteral("line\nbreak"),
        QStringLiteral("한글비밀번호"),
        QStringLiteral("</script>"),
    };
    for (const QString& value : nasty)
      QCOMPARE(roundTrip(value), value);
  }

  void scriptStaysValidWithAHostilePassword() {
    // 스크립트가 깨지면 아이디·비밀번호가 페이지에 잘못 흘러들 수 있다.
    const QString js = HeritageIntranetFlow::loginScript(
        QStringLiteral("A0298"), QStringLiteral("a'b\"c\\d\ne"));
    // 리터럴 두 개가 모두 온전한 JSON 문자열이어야 한다.
    QCOMPARE(roundTrip(QStringLiteral("a'b\"c\\d\ne")), QStringLiteral("a'b\"c\\d\ne"));
    QVERIFY(!js.contains(QLatin1Char('\n')));  // 스크립트 안에 실제 줄바꿈이 남으면 안 된다
  }

  void unverifiedStagesAreMarkedUnverified() {
    // 모르는 것을 아는 척하지 않는다. 로그인만 실제 화면으로 확인했다.
    QVERIFY(HeritageIntranetFlow::isVerified(HeritageStage::Login));
    for (HeritageStage stage : {HeritageStage::DismissTutorial, HeritageStage::OpenDownloadPage,
                                HeritageStage::AgreeTerms, HeritageStage::SelectDataset,
                                HeritageStage::SelectRegion, HeritageStage::Search,
                                HeritageStage::CollectResults, HeritageStage::Download})
      QVERIFY2(!HeritageIntranetFlow::isVerified(stage),
               qPrintable(HeritageIntranetFlow::stageName(stage)));
  }

  void everyStageHasAKoreanName() {
    for (HeritageStage stage :
         {HeritageStage::Idle, HeritageStage::Login, HeritageStage::DismissTutorial,
          HeritageStage::OpenDownloadPage, HeritageStage::AgreeTerms, HeritageStage::SelectDataset,
          HeritageStage::SelectRegion, HeritageStage::Search, HeritageStage::CollectResults,
          HeritageStage::Download, HeritageStage::Done, HeritageStage::Failed}) {
      const QString name = HeritageIntranetFlow::stageName(stage);
      QVERIFY(!name.isEmpty());
      QVERIFY2(name != QStringLiteral("알 수 없음"), qPrintable(name));
    }
  }

  void everyStepReportsNotFoundInsteadOfPretending() {
    // openDownloadPageScript 는 찾는 단계가 아니라 **가는** 단계다.
    // 도착 확인은 downloadPageProbeScript 가 따로 한다(아래 검사).
    const QStringList scripts = {
        HeritageIntranetFlow::dismissTutorialScript(),
        HeritageIntranetFlow::agreeTermsScript(),
        HeritageIntranetFlow::selectRegionScript(QStringLiteral("경상북도"), QStringLiteral("안동시")),
        HeritageIntranetFlow::searchScript(),
    };
    for (const QString& js : scripts) {
      // 못 찾았으면 못 찾았다고 하거나(not-found/no-sido), 할 일이 없다고(none) 말해야 한다.
      QVERIFY2(js.contains(QStringLiteral("not-found")) || js.contains(QStringLiteral("no-sido")) ||
                   js.contains(QStringLiteral("'none'")),
               qPrintable(js.left(60)));
      // 예외를 삼키고 성공처럼 보이면 안 된다.
      QVERIFY(js.contains(QStringLiteral("catch")));
    }
  }

  void everyDatasetSelectsItsOfficialTabAndWaitsForTheNewForm() {
    const QStringList codes = {QStringLiteral("S"), QStringLiteral("P"), QStringLiteral("B"),
                               QStringLiteral("U"), QStringLiteral("R"), QStringLiteral("E")};
    const auto datasets = HeritageStyle::allDatasets();
    QCOMPARE(datasets.size(), codes.size());
    for (qsizetype i = 0; i < datasets.size(); ++i) {
      const QString js = HeritageIntranetFlow::selectDatasetScript(datasets[i]);
      QVERIFY(js.contains(QStringLiteral("fn('changeTab')")));
      QVERIFY(js.contains(QStringLiteral("tab(%1)").arg(HeritageIntranetFlow::jsString(codes[i]))));
      QVERIFY(js.contains(QStringLiteral("no-changeTab")));
      QVERIFY(js.contains(QStringLiteral("if(!oldForm)return 'not-found'")));
      const QString ready = HeritageIntranetFlow::datasetReadyScript(datasets[i]);
      QVERIFY(ready.contains(QStringLiteral("form===W.__kaHeritagePreviousForm")));
      QVERIFY(ready.contains(QStringLiteral("var code=%1").arg(HeritageIntranetFlow::jsString(codes[i]))));
      QVERIFY(ready.contains(QStringLiteral("mode.value!==code")));
      QVERIFY(ready.contains(QStringLiteral("return 'pending'")));
      QVERIFY(ready.contains(QStringLiteral("return 'ready'")));
    }
  }

  void regionStepStopsAtSigunguAndNeverGoesDeeper() {
    const QString js =
        HeritageIntranetFlow::selectRegionScript(QStringLiteral("경상북도"), QStringLiteral("안동시"));
    QVERIFY(js.contains(QStringLiteral("경상북도")));
    QVERIFY(js.contains(QStringLiteral("안동시")));
    // 시/군까지만 받는다. 읍면동·리를 고르는 코드가 있으면 안 된다.
    QVERIFY(!js.contains(QStringLiteral("읍면동")));
    QVERIFY(!js.contains(QStringLiteral("emd")));
    QVERIFY(!js.contains(QStringLiteral("sels[2]")));
    QVERIFY(!js.contains(QStringLiteral("sels[3]")));
  }

  void downloadPageIsOpenedByItsRealUrlAndArrivalIsChecked() {
    // 주소는 요청 기록에서 나온 그대로. 지어내지 않는다.
    QCOMPARE(HeritageIntranetFlow::downloadPagePath(),
             QStringLiteral("/user/data/heritageDownload.do?pageMenuId=DAT010010"));
    QCOMPARE(HeritageIntranetFlow::downloadListPath(),
             QStringLiteral("/user/data/heritageDownloadList.do"));

    const QString open = HeritageIntranetFlow::openDownloadPageScript();
    // 조각을 최상위로 직접 열면 jQuery·tabContentAjax 가 없는 반쪽 페이지가 된다.
    // 사이트가 제 방식대로 삽입하게 둔다.
    QVERIFY(!open.contains(QStringLiteral("form.submit")));
    QVERIFY(!open.contains(QStringLiteral("window.top")));
    QVERIFY(open.contains(QStringLiteral("showAgreePopup")));
    QVERIFY(open.contains(QStringLiteral("c[i].showAgreePopup()")));
    QVERIFY(open.contains(QStringLiteral("return 'opening'")));
    QVERIFY(open.contains(QStringLiteral("downloadForm(c[i].document)||agreementBox(c[i].document)")));
    QVERIFY(open.contains(QStringLiteral("not-found")));

    // 목록은 사이트 자신의 전송 통로로 부른다.
    const QString load = HeritageIntranetFlow::loadListScript(QStringLiteral("a=1"));
    QVERIFY(load.contains(HeritageIntranetFlow::downloadListPath()));
    QVERIFY(load.contains(QStringLiteral("host.tabContentAjax")));
    QVERIFY(load.contains(QStringLiteral("no-fn")));

    const QString probe = HeritageIntranetFlow::downloadPageProbeScript();
    QVERIFY(probe.contains(QStringLiteral("'ready'")));
    QVERIFY(probe.contains(QStringLiteral("'not-yet'")));
  }


  void searchClicksTheScopedButtonWithoutCallingTheOverwrittenMapFunction() {
    // 같은 이름의 지도 함수가 다운로드 함수를 덮어쓸 수 있으므로 공식 버튼을 누른다.
    const QString js = HeritageIntranetFlow::searchScript();
    // 폼 제출로 페이지를 옮기지 않고 검색 결과가 바뀌었는지 확인할 노드를 기억한다.
    QVERIFY(!js.contains(QStringLiteral("form.submit")));
    QVERIFY(!js.contains(QStringLiteral("searchGisChaRirList")));
    QVERIFY(js.contains(QStringLiteral("__kaHeritageSearchNode")));
    QVERIFY(js.contains(QStringLiteral("btn.click();return 'searching'")));
    QVERIFY(js.contains(QStringLiteral("not-found")));
  }

  void downloadPlanCarriesRawValuesForQtToEncodeOnce() {
    // 사이트 함수는 쓸 수 없다: 지도 스크립트(bury/select.js)가 같은 이름의 다른 함수로
    // 덮어써서 #searchHeName 을 읽다 터진다(2026-09-13 스택 확인).
    // 값은 날것으로 넘기고 인코딩은 C++ 이 한 번만 한다.
    const QString js = HeritageIntranetFlow::buildDownloadUrlScript(QStringLiteral("S"));
    QVERIFY(js.contains(QStringLiteral("JSON.stringify({path")));
    QVERIFY(js.contains(HeritageIntranetFlow::downloadFilesAllPath()));
    QVERIFY(js.contains(QStringLiteral("codedetaCd0")));
    QVERIFY(js.contains(QStringLiteral("no-region")));
    QVERIFY(js.contains(QStringLiteral("bjdcd")));
    // JS 에서 인코딩하지 않는다. 하면 Qt 가 또 해서 %25 가 된다.
    QVERIFY(!js.contains(QStringLiteral("encodeURIComponent")));
    QVERIFY(!js.contains(QStringLiteral("encodeURI(")));
  }



  

  void regionIsVerifiedAgainAfterPicking() {
    // 탭을 바꾸면 폼이 ajax 로 다시 그려진다. 「골랐다」를 한 번 보고 믿으면
    // 값이 빈 채로 전국을 받게 된다(2026-09-12 실제 발생).
    const QString js = HeritageIntranetFlow::verifyRegionScript();
    QVERIFY(js.contains(QStringLiteral("codedeta")));
    QVERIFY(js.contains(QStringLiteral("codecdsg")));
    QVERIFY(js.contains(QStringLiteral("empty")));
    QVERIFY(js.contains(QStringLiteral("'ok:'")));
    // 지도 패널은 폼이 아니다.
    QVERIFY(js.contains(QStringLiteral("bjdcd")));
  }

  void searchIsScopedToTheDownloadFormNotTheMapPanel() {
    // 왼쪽 지도 패널에도 「검색」이 있다. 그걸 누르면 조건이 안 걸린 채 전국이 검색된다.
    // 그리고 「검색」 버튼은 select 상자 **바깥 옆**에 있어서, 범위를 너무 좁히면 못 찾는다.
    // 사이트 JS 가 준 이름(#searchForm)을 쓰고, 없으면 codedeta 와 검색을 함께 품은 조상까지 올라간다.
    const QString js = HeritageIntranetFlow::searchScript();
    QVERIFY(js.contains(QStringLiteral("searchForm")));
    QVERIFY(js.contains(QStringLiteral("codedeta")));
    QVERIFY(js.contains(QStringLiteral("parentElement")));
    QVERIFY(js.contains(QStringLiteral("not-found")));
    QVERIFY(!js.contains(QStringLiteral("form.submit")));
  }

  void noScriptMayNavigateThePage() {
    // 2026-09-12 하루에 같은 실수를 세 번 했다. 전부 「페이지를 옮겨서」 사이트를 깨뜨린 것이다.
    //   ① 조각(heritageDownload.do)을 최상위로 POST → jQuery·tabContentAjax 없는 반쪽 페이지
    //   ② form.submit() → 패널이 날아가고 지도+튜토리얼로 돌아감
    //   ③ frameset 안쪽 주소로 이동 → 빈 top 프레임(하얀 화면)
    // 이 사이트는 제 방식대로 굴러가게 두어야 한다. 우리는 읽고, 사이트 함수를 부르기만 한다.
    const QStringList scripts = {
        HeritageIntranetFlow::loginProbeScript(),
        HeritageIntranetFlow::dismissTutorialScript(),
        HeritageIntranetFlow::openDownloadPageScript(),
        HeritageIntranetFlow::agreeTermsScript(),
        HeritageIntranetFlow::selectDatasetScript(HeritageDataset::BuriedHeritageArea),
        HeritageIntranetFlow::selectRegionScript(QStringLiteral("경상북도"), QStringLiteral("안동시")),
        HeritageIntranetFlow::searchScript(),
        HeritageIntranetFlow::loadListScript(QStringLiteral("a=1")),
        HeritageIntranetFlow::tabContentHtmlScript(),
        HeritageIntranetFlow::downloadPageProbeScript(),
        HeritageIntranetFlow::pageOutlineScript(),
    };
    const QStringList banned = {
        QStringLiteral("location.href="),  // ①③ 페이지 이동 (읽기는 허용, 대입만 금지)
        QStringLiteral("location.replace"),
        QStringLiteral("location.assign"),
        QStringLiteral("form.submit"),     // ② 폼 제출로 화면 넘김
        QStringLiteral("document.write"),
        QStringLiteral("window.open"),
    };
    for (const QString& js : scripts) {
      for (const QString& bad : banned) {
        QVERIFY2(!js.contains(bad),
                 qPrintable(QStringLiteral("금지된 페이지 이동(%1): %2").arg(bad, js.left(70))));
      }
    }
  }

  void termsMustBeAgreedBeforeAskingForTheList() {
    // 서약서에 동의하지 않으면 사이트가 목록을 주지 않는다(2026-09-12 화면으로 확인).
    // 동의 스크립트는 체크와 확인을 모두 해야 하고, 못 하면 그렇다고 말해야 한다.
    const QString js = HeritageIntranetFlow::agreeTermsScript();
    QVERIFY(js.contains(QStringLiteral("서약서에동의")));
    QVERIFY(js.contains(QStringLiteral("'확인'")));
    QVERIFY(js.contains(QStringLiteral("agreed")));
    QVERIFY(js.contains(QStringLiteral("checked-no-confirm")));
    QVERIFY(js.contains(QStringLiteral("not-found")));
  }

  void requestLogKeepsEndpointsButNeverLoginContent() {
    // 기록의 목적: 검색·다운로드의 진짜 주소와 파라미터를 알아내는 것.
    const QString dl = HeritageIntranetFlow::redactRequestLine(
        QStringLiteral("POST"),
        QUrl(QStringLiteral("https://intranet.gis-heritage.go.kr/ngis/gis/chaRirList.do?sido=50&sgg=50110")));
    QVERIFY(dl.contains(QStringLiteral("chaRirList.do")));
    QVERIFY(dl.contains(QStringLiteral("sgg=50110")));
    QVERIFY(dl.startsWith(QStringLiteral("POST ")));

    // 로그인 계열은 주소도 질의도 남기지 않는다. 비밀번호가 실려 간다.
    const QStringList secret = {
        QStringLiteral("https://intranet.gis-heritage.go.kr/j_spring_security_check"),
        QStringLiteral("https://intranet.gis-heritage.go.kr/user/checkNet.do?x=1"),
        QStringLiteral("https://intranet.gis-heritage.go.kr/user/loginProc.do?id=A0298"),
    };
    for (const QString& u : secret) {
      const QString line = HeritageIntranetFlow::redactRequestLine(QStringLiteral("POST"), QUrl(u));
      QVERIFY2(!line.contains(QStringLiteral("A0298")), qPrintable(line));
      QVERIFY2(!line.contains(QStringLiteral("checkNet")), qPrintable(line));
      QVERIFY2(!line.contains(QStringLiteral("security_check")), qPrintable(line));
      QVERIFY2(line.contains(QStringLiteral("로그인 요청")), qPrintable(line));
    }
  }

  void scriptsUseValidBackslashEscapes() {
    // C++ 문자열 안의 \s 는 잘못된 이스케이프라 s 가 되어 정규식이 망가진다.
    // 화면 글자의 공백을 지우지 못하면 버튼을 하나도 못 찾는다.
    const QStringList scripts = {
        HeritageIntranetFlow::dismissTutorialScript(),
        HeritageIntranetFlow::openDownloadPageScript(),
        HeritageIntranetFlow::agreeTermsScript(),
        HeritageIntranetFlow::searchScript(),
        HeritageIntranetFlow::downloadPageProbeScript(),
        HeritageIntranetFlow::selectRegionScript(QStringLiteral("경상북도"), QStringLiteral("안동시")),
    };
    for (const QString& js : scripts) {
      if (!js.contains(QStringLiteral("replace(/"))) continue;
      // 역슬래시를 소스에 직접 쓰지 않는다(이 파일도 같은 함정에 빠진다).
      const QString backslash(QChar(0x5C));
      QVERIFY2(!js.contains(QStringLiteral("replace(/s")), qPrintable(js.left(80)));
      QVERIFY2(js.contains(QStringLiteral("replace(/") + backslash + QStringLiteral("s")) ||
                   js.contains(QStringLiteral("replace(/[^0-9]")),
               qPrintable(js.left(80)));
    }
  }

  void downloadPageRequiresAVisibleDownloadFormOrAgreement() {
    // 2026-09-11 실패 outline(main.do): 지도 칸 bjdcd1_newJbn「시도 선택」과
    // 메뉴 「전체다운로드」는 메인 화면에 있다. 다운로드 폼(codedeta)은 없다.
    // 그걸 도착으로 치면 서약서·시군 선택으로 가지 못하고 여기서 멈춘다.
    const QString probe = HeritageIntranetFlow::downloadPageProbeScript();
    QVERIFY(probe.contains(QStringLiteral("getElementById('searchForm')")));
    QVERIFY(probe.contains(QStringLiteral("querySelector('#mode')")));
    QVERIFY(probe.contains(QStringLiteral("/^[SPBURE]$/.test(m.value)&&visibleElement(f)")));
    QVERIFY(probe.contains(QStringLiteral("downloadForm(document)||agreementBox(document)")));
    QVERIFY(!probe.contains(QStringLiteral("전체다운로드")));
    QVERIFY(!probe.contains(QStringLiteral("시도선택")));
    QVERIFY(probe.contains(QStringLiteral("서약서에동의")));
    QVERIFY(probe.contains(QStringLiteral("not-yet")));
    QVERIFY(probe.contains(QStringLiteral("page-error")));
  }

  void wrapPrefersTheDownloadFormFrameOverMenuText() {
    // 먼저 실제 다운로드 폼을 고른다. 부모의 메뉴 글자만 보고 지도 창에 머물지 않는다.
    const QString js = HeritageIntranetFlow::searchScript();
    const int formAt = js.indexOf(QStringLiteral("if(hasCodedeta(c[i]))return c[i]"));
    const int menuAt = js.indexOf(QStringLiteral("t.indexOf('지정유산')"));
    QVERIFY(formAt >= 0);
    QVERIFY(menuAt >= 0);
    QVERIFY2(formAt < menuAt, "pick must prefer the download form over parent menu text");
    QVERIFY(js.contains(QStringLiteral("downloadForm(w.document)")));
    QVERIFY(js.contains(QStringLiteral("frames[i]")));
  }

  void formHintMarksTheDownloadFormNotTheMap() {
    const QString js = HeritageIntranetFlow::formHintScript();
    QVERIFY(js.contains(QStringLiteral("codedeta")));
    QVERIFY(js.contains(QStringLiteral("'codedeta'")));
    QVERIFY(!js.contains(QStringLiteral("지정유산")));
    QVERIFY(!js.contains(QStringLiteral("bjdcd")));
    // 공통 도우미에는 서약서 문구가 있지만 반환 판정은 다운로드 폼만 사용한다.
    QVERIFY(js.contains(QStringLiteral("if(downloadForm(document))return 'codedeta'")));
    QVERIFY(js.contains(QStringLiteral("return 'other'")));
    QVERIFY(!js.contains(QStringLiteral("agreementBox(document)")));
  }

  void agreeTermsSkipsWhenDownloadFormIsAlreadyOpen() {
    const QString js = HeritageIntranetFlow::agreeTermsScript();
    QVERIFY(js.contains(QStringLiteral("already-open")));
    QVERIFY(js.contains(QStringLiteral("if(downloadForm(document))return 'already-open'")));
  }

  void searchAndDownloadUseOnlyTheDownloadPanelButtons() {
    // 부모 메뉴의 searchGisChaRirList 는 다른 document 의 것이다.
    // fn() 으로 아무 창의 함수나 잡으면 지도 쪽이 검색돼 조건이 안 걸린다.
    const QString search = HeritageIntranetFlow::searchScript();
    const QString down = HeritageIntranetFlow::downloadAllScript();
    QVERIFY(!search.contains(QStringLiteral("fn('searchGisChaRirList')")));
    QVERIFY(!down.contains(QStringLiteral("fn('searchGisChaRirList')")));
    // 다운로드 폼의 검색과 전체다운로드 버튼에 연결된 사이트 동작을 사용한다.
    QVERIFY(search.contains(QStringLiteral("cands()")));
    QVERIFY(!search.contains(QStringLiteral("searchGisChaRirList")));
    QVERIFY(!down.contains(QStringLiteral("searchGisChaRirList")));
    QVERIFY(search.contains(QStringLiteral("scope?findSearch(scope):null")));
    QVERIFY(search.contains(QStringLiteral("btn.click()")));
    QVERIFY(down.contains(QStringLiteral("form.closest('#tabContentDiv')")));
    QVERIFY(down.contains(QStringLiteral("if(!sido||!sido.value)return 'no-region'")));
    QVERIFY(down.contains(QStringLiteral("if(!sgg||!sgg.value)return 'no-region'")));
    QVERIFY(down.contains(QStringLiteral("if(hits.length!==1)return 'ambiguous'")));
    QVERIFY(down.contains(QStringLiteral("hits[0].click()")));
    // 검색은 페이지를 옮기지 않는다.
    QVERIFY(!search.contains(QStringLiteral("form.submit")));
    QVERIFY(search.contains(QStringLiteral("cands()")));
    QVERIFY(!search.contains(QStringLiteral("new Function")));
    QVERIFY(!down.contains(QStringLiteral("new Function")));
  }


  void frameInventoryReadsIframeTagsWithoutEnteringThem() {
    const QString js = HeritageIntranetFlow::frameInventoryScript();
    QVERIFY(js.contains(QStringLiteral("iframe,frame")));
    QVERIFY(js.contains(QStringLiteral("jsFrames")));
    QVERIFY(js.contains(QStringLiteral("htmlFrames")));
    QVERIFY(!js.contains(QStringLiteral("codedeta")));
  }

  void wrapWalksNestedFrames() {
    const QString js = HeritageIntranetFlow::pageOutlineScript();
    QVERIFY(js.contains(QStringLiteral("codedeta")));
    QVERIFY(js.contains(QStringLiteral("walk(")) || js.contains(QStringLiteral("depth")));
  }

  void resultCollectionKnowsThereCanBeManyPages() {
    const QString js = HeritageIntranetFlow::resultPageInfoScript();
    QVERIFY(js.contains(QStringLiteral("lastPage")));
    QVERIFY(js.contains(QStringLiteral("rows")));
  }

  void portalAndLoginUrlsAreTheRealSite() {
    QCOMPARE(HeritageIntranetFlow::portalUrl().host(),
             QStringLiteral("intranet.gis-heritage.go.kr"));
    // 루트는 frameset(top/main) 이라 프레임을 지정해야 한다. 이 주소로 바로 들어가면 프레임이 없다.
    QCOMPARE(HeritageIntranetFlow::loginUrl().path(), QStringLiteral("/user/checkNet.do"));
    QCOMPARE(HeritageIntranetFlow::loginUrl().scheme(), QStringLiteral("https"));
  }

  void credentialsStoreIsSeparateFromTheTopographicOne() {
    // 수치지형도(NGII) 계정과 같은 자리에 쓰면 둘 다 깨진다.
    HeritageIntranetSettings::Credentials written{QStringLiteral("TESTID"),
                                                  QStringLiteral("testpw\\'\"")};
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = dir.filePath(QStringLiteral("heritage-account.ini"));
    QString error;
    QVERIFY2(HeritageIntranetSettings::saveToFile(file, written, &error), qPrintable(error));
    const auto read = HeritageIntranetSettings::readFromFiles(file, {});
    QCOMPARE(read.username, written.username);
    QCOMPARE(read.password, written.password);

    // 파일 안의 구획 이름이 ngii 가 아니라 heritage 여야 한다.
    QFile f(file);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QString text = QString::fromUtf8(f.readAll());
    QVERIFY(text.contains(QStringLiteral("[heritage]")));
    QVERIFY(!text.contains(QStringLiteral("[ngii]")));
    QVERIFY(text.contains(QStringLiteral("password_dpapi=")));
    QVERIFY(!text.contains(QStringLiteral("\npassword=")));
    QVERIFY(!text.contains(QStringLiteral("password_portable=")));
  }

  void logDescriptionNeverCarriesThePassword() {
    // 계정 값을 로그에 남기지 않는다. 이 PC에 실제로 저장된 비밀번호로 확인한다.
    const auto stored = HeritageIntranetSettings::credentials();
    const QString line = HeritageIntranetSettings::describeForLog();
    QVERIFY(!line.isEmpty());
    if (!stored.password.isEmpty())
      QVERIFY2(!line.contains(stored.password), "로그 문구에 비밀번호가 들어갔다");
    if (!stored.username.trimmed().isEmpty())
      QVERIFY(line.contains(stored.username.trimmed()));
  }
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  HeritageIntranetFlowTest test;
  return QTest::qExec(&test, argc, argv);
}
#include "test_heritage_flow.moc"
