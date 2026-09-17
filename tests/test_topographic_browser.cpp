#include <QtTest>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QJsonObject>
#include <QJsonDocument>
#include <QLineEdit>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPointer>
#include <QPixmap>
#include <QPushButton>
#include <QSignalSpy>
#include <QSettings>
#include <QScreen>
#include <QScrollArea>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QWebEngineDownloadRequest>
#include <QWebEngineLoadingInfo>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>
#include <QUrlQuery>
#include <functional>
#include <memory>
#include "app/KaTopographicBrowser.h"

class LocalDownloadServer : public QTcpServer {
public:
  QByteArray portalFixture;
  QByteArray downloadFixture;
  QByteArray searchFixture;
  QByteArray loginFixture;
  QByteArray anyIdFixture;
  bool loggedIn = true;
  bool acceptLogin = false;
  bool holdLoginResponse = false;
  bool failPortalConnection = false;
  bool redirectTransfer = false;
  int searchCount = 0;
  int loginCheckCount = 0;
  int loginSubmissionCount = 0;
  bool streamFixtureArchives = false;
  bool fixtureArchiveFinished = false;
  QPointer<QTcpSocket> pendingXmlSocket;
  QByteArray pendingXmlName;
  QPointer<QTcpSocket> transferSocket;
  QPointer<QTcpSocket> pendingLoginScript;
  bool transferChunked = false;
  int interruptedRequests = 0;
  bool holdSdkTransfers = false;
  QStringList sdkRequests;
  QString activeSdkFile;
  QPointer<QTcpSocket> sdkXmlSocket;
  QString sdkXmlName;
  std::function<void(const QString&)> sdkRequestObserved;
  static constexpr qint64 transferSize = 4 * 1024 * 1024;
  static constexpr qint64 firstChunkSize = 1024 * 1024;

  void startTransfer(QTcpSocket* socket, const QByteArray& fileName, bool chunked = false) {
    transferSocket = socket;
    transferChunked = chunked;
    const QByteArray length = chunked ? QByteArray("Transfer-Encoding: chunked\r\n")
      : QByteArray("Content-Length: ") + QByteArray::number(transferSize) + "\r\n";
    socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/zip\r\nContent-Disposition: attachment; filename="
      + fileName + "\r\n" + length + "\r\n");
    QByteArray first(firstChunkSize, 'x');first.replace(0, 2, "PK");
    if(chunked)socket->write(QByteArray::number(first.size(),16)+"\r\n"+first+"\r\n");
    else socket->write(first);
  }
  bool finishTransfer() {
    if(!transferSocket)return false;
    const QByteArray rest(transferSize-firstChunkSize,'x');
    if(transferChunked)transferSocket->write(QByteArray::number(rest.size(),16)+"\r\n"+rest+"\r\n0\r\n\r\n");
    else transferSocket->write(rest);
    transferSocket->disconnectFromHost();
    activeSdkFile.clear();
    fixtureArchiveFinished=true;
    if(pendingXmlSocket) {
      const QByteArray body("fixture metadata");
      pendingXmlSocket->write("HTTP/1.1 200 OK\r\nContent-Type: application/xml\r\nContent-Disposition: attachment; filename="
        +pendingXmlName+"\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);
      pendingXmlSocket->disconnectFromHost();
    }
    return true;
  }
  bool finishSdkXml() {
    if(!sdkXmlSocket)return false;
    const QByteArray body("fixture metadata");
    sdkXmlSocket->write("HTTP/1.1 200 OK\r\nContent-Type: application/xml\r\nContent-Disposition: attachment; filename="+
      sdkXmlName.toUtf8()+"\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);
    sdkXmlSocket->disconnectFromHost(); sdkXmlSocket.clear(); return true;
  }
  LocalDownloadServer() {
    connect(this, &QTcpServer::newConnection, this, [this] {
      while (auto* socket = nextPendingConnection()) {
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
          QByteArray input = socket->property("requestBytes").toByteArray() + socket->readAll();
          socket->setProperty("requestBytes", input);
          if (!input.contains("\r\n\r\n") || socket->property("answered").toBool()) return;
          socket->setProperty("answered", true);
          const QByteArray target = input.split(' ').value(1);
          if (target == "/slow.zip") {
            socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/zip\r\nContent-Disposition: attachment; filename=slow.zip\r\nContent-Length: 1000000\r\n\r\nPKpartial");
            return; // Deliberately unfinished transfer; cancellation must never emit import.
          }
          if(target=="/controlled.zip" || target=="/chunked.zip") {
            startTransfer(socket,target.mid(1),target=="/chunked.zip");return;
          }
          if(target=="/interrupted.zip") {
            if(++interruptedRequests==1)startTransfer(socket,"interrupted.zip");
            else {socket->write("HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");socket->disconnectFromHost();}
            return;
          }
          QByteArray body;
          QByteArray contentType("text/html; charset=utf-8");
          QByteArray extra;
          if (target.startsWith("/ms/map/NlipMap.do")) {
            if(failPortalConnection){socket->abort();return;}
            body = portalFixture;
          } else if (target.startsWith("/ms/map/combineBufferQuery.do")) {
            body=R"JSON({"items":[{"wkt":"POLYGON ((1120000 1975000,1130000 1975000,1130000 1985000,1120000 1985000,1120000 1975000))","minx":1120000,"miny":1975000,"maxx":1130000,"maxy":1985000}]})JSON";
            contentType="application/json";
          } else if (target.startsWith("/ms/map/suchiQuery.do")) {
            ++searchCount; body = searchFixture; contentType = "application/json";
          } else if (target == "/mn/loginCheck.do") {
            ++loginCheckCount;
            body = loggedIn?"{\"isLogin\":true}":"{\"isLogin\":false}"; contentType = "application/json";
          } else if (target.startsWith("/member/login.do")) {
            body = loginFixture;
          } else if (target == "/held-login-script.js") {
            pendingLoginScript=socket;return;
          } else if (target == "/anyid/common/anyidLogin.do") {
            body = anyIdFixture;
          } else if (target == "/member/process.do") {
            ++loginSubmissionCount; body="<html>Fixture login response</html>";
            if(holdLoginResponse)return;
            if(acceptLogin){loggedIn=true;body="<html><script>parent.close();</script></html>";}
          } else if (target == "/pd/purchs/download.do") {
            body = redirectTransfer?QByteArray("<html><script>location.replace('/nlippd/pd/innorix/innorixdownLoad.do');</script></html>"):downloadFixture;
          } else if (target == "/nlippd/pd/innorix/innorixdownLoad.do") {
            body = downloadFixture;
          } else if (target.startsWith("/fixture-") && (target.endsWith(".zip") || target.endsWith(".xml"))) {
            if(streamFixtureArchives && target.endsWith(".zip")){startTransfer(socket,"terrain_"+target.mid(9));return;}
            if(streamFixtureArchives && target.endsWith(".xml") && !fixtureArchiveFinished){pendingXmlSocket=socket;pendingXmlName="terrain_"+target.mid(9);return;}
            body = "PK-public-sheet-fixture"; contentType = "application/zip";
            extra = "Content-Disposition: attachment; filename=terrain_"+target.mid(9)+"\r\n";
          } else if (target.startsWith("/nlippd/pd/innorix/innorixDownLoad2.do")) {
            const auto name=QUrlQuery(QUrl::fromEncoded(target)).queryItemValue(QStringLiteral("fileName"));
            sdkRequests.append(name); if(sdkRequestObserved)sdkRequestObserved(name);
            if(holdSdkTransfers && name.endsWith(QStringLiteral(".dxf"))) {
              activeSdkFile=name;startTransfer(socket,name.toUtf8());return;
            }
            if(holdSdkTransfers && name.endsWith(QStringLiteral(".xml"))) {
              sdkXmlSocket=socket;sdkXmlName=name;return;
            }
            body=name.endsWith(QStringLiteral(".dxf"))?QByteArray(transferSize,'x'):
              name.endsWith(QStringLiteral(".xml"))?QByteArray("fixture metadata"):QByteArray("PK-fixture-complete");
            contentType="application/octet-stream";
            extra="Content-Disposition: attachment; filename="+name.toUtf8()+"\r\n";
          } else if (target == "/file.zip") {
            body = "PK-fixture-complete";
            contentType = "application/zip";
            extra = "Content-Disposition: attachment; filename=terrain.zip\r\n";
          } else if (target == "/popup") {
            body = "<html><head><title>popup</title></head><body>popup</body></html>";
          } else {
            body = "<html><head><title>local fixture</title></head><body>fixture</body></html>";
            if(target=="/")extra = "Set-Cookie: fixture=shared; Path=/; SameSite=Lax\r\n";
          }
          socket->write("HTTP/1.1 200 OK\r\nContent-Type: " + contentType + "\r\n" + extra +
                        "Content-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
          socket->disconnectFromHost();
        });
      }
    });
  }
  QUrl url(const QString& path = QStringLiteral("/")) const {
    return QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(serverPort()).arg(path));
  }
};

static QByteArray batchSdkFixture() {
  return R"HTML(<html><body><div id="fileControl"></div><script>
    const records=opener.orderItems.flatMap(r=>['dxf','xml'].map(ext=>({id:r.mapNo+'-'+ext,
      printFileName:'terrain_'+r.mapNo+'.'+ext,fileSize:ext==='dxf'?4194304:16,
      downloadUrl:location.origin+'/nlippd/pd/innorix/innorixDownLoad2.do?fileName=terrain_'+r.mapNo+'.'+ext+'&directory=fixture'})));
    window.control={getOption:()=>({agent:false}),getDownloadFiles:()=>records,download(id){
      window.downloadIds=(window.downloadIds||[]).concat(id===undefined?'ALL':id);
      const selected=id===undefined?records:records.filter(f=>f.id===id);if(!selected.length)return false;
      selected.forEach((f,i)=>setTimeout(()=>{const a=document.createElement('a');a.href=f.downloadUrl;a.target='_self';document.body.appendChild(a);a.click();a.remove();},i*200));
    }};
  </script></body></html>)HTML";
}
static QByteArray batchPortalFixture() {
  return R"HTML(<html><body><main class="map total"><div id="totalSearch"><section class="mapList measure"><ul class="_accordion"><li><dl><dt><button value="0">DXF</button><select><option value="25000">25000</option></select></dt><dd class="items"></dd></dl></li></ul></section></div></main><script>
    window.map={getView:()=>({getProjection:()=>({getCode:()=> 'EPSG:5179'}),setCenter(){},fit(){}})};
    window.mapQuery={clear(){}};window.integ={};window.fixtureEvents={on(n,f){window.fixtureOrderComplete=f;},off(){}};window.jQuery=x=>x===document?fixtureEvents:x;
    window.MEASURE_API={downArray:[],completed(){const box=document.querySelector('.items');box.innerHTML='';for(const num of ['378044','378043']){
      const c=document.createElement('input');c.type='checkbox';for(const [k,v] of Object.entries({name:'cb_suchi',mapType:'0',scale:'25000',num,nam:'강릉',hisno:'0',fileext:'dxf'}))c.setAttribute(k,v);
      box.appendChild(c);c.onchange=()=>{MEASURE_API.downArray=Array.from(box.querySelectorAll('input:checked')).map(e=>({orderTp:'0',mapScale:'25000',mapNo:e.getAttribute('num'),mapName:e.getAttribute('nam'),mapHistoryNo:'0',mapKind:'0',ext:'dxf'}));};}}};
    window.downloadMapData=(type,gb,items)=>{window.orderCalls=(window.orderCalls||0)+1;window.orderItems=items;
      const f=document.createElement('div');f.className='popup mapApplication online';f.innerHTML='<input name="onBrthdy"><select id="onPurchsPurps"><option value="BAAAC00008">기타</option></select><input name="onDetailCn"><input id="agree" type="radio" name="onChkAgree"><button id="submitOrder">다운로드</button>';document.body.appendChild(f);
      document.getElementById('submitOrder').onclick=()=>{window.submitCalls=(window.submitCalls||0)+1;window.open('/pd/purchs/download.do','downForm');fixtureOrderComplete(null,{status:200,responseJSON:{resultCode:'1',allDataMap:{orderDownList:'fixture'}}},{url:'/pd/shbtManage/shbtInsertNRM.do'});};};
    window.checkValidation=()=>document.getElementById('agree').checked;
  </script></body></html>)HTML";
}

static bool saveCompactCapture(QWidget& widget, const QString& name) {
  const QString directory=qEnvironmentVariable("KA_HGIS_COMPACT_CAPTURE");
  return directory.isEmpty() || (QDir().mkpath(directory) && widget.grab().save(directory+QLatin1Char('/')+name));
}

