#include <QtTest>
#include <QApplication>
#include <QMainWindow>
#include <QPushButton>
#include <QStatusBar>
#include <QScreen>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <QWebEngineProfile>
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineView>
#include <cpl_conv.h>

#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QTabWidget>

#include "app/KaHeritageBrowser.h"
#include "core/HeritageRecentDownloads.h"
#include "core/KaPortableRuntime.h"
#include "core/TopographicArchive.h"

namespace {
class OfflineHeritageRequests final : public QWebEngineUrlRequestInterceptor {
public:
  explicit OfflineHeritageRequests(quint16 port, QObject* parent)
      : QWebEngineUrlRequestInterceptor(parent), m_port(port) {}

  void interceptRequest(QWebEngineUrlRequestInfo& info) override {
    const QUrl url = info.requestUrl();
    if (url.scheme() == QLatin1String("about") || url.scheme() == QLatin1String("data")) return;
    if (m_port != 0 && url.scheme() == QLatin1String("http") &&
        url.host() == QLatin1String("127.0.0.1") && url.port() == m_port) return;
    if (m_port != 0 && url == HeritageIntranetFlow::loginUrl()) {
      info.redirect(QUrl(QStringLiteral("http://127.0.0.1:%1/offline-login").arg(m_port)));
      return;
    }
    info.block(true);
  }

private:
  const quint16 m_port;
};
}  // namespace

class HeritageDownloadRetryTest : public QObject {
  Q_OBJECT
private:
  static void restrictRequests(KaHeritageBrowser& browser, quint16 port = 0) {
    if (!browser.m_profile)
      browser.m_profile = new QWebEngineProfile(QStringLiteral("ka-heritage"), &browser);
    browser.m_profile->setUrlRequestInterceptor(new OfflineHeritageRequests(port, browser.m_profile));
    browser.ensureProfile();
  }

private slots:
  void closingMainWindowDuringDownloadDoesNotCrash() {
    auto* window = new QMainWindow;
    auto* browser = new KaHeritageBrowser(window);
    connect(browser, &KaHeritageBrowser::stageChanged, window,
            [window](HeritageStage, const QString&) { window->statusBar()->showMessage(QStringLiteral("받기")); });
    browser->m_running = true;
    delete window;
  }

  void detailsFitAvailableScreen() {
    KaHeritageBrowser browser;
    browser.show();
    auto* details = browser.findChild<QPushButton*>(QStringLiteral("heritageDetails"));
    QVERIFY(details);
    for (bool expanded : {true, false, true}) {
      details->setChecked(expanded);
      QCoreApplication::processEvents();
      QVERIFY(browser.screen()->availableGeometry().contains(browser.frameGeometry()));
      QVERIFY(browser.rect().contains(QRect(details->mapTo(&browser, QPoint()), details->size())));
    }
  }
  void completedResponseIsValidated_data() {
    QTest::addColumn<bool>("empty");
    QTest::addColumn<bool>("validArchive");
    QTest::newRow("missing-zip-directory-reopens-session") << false << false;
    QTest::newRow("empty-response-reopens-session") << true << false;
    QTest::newRow("valid-response-completes-without-reopening") << false << true;
  }

