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

#include "app/KaHeritageBrowser.h"
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
    QVERIFY(loaded.wait(10000));
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