class TopographicBrowserTest : public QObject {
  Q_OBJECT
private slots:
  void initTestCase() {
#ifdef Q_OS_WIN
    const int fontId=QFontDatabase::addApplicationFont(QDir(qEnvironmentVariable("WINDIR")).filePath(QStringLiteral("Fonts/malgun.ttf")));
    QVERIFY(fontId>=0);
    const auto families=QFontDatabase::applicationFontFamilies(fontId);QVERIFY(!families.isEmpty());
    QApplication::setFont(QFont(families.first(),9));
#endif
  }
  void compactDownloadKeepsProgressAndOffersExplicitDetails() {
    LocalDownloadServer server;QVERIFY(server.listen(QHostAddress::LocalHost));QTemporaryDir files;
    KaTopographicBrowser browser(nullptr,files.path());browser.setPortalTestUrl(server.url(QStringLiteral("/ms/map/NlipMap.do")));browser.navigate(server.url());
    browser.setCompactMode(true);browser.show();
    auto* details=browser.findChild<QWidget*>(QStringLiteral("topographicOfficialDetails"));
    auto* expand=browser.findChild<QPushButton*>(QStringLiteral("topographicExpandOfficial"));
    QVERIFY(details);QVERIFY(expand);QVERIFY(details->isHidden());QVERIFY(expand->isVisible());
    QCOMPARE(expand->text(),QStringLiteral("상세 보기"));
    QVERIFY(!browser.findChild<QTreeWidget*>(QStringLiteral("topographicDownloads"))->isVisible());
    auto* status=browser.findChild<QLabel*>(QStringLiteral("topographicCompactStatus"));
    auto* stage=browser.findChild<QLabel*>(QStringLiteral("topographicCurrentStage"));
    auto* summary=browser.findChild<QLabel*>(QStringLiteral("topographicTransferSummary"));
    auto* progress=browser.findChild<QProgressBar*>(QStringLiteral("topographicProgress"));
    QVERIFY(status);QVERIFY(stage);QVERIFY(summary);QVERIFY(progress);
    QVERIFY(stage->isVisible());QVERIFY(!summary->isVisible());QVERIFY(progress->isTextVisible());
    QTest::mouseClick(expand,Qt::LeftButton);
    QVERIFY(details->isVisible());QVERIFY(summary->isVisible());
    QVERIFY(stage->isVisible());QVERIFY(progress->isVisible());
    QCOMPARE(expand->text(),QStringLiteral("상세 접기"));
    QTest::mouseClick(expand,Qt::LeftButton);
    QVERIFY(details->isHidden());QVERIFY(!summary->isVisible());
    QVERIFY(stage->isVisible());QVERIFY(progress->isVisible());
    QCOMPARE(expand->text(),QStringLiteral("상세 보기"));
    browser.setActivityStatus(QStringLiteral("로그인 · 신청서 · 동의 · DXF 다운로드 중"));
    QVERIFY(!status->text().contains(QStringLiteral("로그인")));
    QCOMPARE(progress->minimum(),0);QCOMPARE(progress->maximum(),1);QCOMPARE(progress->value(),0);
    QCOMPARE(progress->format(),QStringLiteral("진행률 확인 중"));
    browser.setCredentials(QStringLiteral("fixture-local-account"),QStringLiteral("fixture-local-password"));
    QCOMPARE(browser.findChild<QLineEdit*>(QStringLiteral("topographicLoginId"))->text(),QStringLiteral("fixture-local-account"));
    QVERIFY(details->isHidden());
    browser.setProcessing(true);browser.navigate(server.url());
    auto* view=browser.findChild<QWebEngineView*>();QVERIFY(view);
    QTRY_COMPARE_WITH_TIMEOUT(view->title(),QStringLiteral("local fixture"),10000);
    // A loaded web page is not a completed map download.
    QCOMPARE(progress->maximum(),1);QCOMPARE(progress->value(),0);QVERIFY(details->isHidden());
    const QString capture=qEnvironmentVariable("KA_HGIS_COMPACT_CAPTURE");
    if(!capture.isEmpty()) {QDir().mkpath(capture);QVERIFY(browser.grab().save(capture+QStringLiteral("/compact-download.png")));}
    browser.setActivityAttention(QStringLiteral("좌표를 확인하지 못했습니다. 자료 확인이 필요합니다."));
    QVERIFY(status->text().contains(QStringLiteral("좌표")));QVERIFY(stage->text().contains(QStringLiteral("확인")));
    QVERIFY(!stage->text().contains(QStringLiteral("실패")));QVERIFY(progress->value()<progress->maximum());
    browser.prepareSheets(1125000,1980000,5000,{QStringLiteral("378044")});
    QVERIFY(status->text().contains(QStringLiteral("좌표")));
    QVERIFY(details->isHidden());QVERIFY(expand->isVisible());
    browser.setProcessing(false);
    QSignalSpy cancelled(&browser,&KaTopographicBrowser::cancelRequested);
    browser.stopAutomatic();QVERIFY(details->isHidden());QVERIFY(cancelled.isEmpty());
    QTest::keyClick(&browser,Qt::Key_Escape);QCOMPARE(cancelled.size(),1);QVERIFY(!browser.isVisible());
  }
  void compactPopupAndExternalTransferNeverExposeSite() {
    LocalDownloadServer server;QVERIFY(server.listen(QHostAddress::LocalHost));QTemporaryDir files;
    KaTopographicBrowser browser(nullptr,files.path());browser.setCompactMode(true);
    browser.navigate(server.url());browser.show();
    auto* tabs=browser.findChild<QTabWidget*>(QStringLiteral("topographicTabs"));
    auto* view=qobject_cast<QWebEngineView*>(tabs->widget(0));QVERIFY(view);
    QTRY_COMPARE_WITH_TIMEOUT(view->title(),QStringLiteral("local fixture"),10000);
    view->page()->runJavaScript(QStringLiteral("window.open('/popup','hidden-download');"));
    QTRY_COMPARE(tabs->count(),2);
    QVERIFY(!tabs->isVisible());QVERIFY(!tabs->widget(1)->isVisible());
    auto* popup=qobject_cast<QWebEngineView*>(tabs->widget(1));QVERIFY(popup);
    QTRY_COMPARE(popup->title(),QStringLiteral("popup"));
    popup->page()->settings()->setUnknownUrlSchemePolicy(QWebEngineSettings::AllowAllUnknownUrlSchemes);
    popup->page()->load(QUrl(QStringLiteral("innorix-test://fixture")));
    auto* status=browser.findChild<QLabel*>(QStringLiteral("topographicCompactStatus"));
    QTRY_VERIFY(status->text().contains(QStringLiteral("전송 프로그램")));
    QVERIFY(browser.findChildren<QMessageBox*>().isEmpty());
    QVERIFY(!tabs->isVisible());
    auto promptResult=std::make_shared<QString>();
    view->page()->runJavaScript(QStringLiteral("alert('fixture-private-message');String(confirm('unexpected-action'))"),
      [promptResult](const QVariant& value){*promptResult=value.toString();});
    QTRY_COMPARE(*promptResult,QStringLiteral("false"));
    QVERIFY(status->text().contains(QStringLiteral("추가 확인")));
    QVERIFY(!status->text().contains(QStringLiteral("fixture-private-message")));
    QVERIFY(browser.findChildren<QMessageBox*>().isEmpty());
  }
  void compactTransferUsesReceivedBytesAndPreparationNeverCompletesEarly() {
    LocalDownloadServer server;QVERIFY(server.listen(QHostAddress::LocalHost));QTemporaryDir files;
    KaTopographicBrowser browser(nullptr,files.path());browser.setCompactMode(true);browser.navigate(server.url());browser.show();
    auto* view=browser.findChild<QWebEngineView*>();QVERIFY(view);
    QTRY_COMPARE_WITH_TIMEOUT(view->title(),QStringLiteral("local fixture"),10000);
    auto* stage=browser.findChild<QLabel*>(QStringLiteral("topographicCurrentStage"));QVERIFY(stage);
    auto* detail=browser.findChild<QLabel*>(QStringLiteral("topographicCompactStatus"));QVERIFY(detail);
    auto* summary=browser.findChild<QLabel*>(QStringLiteral("topographicTransferSummary"));QVERIFY(summary);
    auto* progress=browser.findChild<QProgressBar*>(QStringLiteral("topographicProgress"));QVERIFY(progress);
    QPointer<QWebEngineDownloadRequest> request;
    connect(view->page()->profile(),&QWebEngineProfile::downloadRequested,&browser,
      [&request](QWebEngineDownloadRequest* download){request=download;});
    QSignalSpy received(&browser,&KaTopographicBrowser::fileDownloaded);
    view->page()->download(server.url(QStringLiteral("/controlled.zip")));
    QTRY_VERIFY_WITH_TIMEOUT(request && request->receivedBytes()==LocalDownloadServer::firstChunkSize,10000);
    QCOMPARE(request->totalBytes(),LocalDownloadServer::transferSize);
    QVERIFY(stage->text().endsWith(QStringLiteral("다운로드")));QCOMPARE(progress->minimum(),0);QCOMPARE(progress->maximum(),100);
    QCOMPARE(progress->value(),25);QVERIFY(progress->isTextVisible());
    QVERIFY(summary->text().contains(QStringLiteral("controlled.zip")));
    QVERIFY(summary->text().contains(QStringLiteral("MB")) || summary->text().contains(QStringLiteral("MiB")));
    QVERIFY(summary->text().contains(QLatin1Char('/')));
    QVERIFY(received.isEmpty());
    const int stalledValue=progress->value();const QString stalledSummary=summary->text();
    QTest::qWait(1300); // Several progress/UI ticks without one more received byte.
    QCOMPARE(request->receivedBytes(),LocalDownloadServer::firstChunkSize);
    QCOMPARE(progress->value(),stalledValue);QCOMPARE(summary->text(),stalledSummary);
    QVERIFY(saveCompactCapture(browser,QStringLiteral("download-progress.png")));
    browser.setProcessing(true);
    QVERIFY(server.finishTransfer());QTRY_COMPARE_WITH_TIMEOUT(received.size(),1,10000);
    browser.setPreparationProgress(QStringLiteral("지도에 올리기"),QStringLiteral("SHP 변환 · 강릉 378044 · 선과 면을 지도 레이어로 준비하고 있습니다."));
    QVERIFY(stage->text().endsWith(QStringLiteral("지도에 올리기")));QVERIFY(detail->text().contains(QStringLiteral("378044")));
    QCOMPARE(progress->minimum(),0);QCOMPARE(progress->maximum(),1);QCOMPARE(progress->value(),0);
    QCOMPARE(progress->format(),QStringLiteral("진행률 확인 중"));
    QTest::qWait(600);QVERIFY(browser.isVisible());
    QVERIFY(saveCompactCapture(browser,QStringLiteral("map-preparation.png")));
    QVERIFY(browser.findChild<QWidget*>(QStringLiteral("topographicOfficialDetails"))->isHidden());
    browser.setProcessing(false);
  }
  void compactUnknownLengthStaysIndeterminateAndCancelKeepsSnapshot() {
    LocalDownloadServer server;QVERIFY(server.listen(QHostAddress::LocalHost));QTemporaryDir files;
    KaTopographicBrowser browser(nullptr,files.path());browser.setCompactMode(true);browser.navigate(server.url());browser.show();
    auto* view=browser.findChild<QWebEngineView*>();QVERIFY(view);
    QTRY_COMPARE_WITH_TIMEOUT(view->title(),QStringLiteral("local fixture"),10000);
    QPointer<QWebEngineDownloadRequest> request;
    connect(view->page()->profile(),&QWebEngineProfile::downloadRequested,&browser,
      [&request](QWebEngineDownloadRequest* download){request=download;});
    QSignalSpy received(&browser,&KaTopographicBrowser::fileDownloaded);
    view->page()->download(server.url(QStringLiteral("/chunked.zip")));
    QTRY_VERIFY_WITH_TIMEOUT(request && request->receivedBytes()==LocalDownloadServer::firstChunkSize,10000);
    QVERIFY(request->totalBytes()<=0);
    auto* progress=browser.findChild<QProgressBar*>(QStringLiteral("topographicProgress"));QVERIFY(progress);
    auto* summary=browser.findChild<QLabel*>(QStringLiteral("topographicTransferSummary"));QVERIFY(summary);
    auto* stage=browser.findChild<QLabel*>(QStringLiteral("topographicCurrentStage"));QVERIFY(stage);
    QCOMPARE(progress->minimum(),0);QCOMPARE(progress->maximum(),1);QCOMPARE(progress->value(),0);
    QCOMPARE(progress->format(),QStringLiteral("진행률 확인 중"));QVERIFY(stage->text().endsWith(QStringLiteral("다운로드")));
    QVERIFY(summary->text().contains(QStringLiteral("chunked.zip")));
    QVERIFY(summary->text().contains(QStringLiteral("MB")) || summary->text().contains(QStringLiteral("MiB")));
    const QString beforeCancel=summary->text();
    auto* rows=browser.findChild<QTreeWidget*>(QStringLiteral("topographicDownloads"));QVERIFY(rows);
    QCOMPARE(rows->topLevelItemCount(),1);
    auto* cancel=qobject_cast<QPushButton*>(rows->itemWidget(rows->topLevelItem(0),3));QVERIFY(cancel);cancel->click();
    QTRY_COMPARE(request->state(),QWebEngineDownloadRequest::DownloadCancelled);
    QCOMPARE(rows->topLevelItem(0)->text(2),QStringLiteral("취소됨"));QVERIFY(received.isEmpty());
    QVERIFY(progress->value()<progress->maximum());QCOMPARE(summary->text(),beforeCancel);
    QVERIFY(browser.isVisible());QVERIFY(saveCompactCapture(browser,QStringLiteral("download-cancelled.png")));
  }
  void compactInterruptedHttpTransferPreservesFailureAndReceivedBytes() {
    LocalDownloadServer server;QVERIFY(server.listen(QHostAddress::LocalHost));QTemporaryDir files;
    KaTopographicBrowser browser(nullptr,files.path());browser.setCompactMode(true);browser.navigate(server.url());browser.show();
    auto* view=browser.findChild<QWebEngineView*>();QVERIFY(view);
    QTRY_COMPARE_WITH_TIMEOUT(view->title(),QStringLiteral("local fixture"),10000);
    QPointer<QWebEngineDownloadRequest> request;
    connect(view->page()->profile(),&QWebEngineProfile::downloadRequested,&browser,
      [&request](QWebEngineDownloadRequest* download){request=download;});
    QSignalSpy received(&browser,&KaTopographicBrowser::fileDownloaded);
    view->page()->download(server.url(QStringLiteral("/interrupted.zip")));
    QTRY_VERIFY_WITH_TIMEOUT(request && request->receivedBytes()==LocalDownloadServer::firstChunkSize,10000);
    auto* stage=browser.findChild<QLabel*>(QStringLiteral("topographicCurrentStage"));QVERIFY(stage);
    auto* detail=browser.findChild<QLabel*>(QStringLiteral("topographicCompactStatus"));QVERIFY(detail);
    auto* summary=browser.findChild<QLabel*>(QStringLiteral("topographicTransferSummary"));QVERIFY(summary);
    auto* progress=browser.findChild<QProgressBar*>(QStringLiteral("topographicProgress"));QVERIFY(progress);
    const QString beforeFailure=summary->text();
    QVERIFY(server.transferSocket);server.transferSocket->abort();
    QTRY_COMPARE_WITH_TIMEOUT(request->state(),QWebEngineDownloadRequest::DownloadInterrupted,15000);
    QCOMPARE(stage->text(),QStringLiteral("다운로드 실패 · 다운로드"));
    QVERIFY(!detail->text().isEmpty());QVERIFY(progress->value()<progress->maximum());QCOMPARE(summary->text(),beforeFailure);
    QVERIFY(received.isEmpty());
    const QString error=detail->text();QTest::qWait(1300);
    QVERIFY(browser.isVisible());QCOMPARE(stage->text(),QStringLiteral("다운로드 실패 · 다운로드"));
    QCOMPARE(detail->text(),error);QCOMPARE(summary->text(),beforeFailure);QVERIFY(progress->value()<progress->maximum());
    QVERIFY(saveCompactCapture(browser,QStringLiteral("download-failed.png")));
    QVERIFY(browser.findChild<QWidget*>(QStringLiteral("topographicOfficialDetails"))->isHidden());
  }
  void changedAccountReplacesSessionAndSameAccountKeepsIt() {
    LocalDownloadServer server;QVERIFY(server.listen(QHostAddress::LocalHost));QTemporaryDir files;
    KaTopographicBrowser browser(nullptr,files.path());
    browser.setPortalTestUrl(server.url(QStringLiteral("/ms/map/NlipMap.do")));
    browser.setCredentials(QStringLiteral("fixture-account-one"),QStringLiteral("fixture-password-one"));
    auto* tabs=browser.findChild<QTabWidget*>(QStringLiteral("topographicTabs"));
    auto* first=qobject_cast<QWebEngineView*>(tabs->widget(0));QVERIFY(first);
    QPointer<QWebEngineProfile> firstProfile(first->page()->profile());
    QPointer<QWebEnginePage> firstPage(first->page());
    QSignalSpy loaded(first,&QWebEngineView::loadFinished);browser.navigate(server.url());
    QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(),10000);
    auto cookie=std::make_shared<QString>();
    first->page()->runJavaScript(QStringLiteral("document.cookie"),[cookie](const QVariant& v){*cookie=v.toString();});
    QTRY_VERIFY(cookie->contains(QStringLiteral("fixture=shared")));
    browser.setCredentials(QStringLiteral("fixture-account-one"),QStringLiteral("fixture-password-one"));
    QCOMPARE(qobject_cast<QWebEngineView*>(tabs->widget(0))->page()->profile(),firstProfile.data());
    QSignalSpy received(&browser,&KaTopographicBrowser::fileDownloaded);
    first->page()->runJavaScript(QStringLiteral("location.href='/slow.zip';"));
    auto* rows=browser.findChild<QTreeWidget*>(QStringLiteral("topographicDownloads"));
    QTRY_COMPARE_WITH_TIMEOUT(rows->topLevelItemCount(),1,10000);
    browser.setCredentials(QStringLiteral("fixture-account-two"),QStringLiteral("fixture-password-two"));
    auto* next=qobject_cast<QWebEngineView*>(tabs->widget(0));QVERIFY(next);
    QVERIFY(next->page()->profile()!=firstProfile.data());QVERIFY(next->page()->profile()->isOffTheRecord());
    QTRY_VERIFY(firstProfile.isNull());QVERIFY(firstPage.isNull());QVERIFY(received.isEmpty());
    QCOMPARE(rows->topLevelItem(0)->text(2),QStringLiteral("취소됨"));
    QSignalSpy reloaded(next,&QWebEngineView::loadFinished);browser.navigate(server.url(QStringLiteral("/popup")));
    QTRY_VERIFY_WITH_TIMEOUT(!reloaded.isEmpty(),10000);
    *cookie=QStringLiteral("pending");next->page()->runJavaScript(QStringLiteral("document.cookie"),[cookie](const QVariant& v){*cookie=v.toString();});
    QTRY_COMPARE(*cookie,QString());
    browser.setCompactMode(false);browser.show();
    QVERIFY(browser.size().width()<=browser.screen()->availableGeometry().width());
    QVERIFY(browser.size().height()<=browser.screen()->availableGeometry().height());
    QVERIFY(browser.findChild<QScrollArea*>(QStringLiteral("topographicDetailsScroll"))->widgetResizable());
  }

  void finalListRequiresExactSheetNumber() {
    LocalDownloadServer server;
    server.downloadFixture=R"HTML(<html><body><table><tr><td><input type="checkbox"></td><td>13780449.zip</td><td>1 MB</td></tr></table><button>선택 다운로드</button></body></html>)HTML";
    QVERIFY(server.listen(QHostAddress::LocalHost));QTemporaryDir files;
    KaTopographicBrowser browser(nullptr,files.path());browser.show();
    auto* tabs=browser.findChild<QTabWidget*>(QStringLiteral("topographicTabs"));
    auto* view=qobject_cast<QWebEngineView*>(tabs->widget(0));QSignalSpy loaded(view,&QWebEngineView::loadFinished);
    browser.navigate(server.url(QStringLiteral("/pd/purchs/download.do")));
    QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(),10000);
    QFile script(QStringLiteral("data/providers/ngii-topographic.js"));QVERIFY(script.open(QIODevice::ReadOnly));
    auto state=std::make_shared<QString>();
    view->page()->runJavaScript(QString::fromUtf8(script.readAll())+QStringLiteral("({command:'files',generation:'1',number:'378044'});"),
      [state](const QVariant& value){*state=value.toMap().value(QStringLiteral("state")).toString();});
    QTRY_COMPARE(*state,QStringLiteral("blocked"));
  }
  void automaticSelectionPreservesProductAndSubmitsOnce_data() {
    QTest::addColumn<bool>("applicationSucceeds");
    QTest::addColumn<bool>("retainedModal");
    QTest::addColumn<bool>("reloadBeforeSubmit");
    QTest::addColumn<bool>("compact");
    QTest::addColumn<bool>("loginRequired");
    QTest::addColumn<bool>("unsupportedTransfer");
    QTest::addColumn<bool>("redirectTransfer");
    QTest::newRow("completed-official-order")<<true<<false<<false<<false<<false<<false<<false;
    QTest::newRow("server-rejected-order-stays-before-download")<<false<<false<<false<<false<<false<<false<<false;
    QTest::newRow("two-sheets-after-official-close-retains-hidden-dom-and-clears-selection")<<true<<true<<false<<false<<false<<false<<false;
    QTest::newRow("document-reload-after-form-before-submit")<<true<<false<<true<<false<<false<<false<<false;
    QTest::newRow("compact-hidden-portal-completes-in-survey-folder")<<true<<false<<false<<true<<false<<false<<false;
    QTest::newRow("hidden-login-completion-resumes-one-application-and-download")<<true<<false<<false<<true<<true<<false<<false;
    QTest::newRow("unsupported-final-transfer-stays-visible-at-file-preparation")<<true<<false<<false<<true<<false<<true<<false;
    QTest::newRow("official-final-transfer-redirect-retains-order")<<true<<false<<false<<true<<false<<false<<true;
  }
  void automaticSelectionPreservesProductAndSubmitsOnce() {
    QFETCH(bool,applicationSucceeds);
    QFETCH(bool,retainedModal);
    QFETCH(bool,reloadBeforeSubmit);
    QFETCH(bool,compact);
    QFETCH(bool,loginRequired);
    QFETCH(bool,unsupportedTransfer);
    QFETCH(bool,redirectTransfer);
    LocalDownloadServer server;
    server.redirectTransfer=redirectTransfer;
    server.loggedIn=!loginRequired;server.acceptLogin=loginRequired;
    server.streamFixtureArchives=compact && !unsupportedTransfer;
    server.searchFixture = R"({"items":[{"num":"378044","name":"강릉","scale":"25000","projection":"GRS80","fileExt":"dxf","historyNo":0,"makeYer":"2024","minx":1120928.8,"miny":1973152.5,"maxx":1132145.6,"maxy":1987191.4}]})";
    auto fixtureSheets=QJsonDocument::fromJson(server.searchFixture).object().value(QStringLiteral("items")).toArray();
    if(retainedModal){auto next=fixtureSheets[0].toObject();next.insert(QStringLiteral("num"),QStringLiteral("378043"));fixtureSheets.append(next);}
    server.searchFixture=QJsonDocument(QJsonObject{{QStringLiteral("items"),fixtureSheets}}).toJson(QJsonDocument::Compact);
    server.portalFixture = R"HTML(<html><head><title>automation fixture</title></head><body>
      <main class="map total"><div id="totalSearch"><section class="mapList measure"><ul class="_accordion"><li><dl><dt><button value="0">DXF</button><select><option value="25000">25000</option></select></dt><dd><div class="items"></div></dd></dl></li></ul></section></div></main>
      <script>
      window.map={getView:()=>({getProjection:()=>({getCode:()=> 'EPSG:5179'}),setCenter:x=>window.movedCenter=x,fit:x=>window.movedBounds=x})};
      window.mapQuery={clear(){}, index:[], scale:null}; window.integ={searchForm:0};window.fixtureEvents={on(name,fn){window.fixtureOrderComplete=fn;},off(){}};window.jQuery=x=>x===document?fixtureEvents:x;
      window.MEASURE_API={downArray:[],completed(){let box=document.querySelector('.items');box.innerHTML='';for(const r of fixtureSheets){let c=document.createElement('input');c.type='checkbox';for(const [k,v] of Object.entries({name:'cb_suchi',mapType:'0',scale:'25000',num:r.num,nam:r.name,hisno:'0',fileext:'dxf'}))c.setAttribute(k,v);box.appendChild(c);c.onchange=()=>{MEASURE_API.downArray=Array.from(box.querySelectorAll('input:checked')).map(e=>({orderTp:'0',mapScale:'25000',mapNo:e.getAttribute('num'),mapName:e.getAttribute('nam'),mapHistoryNo:'0',mapKind:'0',ext:'dxf'}));};}}};
      window.fnCloseModal=()=>{document.querySelectorAll('[name=cb_suchi]').forEach(c=>c.checked=false);MEASURE_API.downArray=[];document.querySelectorAll('.mapApplication').forEach(f=>{f.style.opacity='0';f.style.display='none';});};
      window.fnLogin=()=>{window.loginCalls=(window.loginCalls||0)+1;window.open('/member/login.do','id-login');};
      window.downloadMapData=function(type,gb,items){window.orderCalls=(window.orderCalls||0)+1;window.orderItems=items;document.querySelectorAll('.mapApplication').forEach(x=>x.remove());let f=document.createElement('div');f.className='popup mapApplication online';f.innerHTML='<input name="onBrthdy"><select id="onPurchsPurps"><option value=""></option><option value="BAAAC00008">기타</option></select><input name="onDetailCn"><input id="agree" type="radio" name="onChkAgree"><button id="submitOrder">다운로드</button>';document.body.appendChild(f);document.getElementById('submitOrder').onclick=function(){window.submitCalls=(window.submitCalls||0)+1;if(window.fixtureApplicationSucceeds)window.open('/pd/purchs/download.do','downForm');if(window.fixtureRetainedModal)window.open('/pd/purchs/download.do','unrelatedOldDownload');window.fixtureOrderComplete(null,{status:200,responseJSON:{resultCode:window.fixtureApplicationSucceeds?'1':'0',allDataMap:{orderDownList:'fixture'}}},{url:'/pd/shbtManage/shbtInsertNRM.do'});if(window.fixtureRetainedModal)fnCloseModal();};};
      window.checkValidation=()=>!!document.querySelector('[name=onBrthdy]').value&&document.getElementById('agree').checked;
      </script></body></html>)HTML";
    server.portalFixture.replace("<script>",applicationSucceeds?"<script>window.fixtureApplicationSucceeds=true;":"<script>window.fixtureApplicationSucceeds=false;");
    server.loginFixture=R"HTML(<html><title>official structure fixture</title><body><form id="formLogin" action="process.do" method="post" target="hidden_iframe" onsubmit="return fnFormValidation()"><input id="mem_id" name="mem_id"><input id="mem_pw" name="mem_pw" type="password"><input type="submit" value="로그인"></form><iframe name="hidden_iframe"></iframe><script>window.fnFormValidation=()=>true;</script></body></html>)HTML";
    server.portalFixture.replace("<script>","<script>window.fixtureSheets="+QJsonDocument(fixtureSheets).toJson(QJsonDocument::Compact)+";window.fixtureRetainedModal="+(retainedModal?"true;":"false;"));
    if(reloadBeforeSubmit)server.portalFixture.replace("document.body.appendChild(f);", "document.body.appendChild(f);f.querySelector('[name=onBrthdy]').oninput=()=>{if(!sessionStorage.fixtureReloaded){sessionStorage.fixtureReloaded='1';location.reload();}};");
    if(compact)server.portalFixture.replace("if(window.fixtureApplicationSucceeds)window.open('/pd/purchs/download.do','downForm');",
      "if(window.fixtureApplicationSucceeds)setTimeout(()=>window.open('/pd/purchs/download.do','downForm'),1200);");
    server.downloadFixture = R"HTML(<html><head><title>download fixture</title></head><body><table><tr><td><input type="checkbox"></td><td id="fileName"></td><td>0.01 MB</td></tr></table><button onclick="if(document.querySelector('input').checked)location.href='/fixture-'+number+'.zip'">선택 다운로드</button><script>window.number=opener.orderItems[0].mapNo;document.getElementById('fileName').textContent='terrain_'+number+'.zip';</script></body></html>)HTML";
    if(compact)server.downloadFixture=R"HTML(<html><body><table>
      <tr><td><input type="checkbox"></td><td id="zipName"></td><td>0.01 MB</td></tr>
      <tr><td><input type="checkbox"></td><td id="xmlName"></td><td>0.01 MB</td></tr></table>
      <button onclick="if(document.querySelectorAll('input:checked').length===2){location.href='/fixture-'+number+'.zip';setTimeout(()=>location.href='/fixture-'+number+'.xml',500);}">선택 다운로드</button>
      <script>window.number=opener.orderItems[0].mapNo;document.getElementById('zipName').textContent='terrain_'+number+'.zip';document.getElementById('xmlName').textContent='terrain_'+number+'.xml';</script></body></html>)HTML";
    if(retainedModal){server.redirectTransfer=true;server.downloadFixture=batchSdkFixture();}
    if(unsupportedTransfer)server.downloadFixture=R"HTML(<html><title>proprietary transfer fixture</title><body><ul><li>terrain_378044.dxf 4 MB</li><li>terrain_378044.xml 1 KB</li></ul><button>전체 다운로드</button></body></html>)HTML";
    QVERIFY(server.listen(QHostAddress::LocalHost)); QTemporaryDir files;QTemporaryDir survey;
    KaTopographicBrowser browser(nullptr, files.path());
    if(loginRequired)browser.setCredentials(QStringLiteral("fixture-user"),QStringLiteral("fixture-password"));
    if(compact){browser.setCompactMode(true);browser.setDownloadRoot(survey.path()+QStringLiteral("/지형도/원본"));}
    browser.setPortalTestUrl(server.url(QStringLiteral("/ms/map/NlipMap.do?tabGb=total")));
    auto* birth = browser.findChild<QLineEdit*>(QStringLiteral("topographicBirthDate")); QVERIFY(birth);
    birth->setText(QStringLiteral("1990/01/01"));
    auto* purpose = browser.findChild<QLineEdit*>(QStringLiteral("topographicPurpose")); QVERIFY(purpose);
    purpose->setText(QStringLiteral("회귀 테스트용 가상 신청"));
    QSignalSpy selected(&browser, &KaTopographicBrowser::selectionUpdated);
    QSignalSpy received(&browser, &KaTopographicBrowser::sheetDownloaded);
    QSignalSpy needsInput(&browser,&KaTopographicBrowser::automationNeedsInput);
    auto* stage=browser.findChild<QLabel*>(QStringLiteral("topographicCurrentStage"));QVERIFY(stage);
    auto* progress=browser.findChild<QProgressBar*>(QStringLiteral("topographicProgress"));QVERIFY(progress);
    auto* summary=browser.findChild<QLabel*>(QStringLiteral("topographicTransferSummary"));QVERIFY(summary);
    QStringList observedStages;
    QTimer stageObserver;
    connect(&stageObserver,&QTimer::timeout,&browser,[&]{if(!observedStages.contains(stage->text()))observedStages.append(stage->text());});
    stageObserver.start(10);
    // Actual NGII coastal sheets may differ from the nominal-number candidates.
    browser.prepareSheets(1125000, 1980000, 5000, {QStringLiteral("378999")}); browser.show();
    observedStages.append(stage->text());
    if(retainedModal) {
      auto* main=qobject_cast<QWebEngineView*>(browser.findChild<QTabWidget*>(QStringLiteral("topographicTabs"))->widget(0));
      auto selectedCount=std::make_shared<int>(0);
      const auto ordered=[&]{main->page()->runJavaScript(QStringLiteral("window.orderItems?window.orderItems.length:0"),
        [selectedCount](const QVariant& value){*selectedCount=value.toInt();});return *selectedCount>0;};
      QTRY_VERIFY_WITH_TIMEOUT(ordered(),10000);
      QCOMPARE(*selectedCount,2); // Both sheets must be in the same application.
    }
    if(server.streamFixtureArchives) {
      QTRY_VERIFY_WITH_TIMEOUT(server.transferSocket && progress->maximum()==100 && progress->value()==25,15000);
      QVERIFY(stage->text().endsWith(QStringLiteral("다운로드")));
      observedStages.append(stage->text());
      QVERIFY(summary->text().contains(QStringLiteral("도엽 수신 완료 0/1장")));
      QVERIFY(summary->text().contains(QStringLiteral("0/2개")));
      QVERIFY(summary->text().contains(QStringLiteral("terrain_378044.zip")));QVERIFY(received.isEmpty());
      browser.setProcessing(true);
      browser.setPreparationProgress(QStringLiteral("지도에 올리기"),QStringLiteral("SHP 변환 · 378044 강릉"));
      QVERIFY(server.finishTransfer());
    }
    if(unsupportedTransfer) {
      QTRY_VERIFY_WITH_TIMEOUT(!needsInput.isEmpty(),15000);
      QVERIFY(needsInput.last().first().toString().contains(QStringLiteral("공식 파일 전송")));
      QCOMPARE(stage->text(),QStringLiteral("다운로드 실패 · 파일 준비"));
      QVERIFY(summary->text().contains(QStringLiteral("도엽 수신 완료 0/1장")));
      QVERIFY(progress->value()<progress->maximum());QVERIFY(received.isEmpty());
      const QString failedSummary=summary->text();const QString failedStage=stage->text();
      QTest::qWait(1200);QVERIFY(browser.isVisible());QCOMPARE(stage->text(),failedStage);QCOMPARE(summary->text(),failedSummary);
      QVERIFY(browser.findChild<QWidget*>(QStringLiteral("topographicOfficialDetails"))->isHidden());
      QVERIFY(saveCompactCapture(browser,QStringLiteral("file-preparation-failed.png")));
      QCOMPARE(server.searchCount,1);
      auto* main=qobject_cast<QWebEngineView*>(browser.findChild<QTabWidget*>(QStringLiteral("topographicTabs"))->widget(0));
      auto calls=std::make_shared<int>(0);
      main->page()->runJavaScript(QStringLiteral("window.submitCalls"),[calls](const QVariant& value){*calls=value.toInt();});
      QTRY_COMPARE(*calls,1);return;
    }
    if(!applicationSucceeds) {
      QTRY_VERIFY_WITH_TIMEOUT(!needsInput.isEmpty(),15000);
      QVERIFY(needsInput.last().first().toString().contains(QStringLiteral("공식 서버")));
      QVERIFY(received.isEmpty());
    } else {
      QTRY_VERIFY2_WITH_TIMEOUT(received.size()==fixtureSheets.size(),
        qPrintable(needsInput.isEmpty()?QStringLiteral("No automatic result"):needsInput.last().first().toString()),15000);
    }
    if(server.streamFixtureArchives) {
      QVERIFY(summary->text().contains(QStringLiteral("도엽 수신 완료 1/1장")));
      QVERIFY(summary->text().contains(QStringLiteral("2/2개")));
      QVERIFY(stage->text().endsWith(QStringLiteral("지도에 올리기")));
      QCOMPARE(progress->maximum(),1);QCOMPARE(progress->value(),0);QCOMPARE(progress->format(),QStringLiteral("진행률 확인 중"));
      QVERIFY(browser.isVisible());
      const QString history=observedStages.join(QLatin1Char('|'));
      for(const QString& phase:{QStringLiteral("도엽 확인"),QStringLiteral("로그인"),QStringLiteral("신청"),QStringLiteral("파일 준비"),QStringLiteral("다운로드")})
        QVERIFY2(history.contains(phase),qPrintable(QStringLiteral("Missing phase %1 in %2").arg(phase,history)));
      QVERIFY(saveCompactCapture(browser,QStringLiteral("sheet-preparation-counts.png")));
      browser.setProcessing(false);
    }
    QCOMPARE(selected.size(),reloadBeforeSubmit?2:1);
    QCOMPARE(selected.at(0).at(0).toJsonObject().value(QStringLiteral("unavailable")).toArray(),
             QJsonArray{QStringLiteral("378999")});
    if(applicationSucceeds) {
      const auto record = received.at(0).at(1).toJsonObject();
      QCOMPARE(record.value(QStringLiteral("num")).toString(), QStringLiteral("378044"));
      QCOMPARE(record.value(QStringLiteral("projection")).toString(), QStringLiteral("GRS80"));
      QCOMPARE(record.value(QStringLiteral("fileExt")).toString(), QStringLiteral("dxf"));
    }
    auto* tabs=browser.findChild<QTabWidget*>(QStringLiteral("topographicTabs"));
    auto* main=qobject_cast<QWebEngineView*>(tabs->widget(0));
    const auto calls=std::make_shared<int>(0);
    main->page()->runJavaScript(QStringLiteral("window.submitCalls"),[calls](const QVariant& v){*calls=v.toInt();});
    QTRY_COMPARE(*calls,1);
    if(loginRequired) {
      QVERIFY(server.loggedIn);QVERIFY(server.loginCheckCount>=2);QCOMPARE(server.loginSubmissionCount,1);
      auto openings=std::make_shared<QVariantMap>();
      main->page()->runJavaScript(QStringLiteral("({login:window.loginCalls,application:window.orderCalls})"),
        [openings](const QVariant& value){*openings=value.toMap();});
      QTRY_COMPARE(openings->value(QStringLiteral("login")).toInt(),1);
      QCOMPARE(openings->value(QStringLiteral("application")).toInt(),1);
    }
    browser.prepareSheets(1125000,1980000,5000,{QStringLiteral("378999")});
    QCOMPARE(server.searchCount,reloadBeforeSubmit?2:1); QCOMPARE(received.size(),applicationSucceeds?fixtureSheets.size():0);
    if(compact) {
      QVERIFY(browser.findChild<QWidget*>(QStringLiteral("topographicOfficialDetails"))->isHidden());
      const QString completed=received.first().first().toString();
      QVERIFY(completed.startsWith(survey.path()+QStringLiteral("/지형도/원본/")));
      QCOMPARE(QFileInfo(completed).suffix(),QStringLiteral("zip"));
      QVERIFY(QFileInfo::exists(QFileInfo(completed).dir().filePath(QStringLiteral("terrain_378044.xml"))));
    }
  }

  void cachedActualSheetSkipsLoginAndApplication() {
    LocalDownloadServer server;
    server.portalFixture="<html><title>cached fixture</title></html>";
    server.searchFixture=R"({"items":[{"num":"378044","name":"강릉","scale":"25000","projection":"GRS80","fileExt":"dxf","historyNo":0,"minx":1120928.8,"miny":1973152.5,"maxx":1132145.6,"maxy":1987191.4}]})";
    QVERIFY(server.listen(QHostAddress::LocalHost)); QTemporaryDir files;
    KaTopographicBrowser browser(nullptr,files.path());
    browser.setPortalTestUrl(server.url(QStringLiteral("/ms/map/NlipMap.do?tabGb=total")));
    QSignalSpy selected(&browser,&KaTopographicBrowser::selectionUpdated);
    QSignalSpy received(&browser,&KaTopographicBrowser::sheetDownloaded);
    browser.prepareSheets(1125000,1980000,5000,{QStringLiteral("378999")},{QStringLiteral("378044")});
    QTRY_COMPARE_WITH_TIMEOUT(selected.size(),1,15000);
    const auto data=selected.first().first().toJsonObject();
    QCOMPARE(data.value(QStringLiteral("items")).toArray().size(),1);
    QCOMPARE(data.value(QStringLiteral("cached")).toArray(),QJsonArray{QStringLiteral("378044")});
    QCOMPARE(server.loginSubmissionCount,0);QVERIFY(received.isEmpty());
  }

  void unexpectedProductStopsBeforeConsentOrApplication() {
    LocalDownloadServer server;
    server.portalFixture=R"(<html><head><title>wrong product</title></head><body><script>window.map={};window.mapQuery={};window.MEASURE_API={};</script></body></html>)";
    server.searchFixture=R"({"items":[{"num":"378044","scale":"5000","fileExt":"shp","projection":"GRS80"}]})";
    QVERIFY(server.listen(QHostAddress::LocalHost)); QTemporaryDir files;
    KaTopographicBrowser browser(nullptr, files.path());
    browser.setPortalTestUrl(server.url(QStringLiteral("/ms/map/NlipMap.do?tabGb=total")));
    QSignalSpy needsInput(&browser,&KaTopographicBrowser::automationNeedsInput);
    QSignalSpy selected(&browser,&KaTopographicBrowser::selectionUpdated);
    browser.prepareSheets(1125000,1980000,5000,{QStringLiteral("378044")});
    QTRY_VERIFY_WITH_TIMEOUT(!needsInput.isEmpty(),15000);
    QVERIFY(selected.isEmpty());
  }

  void emptyAvailabilityIsReportedWithoutApplication() {
    LocalDownloadServer server;server.portalFixture="<html><title>empty fixture</title></html>";
    server.searchFixture=R"({"items":[]})";
    QVERIFY(server.listen(QHostAddress::LocalHost)); QTemporaryDir files;
    KaTopographicBrowser browser(nullptr,files.path());
    browser.setPortalTestUrl(server.url(QStringLiteral("/ms/map/NlipMap.do?tabGb=total")));
    QSignalSpy selected(&browser,&KaTopographicBrowser::selectionUpdated);
    QSignalSpy received(&browser,&KaTopographicBrowser::sheetDownloaded);
    browser.prepareSheets(1125000,1980000,5000,{QStringLiteral("378044")});
    QTRY_COMPARE_WITH_TIMEOUT(selected.size(),1,10000);
    QVERIFY(selected.at(0).at(0).toJsonObject().value(QStringLiteral("items")).toArray().isEmpty());
    QCOMPARE(selected.at(0).at(0).toJsonObject().value(QStringLiteral("unavailable")).toArray().size(),1);
    QVERIFY(received.isEmpty());
  }

  void knownOfficialLoginSubmitsOnlyOnceAndStopPreventsMoreWork_data() {
    QTest::addColumn<bool>("unanswered");
    QTest::newRow("rejected-login-in-hidden-tab-times-out")<<false;
    QTest::newRow("unanswered-login-in-hidden-tab-times-out")<<true;
  }
  void knownOfficialLoginSubmitsOnlyOnceAndStopPreventsMoreWork() {
    QFETCH(bool,unanswered);
    LocalDownloadServer server;server.loggedIn=false;
    server.holdLoginResponse=unanswered;
    server.searchFixture=R"({"items":[{"num":"378044","name":"fixture","scale":"25000","projection":"GRS80","fileExt":"dxf","historyNo":0,"minx":1120928.8,"miny":1973152.5,"maxx":1132145.6,"maxy":1987191.4}]})";
    server.portalFixture=R"HTML(<html><title>login route fixture</title><body><main class="map total"><div id="totalSearch"><section class="mapList measure"><ul class="_accordion"><li><dl><dt><button value="0">DXF</button><select><option value="25000">25000</option></select></dt><dd class="items"></dd></dl></li></ul></section></div></main><script>
    window.map={getView:()=>({getProjection:()=>({getCode:()=> 'EPSG:5179'}),setCenter(){},fit(){}})};
    window.mapQuery={clear(){}};window.integ={};window.fixtureEvents={on(name,fn){window.fixtureOrderComplete=fn;},off(){}};window.jQuery=x=>x===document?fixtureEvents:x;
    window.MEASURE_API={downArray:[],completed(){document.querySelector('.items').innerHTML='<input type="checkbox" name="cb_suchi" mapType="0" scale="25000" num="378044" nam="fixture" hisno="0" fileext="dxf">';document.querySelector('[name=cb_suchi]').onchange=function(){MEASURE_API.downArray=[{mapNo:'378044',mapScale:'25000',orderTp:'0',ext:'dxf'}];}}};
    window.fnLogin=()=>window.open('/member/login.do','id-login');
    </script></body></html>)HTML";
    server.loginFixture=R"HTML(<html><title>official structure fixture</title><body><form id="formLogin" action="process.do" method="post" target="hidden_iframe" onsubmit="return fnFormValidation()"><input id="mem_id" name="mem_id"><input id="mem_pw" name="mem_pw" type="password"><input type="submit" value="로그인"></form><iframe name="hidden_iframe"></iframe><script>window.testClock=0;Date.now=()=>testClock;window.fnFormValidation=()=>true;</script></body></html>)HTML";
    QVERIFY(server.listen(QHostAddress::LocalHost)); QTemporaryDir files;
    KaTopographicBrowser browser(nullptr,files.path());
    browser.setCompactMode(true);
    browser.setPortalTestUrl(server.url(QStringLiteral("/ms/map/NlipMap.do?tabGb=total")));
    browser.findChild<QLineEdit*>(QStringLiteral("topographicLoginId"))->setText(QStringLiteral("fixture-user"));
    browser.findChild<QLineEdit*>(QStringLiteral("topographicLoginPassword"))->setText(QStringLiteral("fixture-password"));
    QSignalSpy needsInput(&browser,&KaTopographicBrowser::automationNeedsInput);
    QSignalSpy received(&browser,&KaTopographicBrowser::sheetDownloaded);
    browser.prepareSheets(1125000,1980000,5000,{QStringLiteral("378044")});browser.show();
    QTRY_COMPARE_WITH_TIMEOUT(server.loginSubmissionCount,1,20000);
    QTest::qWait(1000); // Multiple polling ticks must not repeat a failed login.
    QCOMPARE(server.loginSubmissionCount,1);
    auto* tabs=browser.findChild<QTabWidget*>(QStringLiteral("topographicTabs"));QVERIFY(tabs);
    QWebEngineView* login=nullptr;
    for(int i=0;i<tabs->count();++i)if(auto* page=qobject_cast<QWebEngineView*>(tabs->widget(i)))
      if(page->url().path()==QLatin1String("/member/login.do"))login=page;
    QVERIFY(login);QVERIFY(browser.findChild<QWidget*>(QStringLiteral("topographicOfficialDetails"))->isHidden());
    // Advance only the fixture clock. The real Browser timer must report the
    // deadline while this login tab still wins its normal page selection.
    needsInput.clear();
    auto advanced=std::make_shared<bool>(false);
    login->page()->runJavaScript(QStringLiteral("window.testClock=300001;true"),
      [advanced](const QVariant& value){*advanced=value.toBool();});
    QTRY_VERIFY(*advanced);
    QTRY_VERIFY_WITH_TIMEOUT(!needsInput.isEmpty(),5000);
    QVERIFY(needsInput.last().first().toString().contains(QStringLiteral("시간이 초과")));
    QCOMPARE(server.loginSubmissionCount,1);QVERIFY(received.isEmpty());
    QVERIFY(browser.findChild<QWidget*>(QStringLiteral("topographicOfficialDetails"))->isHidden());
    browser.stopAutomatic();QTest::qWait(500);QCOMPARE(server.loginSubmissionCount,1);
    QVERIFY(QSettings().value(QStringLiteral("topographicLoginPassword")).isNull());
  }
  void automaticLoginWaitsUntilTheFormCanSubmit_data() {
    QTest::addColumn<bool>("parsingPaused");
    QTest::newRow("HTML-is-still-arriving")<<true;
    QTest::newRow("ready-handler-and-target-not-initialized")<<false;
  }
  void automaticLoginWaitsUntilTheFormCanSubmit() {
    QFETCH(bool,parsingPaused);
    LocalDownloadServer server;server.loggedIn=false;server.acceptLogin=true;
    server.searchFixture=R"({"items":[{"num":"378044","name":"fixture","scale":"25000","projection":"GRS80","fileExt":"dxf","historyNo":0,"minx":1120928.8,"miny":1973152.5,"maxx":1132145.6,"maxy":1987191.4}]})";
    server.portalFixture=R"HTML(<html><body><main class="map total"><div id="totalSearch"><section class="mapList measure"><ul class="_accordion"><li><dl><dt><button value="0">DXF</button><select><option value="25000">25000</option></select></dt><dd class="items"></dd></dl></li></ul></section></div></main><script>
      window.map={getView:()=>({getProjection:()=>({getCode:()=> 'EPSG:5179'}),setCenter(){},fit(){}})};
      window.mapQuery={clear(){}};window.integ={};window.jQuery=x=>x;
      window.MEASURE_API={downArray:[],completed(){document.querySelector('.items').innerHTML='<input type="checkbox" name="cb_suchi" mapType="0" scale="25000" num="378044" nam="fixture" hisno="0" fileext="dxf">';document.querySelector('[name=cb_suchi]').onchange=()=>{MEASURE_API.downArray=[{orderTp:'0',mapScale:'25000',ext:'dxf',mapNo:'378044'}];};}};
      window.fnLogin=()=>window.open('/member/login.do','id-login');
    </script></body></html>)HTML";
    server.loginFixture=R"HTML(<html><head><title>delayed official form</title>PAUSE</head><body>
      <form id="formLogin" action="process.do" method="post" target="hidden_iframe" onsubmit="return fnFormValidation()">
      <input id="mem_id" name="mem_id"><input id="mem_pw" name="mem_pw" type="password"><input type="submit"></form>
      <script>var formLogin=null;window.fnFormValidation=function(){formLogin.find('input');};
      window.finishReady=function(){formLogin={find:()=>true};let target=document.createElement('iframe');target.name='hidden_iframe';document.body.appendChild(target);};READY</script>
      </body></html>)HTML";
    server.loginFixture.replace("PAUSE",parsingPaused?"<script src='/held-login-script.js'></script>":"");
    server.loginFixture.replace("READY",parsingPaused?"document.addEventListener('DOMContentLoaded',finishReady);":"");
    QVERIFY(server.listen(QHostAddress::LocalHost));QTemporaryDir files;
    KaTopographicBrowser browser(nullptr,files.path());browser.setCompactMode(true);browser.show();
    browser.setCredentials(QStringLiteral("fixture-user"),QStringLiteral("fixture-password"));
    browser.setPortalTestUrl(server.url(QStringLiteral("/ms/map/NlipMap.do")));
    QSignalSpy attention(&browser,&KaTopographicBrowser::automationNeedsInput);
    browser.prepareSheets(1125000,1980000,5000,{QStringLiteral("378044")});
    auto* tabs=browser.findChild<QTabWidget*>(QStringLiteral("topographicTabs"));QVERIFY(tabs);
    QTRY_COMPARE_WITH_TIMEOUT(tabs->count(),2,15000);
    auto* login=qobject_cast<QWebEngineView*>(tabs->widget(1));QVERIFY(login);
    if(parsingPaused)QTRY_VERIFY_WITH_TIMEOUT(server.pendingLoginScript,10000);
    // Synchronize with a real automatic poll; do not simulate the production
    // adapter or release the held HTTP response after an arbitrary sleep.
    auto state=std::make_shared<QVariantMap>();
    const auto polled=[&]{login->page()->runJavaScript(QStringLiteral(R"JS((()=>{
      const password=document.getElementById('mem_pw');
      return {polled:!!window.__kaLoginWait,empty:!password||password.value==='',parsing:document.readyState!=='complete'};
    })())JS"),[state](const QVariant& value){*state=value.toMap();});return state->value(QStringLiteral("polled")).toBool();};
    QTRY_VERIFY_WITH_TIMEOUT(polled(),10000);
    const bool failedBeforeReady=!attention.isEmpty();
    if(failedBeforeReady)QVERIFY(saveCompactCapture(browser,QStringLiteral("login-form-before-failed.png")));
    QVERIFY2(!failedBeforeReady,"Automatic polling rejected an official form before its page/ready handler finished");
    QVERIFY2(state->value(QStringLiteral("empty")).toBool(),"Credentials were filled before the official form was ready");
    QCOMPARE(server.loginSubmissionCount,0);
    QVERIFY(saveCompactCapture(browser,QStringLiteral("login-form-waiting.png")));
    if(parsingPaused) {
      server.pendingLoginScript->write("HTTP/1.1 200 OK\r\nContent-Type: application/javascript\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
      server.pendingLoginScript->disconnectFromHost();
    } else login->page()->runJavaScript(QStringLiteral("finishReady();"));
    QTRY_VERIFY_WITH_TIMEOUT(server.loginSubmissionCount==1 || !attention.isEmpty(),10000);
    QVERIFY2(attention.isEmpty(),"The prepared official login form failed to resume");
    QCOMPARE(server.loginSubmissionCount,1);
    QTRY_COMPARE_WITH_TIMEOUT(tabs->count(),1,10000);
    QVERIFY(server.loggedIn);
    browser.stopAutomatic();
  }

  void genuinePageConnectionFailureStopsAutomaticDownload() {
    bool failedLoad=false;
    LocalDownloadServer server;QVERIFY(server.listen(QHostAddress::LocalHost));QTemporaryDir files;
    server.failPortalConnection=true;
    KaTopographicBrowser browser(nullptr,files.path());browser.setCompactMode(true);browser.show();
    browser.setPortalTestUrl(server.url(QStringLiteral("/ms/map/NlipMap.do")));
    auto* view=browser.findChild<QWebEngineView*>();QVERIFY(view);
    connect(view->page(),&QWebEnginePage::loadingChanged,&browser,[&](const QWebEngineLoadingInfo& info){
      failedLoad|=info.status()==QWebEngineLoadingInfo::LoadFailedStatus;
    });
    QSignalSpy attention(&browser,&KaTopographicBrowser::automationNeedsInput);
    browser.prepareSheets(1125000,1980000,5000,{QStringLiteral("378044")});
    QTRY_VERIFY_WITH_TIMEOUT(failedLoad,15000);
    QTRY_VERIFY_WITH_TIMEOUT(!attention.isEmpty(),5000);
    QVERIFY(browser.findChild<QLabel*>(QStringLiteral("topographicCurrentStage"))->text().contains(QStringLiteral("실패")));
    QCOMPARE(server.loginSubmissionCount,0);
  }

  void completedUnrecognizedLoginFormNeverReceivesCredentials() {
    LocalDownloadServer server;QVERIFY(server.listen(QHostAddress::LocalHost));QTemporaryDir files;
    server.loginFixture=R"HTML(<html><body><form id="formLogin" action="unexpected.do" method="post" target="hidden_iframe"><input id="mem_id" name="mem_id"><input id="mem_pw" name="mem_pw" type="password"><input type="submit"></form><iframe name="hidden_iframe"></iframe><script>window.fnFormValidation=()=>true;</script></body></html>)HTML";
    KaTopographicBrowser browser(nullptr,files.path());browser.setCompactMode(true);browser.show();
    browser.navigate(server.url(QStringLiteral("/member/login.do")));
    auto* view=browser.findChild<QWebEngineView*>();QVERIFY(view);
    auto loaded=std::make_shared<bool>(false);
    const auto ready=[&]{view->page()->runJavaScript(QStringLiteral("document.readyState==='complete' && !!document.getElementById('mem_pw')"),
      [loaded](const QVariant& value){*loaded=value.toBool();});return *loaded;};
    QTRY_VERIFY_WITH_TIMEOUT(ready(),10000);
    QFile adapter(QStringLiteral("data/providers/ngii-topographic.js"));QVERIFY(adapter.open(QIODevice::ReadOnly));
    const auto code=QStringLiteral("(()=>{const adapter=")+QString::fromUtf8(adapter.readAll())+QStringLiteral(R"JS(;
      const result=adapter({command:'login',generation:'fixture',loginAttempt:'1',loginId:'fixture-user',loginPassword:'fixture-password'});
      return {state:result.state,empty:document.getElementById('mem_pw').value===''};})())JS");
    auto result=std::make_shared<QVariantMap>();view->page()->runJavaScript(code,[result](const QVariant& value){*result=value.toMap();});
    QTRY_VERIFY(!result->isEmpty());
    QCOMPARE(result->value(QStringLiteral("state")).toString(),QStringLiteral("blocked"));
    QVERIFY(result->value(QStringLiteral("empty")).toBool());QCOMPARE(server.loginSubmissionCount,0);
  }

  void loginStatusChecksAreSerializedAndExpireWithoutReopening() {
    LocalDownloadServer server;QVERIFY(server.listen(QHostAddress::LocalHost));QTemporaryDir files;
    KaTopographicBrowser browser(nullptr,files.path());browser.navigate(server.url());
    auto* tabs=browser.findChild<QTabWidget*>(QStringLiteral("topographicTabs"));QVERIFY(tabs);
    auto* view=qobject_cast<QWebEngineView*>(tabs->widget(0));QVERIFY(view);
    QTRY_COMPARE_WITH_TIMEOUT(view->title(),QStringLiteral("local fixture"),10000);
    QFile script(QStringLiteral("data/providers/ngii-topographic.js"));QVERIFY(script.open(QIODevice::ReadOnly));
    auto ready=std::make_shared<bool>(false);
    view->page()->runJavaScript(QStringLiteral("window.runProvider=")+QString::fromUtf8(script.readAll())+QStringLiteral(R"JS(;
      window.testClock=0;Date.now=()=>testClock;window.fetchCalls=0;window.loginCalls=0;
      window.fetch=()=>{++fetchCalls;return new Promise(resolve=>window.finishCheck=()=>resolve({ok:true,json:()=>Promise.resolve({isLogin:false})}));};
      window.fnLogin=()=>++loginCalls;
      window.pollMany=()=>{let last;for(let i=0;i<64;++i)last=runProvider({command:'openForm',generation:'1',number:'378044'});
        return {fetches:fetchCalls,logins:loginCalls,state:last.state};};true;
    )JS"),[ready](const QVariant& result){*ready=result.toBool();});
    QTRY_VERIFY(*ready);
    auto result=std::make_shared<QVariantMap>();
    const auto poll=[&](const QString& code){
      result->clear();view->page()->runJavaScript(code,[result](const QVariant& value){*result=value.toMap();});
    };
    poll(QStringLiteral("pollMany()"));
    QTRY_COMPARE(result->value(QStringLiteral("fetches")).toInt(),1);
    QCOMPARE(result->value(QStringLiteral("state")).toString(),QStringLiteral("checkingLogin"));
    // Finish the controlled asynchronous response before another event-loop turn.
    poll(QStringLiteral("finishCheck();({finished:true})"));QTRY_VERIFY(result->value(QStringLiteral("finished")).toBool());
    poll(QStringLiteral("pollMany()"));
    QTRY_COMPARE(result->value(QStringLiteral("logins")).toInt(),1);
    QCOMPARE(result->value(QStringLiteral("fetches")).toInt(),1);
    QCOMPARE(result->value(QStringLiteral("state")).toString(),QStringLiteral("waitingLogin"));
    poll(QStringLiteral("testClock=2000;pollMany()"));
    QTRY_COMPARE(result->value(QStringLiteral("fetches")).toInt(),2);
    QCOMPARE(result->value(QStringLiteral("logins")).toInt(),1);
    poll(QStringLiteral("finishCheck();({finished:true})"));QTRY_VERIFY(result->value(QStringLiteral("finished")).toBool());
    poll(QStringLiteral("testClock=300001;pollMany()"));
    QTRY_COMPARE(result->value(QStringLiteral("state")).toString(),QStringLiteral("blocked"));
    QCOMPARE(result->value(QStringLiteral("fetches")).toInt(),2);
    QCOMPARE(result->value(QStringLiteral("logins")).toInt(),1);
  }
  void hiddenAnyIdChooserOpensIdMethodOnce_data() {
    QTest::addColumn<bool>("matchingContract");
    QTest::newRow("hidden-official-id-choice")<<true;
    QTest::newRow("changed-id-choice-does-not-click")<<false;
  }
  void hiddenAnyIdChooserOpensIdMethodOnce() {
    QFETCH(bool,matchingContract);
    LocalDownloadServer server;QVERIFY(server.listen(QHostAddress::LocalHost));QTemporaryDir files;
    // Public NGII structure: its paint-dependent ResizeObserver has not made
    // the container visible, but the ID login method is fully initialized.
    server.anyIdFixture=R"HTML(<html><body><div id="loginContainer" style="visibility:hidden">
      <x-anyidc style="display:block;height:196px"></x-anyidc>
      <a class="login-link open-pop" data-target="UI_POT_01_107_wp" href="javascript:testFnLogin();">아이디 로그인</a>
      </div><script>window.idOpenings=0;window.testFnLogin=()=>{++idOpenings;window.open('/member/login.do','id-login');};</script></body></html>)HTML";
    if(!matchingContract)server.anyIdFixture.replace("href=\"javascript:testFnLogin();\"","href=\"javascript:unrecognizedLogin();\"");
    server.loginFixture="<html><title>public ID form</title><body><form id='formLogin'><input id='mem_pw' type='password'></form></body></html>";
    KaTopographicBrowser browser(nullptr,files.path());browser.setCompactMode(true);browser.show();
    browser.navigate(server.url(QStringLiteral("/anyid/common/anyidLogin.do")));
    auto* tabs=browser.findChild<QTabWidget*>(QStringLiteral("topographicTabs"));QVERIFY(tabs);
    auto* main=qobject_cast<QWebEngineView*>(tabs->widget(0));QVERIFY(main);
    auto ready=std::make_shared<bool>(false);
    auto initialized=[&]{main->page()->runJavaScript(QStringLiteral("typeof testFnLogin==='function' && document.readyState==='complete'"),
      [ready](const QVariant& value){*ready=value.toBool();});return *ready;};
    QTRY_VERIFY_WITH_TIMEOUT(initialized(),10000);
    QFile adapter(QStringLiteral("data/providers/ngii-topographic.js"));QVERIFY(adapter.open(QIODevice::ReadOnly));
    const QString script=QStringLiteral("(()=>{const adapter=")+QString::fromUtf8(adapter.readAll())+QStringLiteral(R"JS(;
      for(let i=0;i<10;++i)adapter({command:'login',generation:'fixture',loginAttempt:'1'});
      return {openings:window.idOpenings,visibility:getComputedStyle(document.getElementById('loginContainer')).visibility};})())JS");
    auto result=std::make_shared<QVariantMap>();
    main->page()->runJavaScript(script,[result](const QVariant& value){*result=value.toMap();});
    QTRY_VERIFY(!result->isEmpty());
    QCOMPARE(result->value(QStringLiteral("visibility")).toString(),QStringLiteral("hidden"));
    QTRY_COMPARE(tabs->count(),matchingContract?2:1);
    if(matchingContract) {
      auto* id=qobject_cast<QWebEngineView*>(tabs->widget(1));QVERIFY(id);
      QTRY_COMPARE(id->title(),QStringLiteral("public ID form"));
      QCOMPARE(id->page()->profile(),main->page()->profile());
      QVERIFY(!id->isVisible());
    }
    auto openings=std::make_shared<int>(-1);
    const auto readOpenings=[&]{main->page()->runJavaScript(QStringLiteral("window.idOpenings"),
      [openings](const QVariant& value){*openings=value.toInt();});return *openings;};
    QTRY_COMPARE(readOpenings(),matchingContract?1:0);
    QCOMPARE(server.loginSubmissionCount,0);
  }

  void fileSelectionWaitsUntilDownloadPageIsReady() {
    LocalDownloadServer server;QVERIFY(server.listen(QHostAddress::LocalHost));QTemporaryDir files;
    server.downloadFixture=R"HTML(<html><head><script src='/held-login-script.js'></script></head><body><table><tr><td><input type='checkbox'></td><td>terrain_378044.zip</td><td>4 MB</td></tr></table><button>선택 다운로드</button></body></html>)HTML";
    KaTopographicBrowser browser(nullptr,files.path());browser.setCompactMode(true);browser.show();
    browser.navigate(server.url(QStringLiteral("/pd/purchs/download.do")));
    auto* view=browser.findChild<QWebEngineView*>();QVERIFY(view);
    QTRY_VERIFY_WITH_TIMEOUT(server.pendingLoginScript,10000);
    QFile file(QStringLiteral("data/providers/ngii-topographic.js"));QVERIFY(file.open(QIODevice::ReadOnly));
    const QString script=QString::fromUtf8(file.readAll())+QStringLiteral("({command:'files',generation:'fixture',number:'378044'});");
    auto state=std::make_shared<QVariantMap>();
    view->page()->runJavaScript(script,[state](const QVariant& value){*state=value.toMap();});
    QTRY_VERIFY_WITH_TIMEOUT(!state->isEmpty(),5000);
    QCOMPARE(state->value(QStringLiteral("state")).toString(),QStringLiteral("waitingFiles"));
    server.pendingLoginScript->write("HTTP/1.1 200 OK\r\nContent-Type: application/javascript\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    server.pendingLoginScript->disconnectFromHost();
    const auto ready=[&]{view->page()->runJavaScript(script,[state](const QVariant& value){*state=value.toMap();});return state->value(QStringLiteral("state")).toString()==QLatin1String("ready");};
    QTRY_VERIFY_WITH_TIMEOUT(ready(),10000);
    QCOMPARE(state->value(QStringLiteral("files")).toInt(),1);
  }

  void officialControllerStartsWithoutAnExternalAgent() {
    LocalDownloadServer server;QVERIFY(server.listen(QHostAddress::LocalHost));QTemporaryDir files;
    server.downloadFixture=R"HTML(<html><head><script>
      window.innorix={create:o=>{window.createdOptions=o;return {};}};
      window.onload=()=>{window.control=innorix.create({el:'#fileControl',transferMode:'download',agent:true,showTransferWindow:true});};
    </script></head><body><div id='fileControl'></div></body></html>)HTML";
    KaTopographicBrowser browser(nullptr,files.path());browser.setCompactMode(true);browser.show();
    browser.navigate(server.url(QStringLiteral("/nlippd/pd/innorix/innorixdownLoad.do")));
    auto* view=browser.findChild<QWebEngineView*>();QVERIFY(view);
    auto options=std::make_shared<QVariantMap>();
    const auto ready=[&]{view->page()->runJavaScript(QStringLiteral("window.createdOptions||{}"),[options](const QVariant& value){*options=value.toMap();});return !options->isEmpty();};
    QTRY_VERIFY_WITH_TIMEOUT(ready(),10000);
    QCOMPARE(options->value(QStringLiteral("agent")).toBool(),false);
    QCOMPARE(options->value(QStringLiteral("showTransferWindow")).toBool(),false);
    QCOMPARE(options->value(QStringLiteral("transferMode")).toString(),QStringLiteral("download"));
  }

  void officialControllerTransfersOnlyTheRequestedSheet_data() {
    QTest::addColumn<bool>("matching");
    QTest::addColumn<QString>("fileId");
    QTest::newRow("current-sheet")<<true<<QStringLiteral("fixture-data");
    QTest::newRow("another-sheet-is-rejected")<<false<<QStringLiteral("fixture-data");
    QTest::newRow("missing-file-id-is-rejected")<<true<<QString();
    QTest::newRow("unknown-sdk-file-id-does-not-download")<<true<<QStringLiteral("unknown-data");
  }
  void officialControllerTransfersOnlyTheRequestedSheet() {
    QFETCH(bool,matching);
    QFETCH(QString,fileId);
    LocalDownloadServer server;QVERIFY(server.listen(QHostAddress::LocalHost));QTemporaryDir files;
    server.downloadFixture=R"HTML(<html><body><div id='fileControl'></div><script>
      const records=[{id:'FILE_ID',printFileName:'terrain_NUMBER.zip',fileSize:19,downloadUrl:location.origin+'/nlippd/pd/innorix/innorixDownLoad2.do?fileName=terrain_NUMBER.zip&directory=fixture'}];
      if(!records[0].id)delete records[0].id;
      window.control={getOption:()=>({agent:false}),getDownloadFiles:()=>records,download(id){window.downloadCalls=(window.downloadCalls||0)+1;if(id!=='fixture-data')return false;location.href=records[0].downloadUrl;}};
    </script></body></html>)HTML";
    server.downloadFixture.replace("NUMBER",matching?"378044":"378043");
    server.downloadFixture.replace("FILE_ID",fileId.toUtf8());
    KaTopographicBrowser browser(nullptr,files.path());browser.setCompactMode(true);browser.show();
    QSignalSpy received(&browser,&KaTopographicBrowser::fileDownloaded);
    browser.navigate(server.url(QStringLiteral("/nlippd/pd/innorix/innorixdownLoad.do")));
    auto* view=browser.findChild<QWebEngineView*>();QVERIFY(view);
    auto loaded=std::make_shared<bool>(false);
    const auto ready=[&]{view->page()->runJavaScript(QStringLiteral("document.readyState==='complete' && !!window.control"),[loaded](const QVariant& value){*loaded=value.toBool();});return *loaded;};
    QTRY_VERIFY_WITH_TIMEOUT(ready(),10000);
    QFile file(QStringLiteral("data/providers/ngii-topographic.js"));QVERIFY(file.open(QIODevice::ReadOnly));
    const QString adapter=QString::fromUtf8(file.readAll());
    auto state=std::make_shared<QVariantMap>();
    view->page()->runJavaScript(adapter+QStringLiteral("({command:'files',generation:'fixture',number:'378044'});"),[state](const QVariant& value){*state=value.toMap();});
    QTRY_VERIFY_WITH_TIMEOUT(!state->isEmpty(),5000);
    const bool readyList=matching && !fileId.isEmpty();
    QCOMPARE(state->value(QStringLiteral("state")).toString(),readyList?QStringLiteral("ready"):QStringLiteral("blocked"));
    if(!readyList){QCOMPARE(received.size(),0);QVERIFY(server.sdkRequests.isEmpty());return;}
    QCOMPARE(state->value(QStringLiteral("files")).toInt(),1);
    QVERIFY(state->value(QStringLiteral("serial")).toBool());
    const auto list=state->value(QStringLiteral("manifest")).toList();QCOMPARE(list.size(),1);
    QCOMPARE(list.first().toMap().value(QStringLiteral("id")).toString(),fileId);
    state->clear();
    view->page()->runJavaScript(adapter+QStringLiteral("({command:'download',generation:'fixture',number:'378044',fileIndex:0});"),
      [state](const QVariant& value){*state=value.toMap();});
    if(fileId==QLatin1String("unknown-data")) {
      QTRY_VERIFY_WITH_TIMEOUT(!state->isEmpty(),5000);
      QCOMPARE(state->value(QStringLiteral("state")).toString(),QStringLiteral("blocked"));
      QVERIFY(received.isEmpty());QVERIFY(server.sdkRequests.isEmpty());return;
    }
    QTRY_COMPARE_WITH_TIMEOUT(received.size(),1,10000);
    QFile actual(received.first().first().toString());QVERIFY(actual.open(QIODevice::ReadOnly));QCOMPARE(actual.readAll(),QByteArray("PK-fixture-complete"));
  }

  void batchApplicationSerializesActualHttpCompletionAndWaitsForEachXml_data() {
    QTest::addColumn<bool>("cancelFirst");
    QTest::newRow("two-sheets-one-application-four-serial-files")<<false;
    QTest::newRow("cancel-first-file-blocks-later-sdk-files")<<true;
  }
  void batchApplicationSerializesActualHttpCompletionAndWaitsForEachXml() {
    QFETCH(bool,cancelFirst);
    LocalDownloadServer server;server.portalFixture=batchPortalFixture();server.downloadFixture=batchSdkFixture();
    server.redirectTransfer=true;server.holdSdkTransfers=true;
    server.searchFixture=R"({"items":[{"num":"378044","name":"강릉","scale":"25000","projection":"GRS80","fileExt":"dxf","historyNo":0,"makeYer":"2024","minx":1120928.8,"miny":1973152.5,"maxx":1132145.6,"maxy":1987191.4},{"num":"378043","name":"강릉","scale":"25000","projection":"GRS80","fileExt":"dxf","historyNo":0,"makeYer":"2024","minx":1120928.8,"miny":1973152.5,"maxx":1132145.6,"maxy":1987191.4}]})";
    QVERIFY(server.listen(QHostAddress::LocalHost));QTemporaryDir files;
    KaTopographicBrowser browser(nullptr,files.path());browser.setCompactMode(true);browser.show();
    browser.setPortalTestUrl(server.url(QStringLiteral("/ms/map/NlipMap.do")));
    browser.findChild<QLineEdit*>(QStringLiteral("topographicBirthDate"))->setText(QStringLiteral("1990/01/01"));
    browser.findChild<QLineEdit*>(QStringLiteral("topographicPurpose"))->setText(QStringLiteral("가상 회귀 검증"));
    auto* rows=browser.findChild<QTreeWidget*>(QStringLiteral("topographicDownloads"));QVERIFY(rows);
    auto* stage=browser.findChild<QLabel*>(QStringLiteral("topographicCurrentStage"));QVERIFY(stage);
    auto* progress=browser.findChild<QProgressBar*>(QStringLiteral("topographicProgress"));QVERIFY(progress);
    QSignalSpy received(&browser,&KaTopographicBrowser::sheetDownloaded);
    bool nextRequestBeforeCompletion=false;
    server.sdkRequestObserved=[&](const QString& name) {
      if(server.sdkRequests.size()<=1)return;
      const QString previous=server.sdkRequests.at(server.sdkRequests.size()-2);
      bool complete=false;
      for(int i=0;i<rows->topLevelItemCount();++i) {
        const auto* row=rows->topLevelItem(i);
        if(row->text(0)==previous && row->text(2)==QStringLiteral("완료"))complete=true;
      }
      if(!complete)nextRequestBeforeCompletion=true;
      Q_UNUSED(name);
    };
    browser.prepareSheets(1125000,1980000,5000,{QStringLiteral("378044"),QStringLiteral("378043")});
    QTRY_VERIFY2_WITH_TIMEOUT(!server.sdkRequests.isEmpty() || stage->text().startsWith(QStringLiteral("다운로드 실패")),
      qPrintable(stage->text()),15000);
    QVERIFY2(!server.sdkRequests.isEmpty(),qPrintable(stage->text()));
    QCOMPARE(server.sdkRequests.first(),QStringLiteral("terrain_378044.dxf"));
    QTRY_VERIFY_WITH_TIMEOUT(progress->maximum()==100 && progress->value()==25,10000);
    // The deployed SDK's download() path starts every file 200 ms apart. Keep
    // the first real HTTP response unfinished past that entire four-file window.
    QTest::qWait(900);
    QCOMPARE(server.sdkRequests,QStringList{QStringLiteral("terrain_378044.dxf")});
    QVERIFY(!nextRequestBeforeCompletion);QVERIFY(received.isEmpty());
    auto* main=qobject_cast<QWebEngineView*>(browser.findChild<QTabWidget*>(QStringLiteral("topographicTabs"))->widget(0));
    auto calls=std::make_shared<QVariantMap>();
    main->page()->runJavaScript(QStringLiteral("({application:window.orderCalls,submit:window.submitCalls,sheets:window.orderItems.length})"),
      [calls](const QVariant& value){*calls=value.toMap();});
    QTRY_VERIFY_WITH_TIMEOUT(!calls->isEmpty(),5000);
    QCOMPARE(calls->value(QStringLiteral("application")).toInt(),1);
    QCOMPARE(calls->value(QStringLiteral("submit")).toInt(),1);
    QCOMPARE(calls->value(QStringLiteral("sheets")).toInt(),2);
    if(cancelFirst) {
      QTest::keyClick(&browser,Qt::Key_Escape);
      QTRY_COMPARE_WITH_TIMEOUT(rows->topLevelItem(0)->text(2),QStringLiteral("취소됨"),10000);
      QTest::qWait(900);
      QCOMPARE(server.sdkRequests.size(),1);QVERIFY(received.isEmpty());QVERIFY(!nextRequestBeforeCompletion);
      return;
    }
    QVERIFY(server.finishTransfer());
    QTRY_VERIFY_WITH_TIMEOUT(server.sdkXmlSocket && server.sdkXmlName==QStringLiteral("terrain_378044.xml"),10000);
    QCOMPARE(server.sdkRequests.size(),2);QVERIFY(received.isEmpty());
    QTest::qWait(500);QCOMPARE(server.sdkRequests.size(),2);QVERIFY(received.isEmpty());
    QVERIFY(server.finishSdkXml());
    QTRY_COMPARE_WITH_TIMEOUT(received.size(),1,10000);
    QCOMPARE(received.first().at(1).toJsonObject().value(QStringLiteral("num")).toString(),QStringLiteral("378044"));
    QTRY_COMPARE_WITH_TIMEOUT(server.activeSdkFile,QStringLiteral("terrain_378043.dxf"),10000);
    QCOMPARE(server.sdkRequests.size(),3);QCOMPARE(received.size(),1);
    QVERIFY(server.finishTransfer());
    QTRY_VERIFY_WITH_TIMEOUT(server.sdkXmlSocket && server.sdkXmlName==QStringLiteral("terrain_378043.xml"),10000);
    QCOMPARE(received.size(),1);QVERIFY(server.finishSdkXml());
    QTRY_COMPARE_WITH_TIMEOUT(received.size(),2,10000);
    QCOMPARE(server.sdkRequests,QStringList({QStringLiteral("terrain_378044.dxf"),QStringLiteral("terrain_378044.xml"),
      QStringLiteral("terrain_378043.dxf"),QStringLiteral("terrain_378043.xml")}));
    QVERIFY(!nextRequestBeforeCompletion);
    for(const auto& event:received) {
      const auto path=event.first().toString();const auto number=event.at(1).toJsonObject().value(QStringLiteral("num")).toString();
      QCOMPARE(QFileInfo(path).size(),LocalDownloadServer::transferSize);
      QVERIFY(path.contains(number));
      const auto xml=QFileInfo(path).dir().filePath(QStringLiteral("terrain_%1.xml").arg(number));
      QCOMPARE(QFileInfo(xml).size(),qint64(16));
    }
    QVERIFY(QFileInfo(received[0].first().toString()).absolutePath()!=QFileInfo(received[1].first().toString()).absolutePath());
    QVERIFY(browser.findChild<QWidget*>(QStringLiteral("topographicOfficialDetails"))->isHidden());
    browser.stopAutomatic();
  }

  void livePublicPortalPopupAndSsoParentNavigation() {
    if (qEnvironmentVariableIntValue("KA_HGIS_LIVE_NGII_TEST") != 1)
      QSKIP("Official public-page network check is opt-in: KA_HGIS_LIVE_NGII_TEST=1");
    QTemporaryDir files; QVERIFY(files.isValid());
    const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR",
        QDir::currentPath() + QStringLiteral("/qa/ngii-browser"));
    QVERIFY(QDir().mkpath(output));
    KaTopographicBrowser browser(nullptr, files.path());
    auto* tabs = browser.findChild<QTabWidget*>(QStringLiteral("topographicTabs")); QVERIFY(tabs);
    auto* main = qobject_cast<QWebEngineView*>(tabs->widget(0)); QVERIFY(main);
    QSignalSpy portalLoaded(main, &QWebEngineView::loadFinished);
    browser.setCompactMode(true);
    browser.navigate(QUrl(QStringLiteral("https://map.ngii.go.kr/ms/map/NlipMap.do?tabGb=total")));
    browser.show();
    QTRY_VERIFY_WITH_TIMEOUT(!portalLoaded.isEmpty(), 45000);
    QVERIFY2(portalLoaded.last().at(0).toBool(), "The official portal did not load");
    QVERIFY(browser.grab().save(QDir(output).filePath(QStringLiteral("ngii-public-portal.png"))));

    main->page()->runJavaScript(QStringLiteral("window.open('/mn/loginPopup.do', 'ngii-public-login');"));
    QTRY_COMPARE_WITH_TIMEOUT(tabs->count(), 2, 15000);
    auto* popup = qobject_cast<QWebEngineView*>(tabs->widget(1)); QVERIFY(popup);
    QCOMPARE(popup->page()->profile(), main->page()->profile());
    QVERIFY(popup->page()->profile()->isOffTheRecord());
    const auto buttonReady = std::make_shared<bool>(false);
    auto requestButtonState = [popup, buttonReady] {
      popup->page()->runJavaScript(QStringLiteral("!!document.getElementById('ssoLogin')"),
                                  [buttonReady](const QVariant& value) { *buttonReady = value.toBool(); });
      return *buttonReady;
    };
    QTRY_VERIFY_WITH_TIMEOUT(requestButtonState(), 30000);
    QVERIFY(browser.grab().save(QDir(output).filePath(QStringLiteral("ngii-public-login-popup.png"))));

    // This clicks only the public login-method chooser, never credentials,
    // consent, an application, or a download on the live service.
    const auto ssoNavigation = std::make_shared<bool>(false);
    QSignalSpy ssoLoaded(main, &QWebEngineView::loadFinished);
    const auto connection = connect(main, &QWebEngineView::urlChanged, &browser,
        [ssoNavigation](const QUrl& url) {
      if (url.host() == QLatin1String("map.ngii.go.kr") &&
          url.path() == QLatin1String("/anyid/common/login.do")) *ssoNavigation = true;
    });
    popup->page()->runJavaScript(QStringLiteral("document.getElementById('ssoLogin').click();"));
    QTRY_VERIFY_WITH_TIMEOUT(*ssoNavigation, 30000);
    QTRY_COMPARE_WITH_TIMEOUT(tabs->count(), 1, 15000);
    QTRY_VERIFY_WITH_TIMEOUT(!ssoLoaded.isEmpty() && ssoLoaded.last().at(0).toBool(), 45000);
    disconnect(connection);
    QVERIFY(browser.grab().save(QDir(output).filePath(QStringLiteral("ngii-public-sso-parent.png"))));

    // Exercise the product adapter in the same hidden view as the field app.
    // Stop at the public ID form: never supply credentials to this live test.
    browser.setCompactMode(true);
    QFile adapter(QStringLiteral("data/providers/ngii-topographic.js"));QVERIFY(adapter.open(QIODevice::ReadOnly));
    const QString adapterSource=QString::fromUtf8(adapter.readAll());
    const QString loginScript=adapterSource+QStringLiteral("({command:'login',generation:'public-test',loginAttempt:'1'});");
    auto idFormReady=std::make_shared<bool>(false);
    auto publicState=std::make_shared<QVariantMap>();
    auto pollPublicLogin=[&] {
      QWebEngineView* active=main;
      for(int i=0;i<tabs->count();++i)if(auto* candidate=qobject_cast<QWebEngineView*>(tabs->widget(i))) {
        if(candidate->url().path()==QLatin1String("/member/login.do"))active=candidate;
      }
      active->page()->runJavaScript(QStringLiteral(R"JS((()=>{
        const container=document.getElementById('loginContainer');
        const module=document.querySelector('x-anyidc');
        return {path:location.pathname,containerVisibility:container?getComputedStyle(container).visibility:'absent',
          moduleHeight:module?module.getBoundingClientRect().height:-1,idMethod:typeof window.testFnLogin,
          idForm:!!document.querySelector('#formLogin #mem_pw')};
      })())JS"),[publicState,idFormReady](const QVariant& value){
        *publicState=value.toMap();*idFormReady=publicState->value(QStringLiteral("idForm")).toBool();
      });
      if(active->url().path().startsWith(QLatin1String("/anyid/common/")))active->page()->runJavaScript(loginScript);
      return *idFormReady;
    };
    QElapsedTimer publicClock;publicClock.start();
    while(!pollPublicLogin() && publicClock.elapsed()<30000)QTest::qWait(500);
    qInfo().noquote()<<"Public login structure:"<<QJsonDocument(QJsonObject::fromVariantMap(*publicState)).toJson(QJsonDocument::Compact);
    QVERIFY2(*idFormReady,"Hidden official authentication flow did not reach the ID/password form");
    // Show only our separate public QA form to capture the verified destination.
    browser.setCompactMode(false);
    auto* formView=qobject_cast<QWebEngineView*>(tabs->currentWidget());QVERIFY(formView);
    const auto formPainted=[formView]{
      const QImage pixels=formView->grab().toImage();int logoPixels=0;
      for(int y=0;y<pixels.height();++y)for(int x=0;x<pixels.width();++x) {
        const auto color=pixels.pixelColor(x,y);
        if(color.saturation()>120 && color.value()>80)++logoPixels;
      }
      return logoPixels>200;
    };
    QTRY_VERIFY_WITH_TIMEOUT(formPainted(),15000);
    QVERIFY(browser.grab().save(QDir(output).filePath(QStringLiteral("ngii-public-id-form.png"))));
    browser.setCompactMode(true);
    // Additional opt-in checks the user's configured account once. Credentials
    // stay in memory, are never printed, and are never part of a capture.
    if(qEnvironmentVariableIntValue("KA_HGIS_LIVE_NGII_AUTH_TEST")==1) {
      const QString accountFile=qEnvironmentVariable("KA_HGIS_NGII_ACCOUNT_FILE");
      QVERIFY2(QFileInfo::exists(accountFile),"An existing account settings file is required for the opt-in authentication check");
      QSettings account(accountFile,QSettings::IniFormat);account.setFallbacksEnabled(false);
      QJsonObject input{{QStringLiteral("command"),QStringLiteral("login")},
        {QStringLiteral("generation"),QStringLiteral("public-test")},{QStringLiteral("loginAttempt"),QStringLiteral("1")},
        {QStringLiteral("loginId"),account.value(QStringLiteral("ngii/username")).toString()},
        {QStringLiteral("loginPassword"),account.value(QStringLiteral("ngii/password")).toString()}};
      QVERIFY2(!input.value(QStringLiteral("loginId")).toString().isEmpty() &&
        !input.value(QStringLiteral("loginPassword")).toString().isEmpty(),"The configured account is incomplete");
      QWebEngineView* id=nullptr;
      for(int i=0;i<tabs->count();++i)if(auto* candidate=qobject_cast<QWebEngineView*>(tabs->widget(i))) {
        const QUrl url=candidate->url();
        if(url.scheme()==QLatin1String("https") && url.port(-1)==-1 && url.path()==QLatin1String("/member/login.do") &&
          (url.host()==QLatin1String("ngii.go.kr") || url.host()==QLatin1String("www.ngii.go.kr")))id=candidate;
      }
      QVERIFY(id);
      id->page()->runJavaScript(adapterSource+QLatin1Char('(')+QString::fromUtf8(QJsonDocument(input).toJson(QJsonDocument::Compact))+QStringLiteral(");"));
      input={};
      auto authenticated=std::make_shared<bool>(false);
      auto checking=std::make_shared<bool>(false);
      auto checkSession=[&]{
        if(!*checking && main->url().host()==QLatin1String("map.ngii.go.kr")) {
          *checking=true;
          main->page()->runJavaScript(QStringLiteral(R"JS((()=>{
            if(!window.__qaSessionChecking){window.__qaSessionChecking=true;
              fetch('/mn/loginCheck.do',{method:'POST',credentials:'same-origin',signal:AbortSignal.timeout(10000)})
                .then(r=>r.json()).then(r=>{window.__qaSessionAuthenticated=r.isLogin===true;})
                .catch(()=>{}).finally(()=>{window.__qaSessionChecking=false;});}
            return window.__qaSessionAuthenticated===true;
          })())JS"),[checking,authenticated](const QVariant& value){*checking=false;*authenticated=value.toBool();});
        }
        return *authenticated && main->url().path()==QLatin1String("/ms/map/NlipMap.do") && tabs->count()==1;
      };
      QElapsedTimer authClock;authClock.start();
      while(!checkSession() && authClock.elapsed()<45000)QTest::qWait(1000);
      qInfo().noquote()<<"Configured authentication:"<<*authenticated<<"portal path:"<<main->url().path()<<"tabs:"<<tabs->count();
      QVERIFY2(*authenticated,"Configured account authentication did not produce an authenticated portal session");
      QCOMPARE(main->url().path(),QStringLiteral("/ms/map/NlipMap.do"));
    }
  }

  void liveAutomaticPollingAuthenticatesAndReturnsToMap() {
    if(qEnvironmentVariableIntValue("KA_HGIS_LIVE_NGII_AUTH_TEST")!=1)
      QSKIP("Real account automation is opt-in and never runs in the standard test suite");
    const QString accountPath=qEnvironmentVariable("KA_HGIS_NGII_ACCOUNT_FILE");
    QVERIFY2(QFileInfo::exists(accountPath),"An existing account settings file is required");
    QSettings account(accountPath,QSettings::IniFormat);account.setFallbacksEnabled(false);
    bool visitedAuthentication=false,returnedToMap=false;
    QTemporaryDir files;KaTopographicBrowser browser(nullptr,files.path());
    browser.setCredentials(account.value(QStringLiteral("ngii/username")).toString(),account.value(QStringLiteral("ngii/password")).toString());
    browser.setCompactMode(true);browser.show();
    auto* tabs=browser.findChild<QTabWidget*>(QStringLiteral("topographicTabs"));QVERIFY(tabs);
    auto* main=qobject_cast<QWebEngineView*>(tabs->widget(0));QVERIFY(main);
    QSignalSpy attention(&browser,&KaTopographicBrowser::automationNeedsInput);
    const auto observeLoads=[tabs,&browser]{
      for(int i=0;i<tabs->count();++i)if(auto* view=qobject_cast<QWebEngineView*>(tabs->widget(i))) {
        if(view->property("qaObservedLoads").toBool())continue;
        view->setProperty("qaObservedLoads",true);
        connect(view->page(),&QWebEnginePage::loadingChanged,&browser,[](const QWebEngineLoadingInfo& info){
          qInfo().noquote()<<"Public page load:"<<info.url().host()<<info.url().path()
            <<"status"<<static_cast<int>(info.status())<<"code"<<info.errorCode()<<"domain"<<static_cast<int>(info.errorDomain());
        });
      }
    };
    connect(tabs,&QTabWidget::currentChanged,&browser,[observeLoads](int){observeLoads();});observeLoads();
    connect(main,&QWebEngineView::urlChanged,&browser,[&](const QUrl& url){
      if(url.host()==QLatin1String("map.ngii.go.kr") && url.path().startsWith(QLatin1String("/anyid/")))visitedAuthentication=true;
      if(visitedAuthentication && url.host()==QLatin1String("map.ngii.go.kr") && url.path()==QLatin1String("/ms/map/NlipMap.do")) {
        returnedToMap=true;
        // Authentication boundary only; never submit an application in this test.
        browser.stopAutomatic();
      }
    });
    browser.prepareSheets(896113.647843102,1498729.80739648,10000.,
      {QStringLiteral("336062"),QStringLiteral("336063"),QStringLiteral("336064")});
    QTRY_VERIFY_WITH_TIMEOUT(returnedToMap || !attention.isEmpty(),90000);
    if(!returnedToMap) {
      QVERIFY(saveCompactCapture(browser,QStringLiteral("live-login-failed.png")));
      if(!attention.isEmpty())qInfo().noquote()<<"Automatic login failed:"<<attention.last().first().toString();
    }
    QVERIFY2(returnedToMap,"The real automatic poller did not return from authentication to the map");
    auto authenticated=std::make_shared<bool>(false);
    auto checking=std::make_shared<bool>(false);
    const auto verifiedSession=[&]{
      if(!*checking) {
        *checking=true;
        main->page()->runJavaScript(QStringLiteral(R"JS((()=>{
          if(document.readyState!=='complete')return false;
          if(!window.__qaSessionRequest){window.__qaSessionRequest=true;
            fetch('/mn/loginCheck.do',{method:'POST',credentials:'same-origin',signal:AbortSignal.timeout(10000)})
              .then(r=>r.json()).then(r=>{window.__qaAuthenticated=r.isLogin===true;})
              .catch(()=>{window.__qaAuthenticated=false;});}
          return window.__qaAuthenticated===true;
        })())JS"),[checking,authenticated](const QVariant& value){*checking=false;*authenticated=value.toBool();});
      }
      return *authenticated;
    };
    QTRY_VERIFY_WITH_TIMEOUT(verifiedSession(),20000);
    QTRY_COMPARE_WITH_TIMEOUT(tabs->count(),1,10000);
    qInfo()<<"Real automatic poller: authenticated portal session, returned to map, one tab; no application submitted";
  }

  void liveAutomaticDownloadProducesOfficialSheet() {
    if(qEnvironmentVariableIntValue("KA_HGIS_LIVE_NGII_DOWNLOAD_TEST")!=1)
      QSKIP("Real official application/download is opt-in");
    const QString accountPath=qEnvironmentVariable("KA_HGIS_NGII_ACCOUNT_FILE");
    QVERIFY(QFileInfo::exists(accountPath));
    QSettings account(accountPath,QSettings::IniFormat);account.setFallbacksEnabled(false);
    const QString output=qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");QVERIFY(!output.isEmpty());
    QVERIFY(QDir().mkpath(output));
    QTemporaryDir files(output+QStringLiteral("/received-XXXXXX"));QVERIFY(files.isValid());files.setAutoRemove(false);
    KaTopographicBrowser browser(nullptr,files.path());browser.setCompactMode(true);browser.show();
    browser.setCredentials(account.value(QStringLiteral("ngii/username")).toString(),account.value(QStringLiteral("ngii/password")).toString());
    auto* observedTabs=browser.findChild<QTabWidget*>(QStringLiteral("topographicTabs"));QVERIFY(observedTabs);
    const auto observePages=[observedTabs,&browser,output]{
      for(int i=0;i<observedTabs->count();++i)if(auto* view=qobject_cast<QWebEngineView*>(observedTabs->widget(i))) {
        if(view->property("qaTransferObserved").toBool())continue;view->setProperty("qaTransferObserved",true);
        connect(view->page(),&QWebEnginePage::loadingChanged,&browser,[output](const QWebEngineLoadingInfo& info){
          QFile log(output+QStringLiteral("/page-events.jsonl"));if(log.open(QIODevice::Append))log.write(QJsonDocument(QJsonObject{
            {QStringLiteral("path"),info.url().host()+info.url().path()},{QStringLiteral("status"),static_cast<int>(info.status())},
            {QStringLiteral("code"),info.errorCode()}}).toJson(QJsonDocument::Compact)+'\n');
        });
        connect(view,&QWebEngineView::loadFinished,&browser,[view,output](bool){
          if(view->url().path()!=QLatin1String("/pd/purchs/download.do") &&
              view->url().path()!=QLatin1String("/nlippd/pd/innorix/innorixdownLoad.do"))return;
          view->page()->runJavaScript(QStringLiteral(R"JS((()=>({
            ready:document.readyState,downForm:window.name==='downForm',scripts:Array.from(document.scripts).filter(s=>s.src).map(s=>{const u=new URL(s.src);return u.origin+u.pathname;}),
            globals:Object.keys(window).filter(k=>/^[A-Za-z_$][A-Za-z0-9_$]{0,79}$/.test(k)&&/innorix|inno|download|control/i.test(k))
          }))())JS"),[output](const QVariant& value){QFile log(output+QStringLiteral("/ready-transfer.json"));if(log.open(QIODevice::WriteOnly))log.write(QJsonDocument::fromVariant(value).toJson());});
        });
      }
    };
    connect(observedTabs,&QTabWidget::currentChanged,&browser,[observePages](int){observePages();});observePages();
    QSignalSpy received(&browser,&KaTopographicBrowser::sheetDownloaded);
    QSignalSpy attention(&browser,&KaTopographicBrowser::automationNeedsInput);
    connect(&browser,&KaTopographicBrowser::automationNeedsInput,&browser,[&browser](const QString&){
      for(auto* view:browser.findChildren<QWebEngineView*>())if(view->url().path()==QLatin1String("/ms/map/NlipMap.do"))
        view->page()->runJavaScript(QStringLiteral(R"JS((()=>({
          selects:Array.from(document.querySelectorAll('.popup.mapApplication.online select')).map(e=>({id:e.id,name:e.name,value:e.value,options:Array.from(e.options).map(o=>({value:o.value,text:o.textContent}))}))
        }))())JS"),[](const QVariant& value){qInfo().noquote()<<"Public purpose fields:"<<QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact);});
    });
    browser.prepareSheets(896113.647843102,1498729.80739648,10000.,
      {QStringLiteral("336062"),QStringLiteral("336063"),QStringLiteral("336064")});
    auto* currentStage=browser.findChild<QLabel*>(QStringLiteral("topographicCurrentStage"));QVERIFY(currentStage);
    QElapsedTimer downloadClock;downloadClock.start();
    // An application field may still be loading when an input message arrives.
    // Only the poller's final failed state stops this live workflow early.
    while(received.size()<3 && !currentStage->text().startsWith(QStringLiteral("다운로드 실패")) && downloadClock.elapsed()<180000)QTest::qWait(200);
    if(received.size()<3) {
      QVERIFY(saveCompactCapture(browser,QStringLiteral("live-transfer-failed.png")));
      if(!attention.isEmpty())qInfo().noquote()<<"Official automatic download stopped:"<<attention.last().first().toString();
      auto* tabs=browser.findChild<QTabWidget*>(QStringLiteral("topographicTabs"));QVERIFY(tabs);
      for(int i=0;i<tabs->count();++i)if(auto* view=qobject_cast<QWebEngineView*>(tabs->widget(i))) {
        if(view->url().path()!=QLatin1String("/pd/purchs/download.do") &&
            view->url().path()!=QLatin1String("/nlippd/pd/innorix/innorixdownLoad.do"))continue;
        auto data=std::make_shared<QVariantMap>();
        const auto ready=[&]{view->page()->runJavaScript(QStringLiteral(R"JS((()=>({
          ready:document.readyState==='complete',scripts:Array.from(document.scripts).filter(s=>s.src).map(s=>{const u=new URL(s.src);return u.origin+u.pathname;}),
          globals:Object.keys(window).filter(k=>/^[A-Za-z_$][A-Za-z0-9_$]{0,79}$/.test(k)&&/innorix|inno|download|control/i.test(k))
        }))())JS"),[data](const QVariant& value){*data=value.toMap();});return data->value(QStringLiteral("ready")).toBool();};
        QElapsedTimer pageClock;pageClock.start();
        while(!ready() && pageClock.elapsed()<15000)QTest::qWait(200);
        qInfo().noquote()<<"Ready transfer contract:"<<QJsonDocument::fromVariant(*data).toJson(QJsonDocument::Compact);
      }
    }
    QJsonArray manifest;
    QSet<QString> numbers;
    for(const auto& result:received) {
      const QString path=result.at(0).toString();
      const auto record=qvariant_cast<QJsonObject>(result.at(1));
      QVERIFY(QFileInfo(path).size()>0);
      manifest.append(QJsonObject{{QStringLiteral("path"),path},{QStringLiteral("record"),record}});
      numbers.insert(record.value(QStringLiteral("num")).toString());
    }
    QFile evidence(output+QStringLiteral("/received-all.json"));QVERIFY(evidence.open(QIODevice::WriteOnly));
    evidence.write(QJsonDocument(manifest).toJson());
    QCOMPARE(numbers,QSet<QString>({QStringLiteral("336062"),QStringLiteral("336063"),QStringLiteral("336064")}));
    QVERIFY(saveCompactCapture(browser,QStringLiteral("live-transfer-complete.png")));
    qInfo()<<"Official sheets received:"<<received.size()<<"elapsed ms:"<<downloadClock.elapsed();
    browser.stopAutomatic();
  }

  void popupSharesEphemeralLoginSession() {
    LocalDownloadServer server; QVERIFY(server.listen(QHostAddress::LocalHost));
    QTemporaryDir files; QVERIFY(files.isValid());
    KaTopographicBrowser browser(nullptr, files.path());
    browser.navigate(server.url()); browser.show();
    auto* tabs = browser.findChild<QTabWidget*>(QStringLiteral("topographicTabs")); QVERIFY(tabs);
    auto* main = qobject_cast<QWebEngineView*>(tabs->widget(0)); QVERIFY(main);
    QTRY_COMPARE_WITH_TIMEOUT(main->title(), QStringLiteral("local fixture"), 15000);
    QVERIFY(main->page()->profile()->isOffTheRecord());
    main->page()->runJavaScript(QStringLiteral("window.open('/popup', 'test-popup');"));
    QTRY_COMPARE_WITH_TIMEOUT(tabs->count(), 2, 15000);
    auto* popup = qobject_cast<QWebEngineView*>(tabs->widget(1)); QVERIFY(popup);
    QCOMPARE(popup->page()->profile(), main->page()->profile());
    QTRY_COMPARE_WITH_TIMEOUT(popup->title(), QStringLiteral("popup"), 15000);
    const auto cookie = std::make_shared<QString>();
    popup->page()->runJavaScript(QStringLiteral("document.cookie"), [cookie](const QVariant& value) { *cookie = value.toString(); });
    QTRY_VERIFY_WITH_TIMEOUT(cookie->contains(QStringLiteral("fixture=shared")), 15000);
    popup->page()->runJavaScript(QStringLiteral("window.opener.document.title='opener-linked'; window.close();"));
    QTRY_COMPARE_WITH_TIMEOUT(main->title(), QStringLiteral("opener-linked"), 15000);
    QTRY_COMPARE_WITH_TIMEOUT(tabs->count(), 1, 15000);
  }

  void completedAttachmentHasUniquePathAndCancelDoesNotImport() {
    LocalDownloadServer server; QVERIFY(server.listen(QHostAddress::LocalHost));
    QTemporaryDir files; QVERIFY(files.isValid());
    KaTopographicBrowser browser(nullptr, files.path());
    browser.navigate(server.url()); browser.show();
    auto* view = browser.findChild<QWebEngineView*>(); QVERIFY(view);
    QTRY_COMPARE_WITH_TIMEOUT(view->title(), QStringLiteral("local fixture"), 15000);
    QSignalSpy completed(&browser, &KaTopographicBrowser::fileDownloaded);
    view->page()->download(server.url(QStringLiteral("/file.zip")));
    QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 1, 15000);
    const QString firstPath = completed.at(0).at(0).toString();
    QVERIFY(QDir::cleanPath(firstPath).startsWith(QDir::cleanPath(files.path()) + QLatin1Char('/')));
    QFile first(firstPath); QVERIFY(first.open(QIODevice::ReadOnly));
    QCOMPARE(first.readAll(), QByteArray("PK-fixture-complete")); first.close();
    view->page()->download(server.url(QStringLiteral("/file.zip")));
    QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 2, 15000);
    QVERIFY(completed.at(1).at(0).toString() != firstPath);
    view->page()->download(server.url(QStringLiteral("/slow.zip")));
    auto* downloads = browser.findChild<QTreeWidget*>(QStringLiteral("topographicDownloads")); QVERIFY(downloads);
    QTRY_COMPARE_WITH_TIMEOUT(downloads->topLevelItemCount(), 3, 15000);
    auto* row = downloads->topLevelItem(2);
    auto* cancel = qobject_cast<QPushButton*>(downloads->itemWidget(row, 3)); QVERIFY(cancel);
    cancel->click();
    QTRY_COMPARE_WITH_TIMEOUT(row->text(2), QStringLiteral("취소됨"), 15000);
    QCOMPARE(completed.size(), 2);
  }
};

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  const bool manual = app.arguments().contains(QStringLiteral("--manual-ngii"));
  QStandardPaths::setTestModeEnabled(true);
  app.setOrganizationName(QStringLiteral("ka-hgis-qa"));
  app.setApplicationName(manual ? QStringLiteral("ka-hgis-ngii-browser-manual")
                                : QStringLiteral("ka-hgis-ngii-browser-tests"));
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                    QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/qa-settings"));
  if (manual) {
    KaTopographicBrowser browser;
    browser.setWindowTitle(QStringLiteral("수치지형도 내려받기 — 별도 QA 창"));
    browser.show();
    // No timer, auto-close, scripted login, or automatic capture in handoff mode.
    return app.exec();
  }
  TopographicBrowserTest test;
  return QTest::qExec(&test, argc, argv);
}
#include "test_topographic_browser.moc"