  void completedResponseIsValidated() {
    QFETCH(bool, empty);
    QFETCH(bool, validArchive);
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString source = temp.filePath(QStringLiteral("valid.zip"));
    void* zip = CPLCreateZip(source.toUtf8().constData(), nullptr);
    QVERIFY(zip);
    QCOMPARE(CPLCreateFileInZip(zip, "fixture.txt", nullptr), CE_None);
    QCOMPARE(CPLWriteFileInZip(zip, "complete content", 16), CE_None);
    QCOMPARE(CPLCloseFileInZip(zip), CE_None);
    QCOMPARE(CPLCloseZip(zip), CE_None);
    QFile input(source);
    QVERIFY(input.open(QIODevice::ReadOnly));
    const QByteArray valid = input.readAll();
    const auto central = valid.indexOf(QByteArray::fromHex("504b0102"));
    QVERIFY(central > 0);
    const QByteArray body = validArchive ? valid : empty ? QByteArray() : valid.left(central);

    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    int requests = 0;
    int loginRequests = 0;
    connect(&server, &QTcpServer::newConnection, &server, [&] {
      while (auto* socket = server.nextPendingConnection()) {
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
          QByteArray request = socket->property("request").toByteArray() + socket->readAll();
          socket->setProperty("request", request);
          if (!request.contains("\r\n\r\n") || socket->property("sent").toBool()) return;
          socket->setProperty("sent", true);
          if (request.startsWith("GET /offline-login ")) {
            ++loginRequests;
            const QByteArray page = "<!doctype html><title>Offline login fixture</title>";
            socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: "
                          + QByteArray::number(page.size()) + "\r\nConnection: close\r\n\r\n" + page);
          } else if (!request.startsWith("GET /archive.zip")) {
            socket->write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
          } else {
            ++requests;
            socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/zip\r\n"
                          "Content-Disposition: attachment; filename=heritage.zip\r\nContent-Length: "
                          + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
          }
          socket->disconnectFromHost();
        });
      }
    });

    KaHeritageBrowser browser;
    restrictRequests(browser, server.serverPort());
    browser.setDownloadRoot(temp.filePath(QStringLiteral("downloads")));
    browser.setTarget(QStringLiteral("경상북도"), QStringLiteral("포항시"),
                      {HeritageDataset::AlterationStandard, HeritageDataset::DesignatedHeritage});
    browser.m_datasetIndex = 1;
    browser.m_running = true;
    browser.m_stage = HeritageStage::Download;
    auto* view = browser.addPage();
    QSignalSpy loaded(view, &QWebEngineView::loadFinished);
    view->setHtml(QStringLiteral(
        "<div id='tabContentDiv'><form id='searchForm'>"
        "<input id='mode' value='S'>"
        "<select id='codedetaCd0' name='codedetaCd'><option value='47'>경상북도</option></select>"
        "<select id='codeCdSg0' name='codeCdSg'><option value='4711'>포항시</option></select>"
        "<button type='button' onclick=\"location.href='/archive.zip?attempt='+Date.now()\">전체다운로드</button>"
        "</form></div>"), QUrl(QStringLiteral("http://127.0.0.1:%1/fixture").arg(server.serverPort())));
    // The first row starts the WebEngine render process. Under parallel ctest that cold
    // start alone can pass 10 s, so wait on the recorded signal with a wider margin.
    QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 30000);
    browser.m_pageReady = true;
    browser.m_settle = 0;
    QSignalSpy failed(&browser, &KaHeritageBrowser::failed);
    QSignalSpy done(&browser, &KaHeritageBrowser::allFinished);
    QSignalSpy downloaded(&browser, &KaHeritageBrowser::fileDownloaded);
    int accepted = 0;
    connect(&browser, &KaHeritageBrowser::datasetReady, &browser,
            [&](HeritageDataset, const QStringList& paths) {
      const auto result = TopographicArchive::prepare(paths.first(), temp.filePath(QStringLiteral("library")));
      if (!result.error.isEmpty()) browser.rejectDataset(result.error, result.invalidArchive);
      else ++accepted;
    });
    // Validate download completion separately from reopening the failed session.
    // Stop at Login: this fixture never evaluates login scripts or reads credentials.
    QTimer drive;
    connect(&drive, &QTimer::timeout, &browser, [&] {
      if (!browser.m_downloadRetryReason.isEmpty()) {
        QCOMPARE(browser.m_datasetIndex, 1);
        QCOMPARE(accepted, 0);
        QCOMPARE(done.count(), 0);
        QCOMPARE(downloaded.count(), 0);
        browser.m_downloadRetryDelayMs = 0;
      }
      browser.runStage();
      if (browser.stage() == HeritageStage::Login) drive.stop();
    });
    drive.start(20);
    QTRY_VERIFY_WITH_TIMEOUT(done.count() == 1 || loginRequests == 1 || failed.count() > 0, 20000);
    drive.stop();
    QCOMPARE(failed.count(), 0);
    QCOMPARE(requests, 1);
    if (!validArchive) {
      QCOMPARE(browser.stage(), HeritageStage::Login);
      QCOMPARE(browser.m_datasetIndex, 1);
      QCOMPARE(loginRequests, 1);
      QCOMPARE(accepted, 0);
      QCOMPARE(done.count(), 0);
      QCOMPARE(downloaded.count(), 0);
      return;
    }
    QCOMPARE(loginRequests, 0);
    QCOMPARE(done.count(), 1);
    QCOMPARE(accepted, 1);
    QCOMPARE(downloaded.count(), 1);
    QFile received(downloaded.first().first().toString());
    QVERIFY(received.open(QIODevice::ReadOnly));
    QCOMPARE(received.readAll(), valid);
  }

  void stopCancelsPendingRetry() {
    KaHeritageBrowser browser;
    restrictRequests(browser);
    browser.setTarget(QStringLiteral("경상북도"), QStringLiteral("포항시"),
                      {HeritageDataset::DesignatedHeritage});
    browser.m_running = true;
    browser.m_stage = HeritageStage::Download;
    browser.rejectDataset(QStringLiteral("손상된 ZIP"), true);
    QVERIFY(!browser.m_downloadRetryReason.isEmpty());
    browser.stop();
    browser.m_downloadRetryDelayMs = 0;
    browser.runStage();
    QCOMPARE(browser.stage(), HeritageStage::Idle);
    QVERIFY(browser.m_downloadMode.isEmpty());
    QCOMPARE(browser.m_datasetIndex, 0);
  }

  // F120 + F174: ticked neighbours run one 시·군 after another; with 「최근 받은 자료 다시
  // 쓰기」 chosen, recorded downloads are loaded without contacting the site.
  void followUpCitiesRunOneAfterAnother() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString rootA = QDir::cleanPath(temp.filePath(QStringLiteral("주변유적/경상북도 안동시/원본")));
    const HeritageCity yecheon{QStringLiteral("경상북도"), QStringLiteral("예천군")};
    const QString rootB = HeritageFetchPlanning::siblingRoot(rootA, yecheon);
    auto writeZip = [](const QString& path) {
      QDir().mkpath(QFileInfo(path).absolutePath());
      QFile f(path);
      return f.open(QIODevice::WriteOnly) && f.write("PK-fixture") == 10;
    };
    const QString zipA = QDir(rootA).filePath(QStringLiteral("a/지정유산.zip"));
    const QString zipB = QDir(rootB).filePath(QStringLiteral("b/지정유산.zip"));
    QVERIFY(writeZip(zipA));
    QVERIFY(writeZip(zipB));
    QVERIFY(HeritageRecentDownloads::record(rootA, HeritageDataset::DesignatedHeritage, {zipA}));
    QVERIFY(HeritageRecentDownloads::record(rootB, HeritageDataset::DesignatedHeritage, {zipB}));

    KaHeritageBrowser browser;
    restrictRequests(browser);
    browser.setDownloadRoot(rootA);
    browser.setTarget(QStringLiteral("경상북도"), QStringLiteral("안동시"),
                      {HeritageDataset::DesignatedHeritage});
    browser.setFetchPlan({{yecheon}, true});
    QStringList labels;
    QList<QStringList> received;
    connect(&browser, &KaHeritageBrowser::datasetReady, &browser,
            [&](HeritageDataset, const QStringList& files) {
              labels << browser.regionLabelForImport();
              received << files;
            });
    QSignalSpy done(&browser, &KaHeritageBrowser::allFinished);
    QSignalSpy failed(&browser, &KaHeritageBrowser::failed);
    browser.start();
    QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 5000);
    QCOMPARE(failed.count(), 0);
    QCOMPARE(labels, (QStringList{QStringLiteral("안동시"), QStringLiteral("예천군")}));
    QCOMPARE(received.size(), 2);
    QCOMPARE(received.at(0), QStringList{QFileInfo(zipA).absoluteFilePath()});
    QCOMPARE(received.at(1), QStringList{QFileInfo(zipB).absoluteFilePath()});
    QCOMPARE(browser.stage(), HeritageStage::Done);
    QCOMPARE(browser.m_downloadRoot, rootB);
    // No page was opened: nothing was sent to the intranet.
    QCOMPARE(browser.m_tabs->count(), 0);
    // A single-city run keeps the old layer names.
    browser.setTarget(QStringLiteral("경상북도"), QStringLiteral("안동시"),
                      {HeritageDataset::DesignatedHeritage});
    QVERIFY(browser.regionLabelForImport().isEmpty());
  }

  void rejectedRecentCopyIsFetchedAgain() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString root = temp.filePath(QStringLiteral("원본"));
    const QString zip = QDir(root).filePath(QStringLiteral("a/지정유산.zip"));
    QDir().mkpath(QFileInfo(zip).absolutePath());
    QFile f(zip);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("broken");
    f.close();
    QVERIFY(HeritageRecentDownloads::record(root, HeritageDataset::DesignatedHeritage, {zip}));
    KaHeritageBrowser browser;
    restrictRequests(browser);
    browser.setDownloadRoot(root);
    browser.setTarget(QStringLiteral("경상북도"), QStringLiteral("안동시"),
                      {HeritageDataset::DesignatedHeritage, HeritageDataset::SurfaceSurveyArea});
    connect(&browser, &KaHeritageBrowser::datasetReady, &browser,
            [&](HeritageDataset, const QStringList&) {
              browser.rejectDataset(QStringLiteral("손상된 ZIP"), true);
            });
    QVERIFY(!browser.reuseRecentDatasets());
    QCOMPARE(browser.m_datasets.size(), 2);
    QVERIFY(!browser.m_reuseProbe);
    QVERIFY(browser.stage() != HeritageStage::Failed);
  }

  void longRetryKeepsGoingAndSaysSo() {
    KaHeritageBrowser browser;
    restrictRequests(browser);
    // F174 hygiene: the intranet login session is never written to disk.
    QCOMPARE(browser.m_profile->persistentCookiesPolicy(), QWebEngineProfile::NoPersistentCookies);
    browser.setTarget(QStringLiteral("경상북도"), QStringLiteral("포항시"),
                      {HeritageDataset::DesignatedHeritage});
    browser.m_running = true;
    browser.m_stage = HeritageStage::Download;
    auto* notice = browser.findChild<QLabel*>(QStringLiteral("heritageNotice"));
    QVERIFY(notice);
    for (int i = 1; i <= 3; ++i) {
      browser.m_downloadRetryReason.clear();
      browser.rejectDataset(QStringLiteral("손상된 ZIP"), true);
      QCOMPARE(browser.m_downloadRetryCount, quint64(i));
      QCOMPARE(notice->isHidden(), i < 3);
    }
    QVERIFY(notice->text().contains(QStringLiteral("3번째")));
    QVERIFY(notice->text().contains(QStringLiteral("취소")));
    QVERIFY(browser.stage() != HeritageStage::Failed);
    browser.stop();
  }

  void failureDoesNotSilentlySkipToTheNextCity() {
    KaHeritageBrowser browser;
    restrictRequests(browser);
    browser.setTarget(QStringLiteral("경상북도"), QStringLiteral("안동시"),
                      {HeritageDataset::DesignatedHeritage});
    browser.setFetchPlan({{{QStringLiteral("경상북도"), QStringLiteral("예천군")}}, false});
    QCOMPARE(browser.regionLabelForImport(), QStringLiteral("안동시"));
    browser.m_running = true;
    browser.m_stage = HeritageStage::Download;
    QSignalSpy done(&browser, &KaHeritageBrowser::allFinished);
    browser.rejectDataset(QStringLiteral("저장 공간 부족"), false);
    QCOMPARE(browser.stage(), HeritageStage::Failed);
    QVERIFY(browser.m_followUps.isEmpty());
    QVERIFY(!browser.m_pendingNext);
    auto* notice = browser.findChild<QLabel*>(QStringLiteral("heritageNotice"));
    QVERIFY(notice && notice->text().contains(QStringLiteral("예천군")));
    QTest::qWait(20);
    QCOMPARE(done.count(), 0);
  }

  void storageFailureDoesNotRetry() {
    KaHeritageBrowser browser;
    restrictRequests(browser);
    browser.m_running = true;
    browser.m_stage = HeritageStage::Download;
    browser.rejectDataset(QStringLiteral("저장 공간 부족"), false);
    QCOMPARE(browser.stage(), HeritageStage::Failed);
    QVERIFY(browser.m_downloadRetryReason.isEmpty());
  }
};

int main(int argc, char** argv) {
  QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
  KaPortableRuntime::applyWebEngineFlags();
  QApplication app(argc, argv);
  QCoreApplication::setOrganizationName(QStringLiteral("ka-hgis-offline-tests"));
  QCoreApplication::setApplicationName(QStringLiteral("heritage-download-retry"));
  QStandardPaths::setTestModeEnabled(true);
  HeritageDownloadRetryTest test;
  return QTest::qExec(&test, argc, argv);
}
#include "test_heritage_download_retry.moc"
