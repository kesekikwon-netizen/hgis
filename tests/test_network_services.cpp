#include <QtTest>
#include <QDir>
#include <QFile>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUuid>
#include <functional>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "core/AdminBoundaryService.h"
#include "core/LocationSearch.h"

namespace {
const QByteArray serviceError = R"({"response":{"status":"ERROR","error":{"code":"INCORRECT_KEY","text":"private server detail"}}})";
const QByteArray locationResult = R"([{"display_name":"test location","lat":"37.5","lon":"127.1","boundingbox":["37.4","37.6","127.0","127.2"]}])";
const QByteArray boundaryResult = R"({"response":{"status":"OK","result":{"featureCollection":{"features":[{"properties":{"full_nm":"test city dong","emd_cd":"123"},"geometry":{"type":"Polygon","coordinates":[[[1,1],[2,1],[2,2],[1,1]]]}}]}}}})";

class LocalServer : public QTcpServer {
public:
  LocalServer() {
    connect(this, &QTcpServer::newConnection, this, [this]() {
      while (hasPendingConnections()) {
        QTcpSocket* socket = nextPendingConnection();
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
          QByteArray request = socket->property("request").toByteArray() + socket->readAll();
          socket->setProperty("request", request);
          if (!request.contains("\r\n\r\n") || socket->property("handled").toBool()) return;
          socket->setProperty("handled", true);
          const QString path = QString::fromLatin1(request.split(' ').value(1));
          requests.push_back(path);
          if (handler) handler(socket, path);
        });
      }
    });
  }

  static void respond(QTcpSocket* socket, const QByteArray& body, int status = 200) {
    socket->write("HTTP/1.1 " + QByteArray::number(status) + " Response\r\n"
                  "Content-Type: application/json\r\nConnection: close\r\nContent-Length: "
                  + QByteArray::number(body.size()) + "\r\n\r\n" + body);
    socket->disconnectFromHost();
  }

  QStringList requests;
  std::function<void(QTcpSocket*, const QString&)> handler;
};

// Every request, including fallback, stays on localhost. Strip all query items so
// neither the user's API key nor search text can leave through the test server.
class LocalNetwork : public QNetworkAccessManager {
public:
  explicit LocalNetwork(quint16 port) : m_port(port) {
    setProxy(QNetworkProxy::NoProxy);
  }
  QList<int> transferTimeouts;
  QList<QUrlQuery> queries;
protected:
  QNetworkReply* createRequest(Operation operation, const QNetworkRequest& request,
                               QIODevice* outgoingData) override {
    transferTimeouts.push_back(request.transferTimeout());
    QUrlQuery sanitized(request.url());
    sanitized.removeAllQueryItems(QStringLiteral("key"));
    queries.push_back(sanitized);
    QNetworkRequest local(request);
    const QString path = request.url().host().contains(QLatin1String("nominatim"))
                           ? QStringLiteral("/nominatim") : QStringLiteral("/vworld");
    local.setUrl(QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(m_port).arg(path)));
    return QNetworkAccessManager::createRequest(operation, local, outgoingData);
  }
private:
  quint16 m_port;
};
}

class NetworkServicesTest : public QObject {
  Q_OBJECT
private slots:
  void parcelMatches_data() {
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("returned");
    QTest::addColumn<QString>("pnu");
    QTest::addColumn<bool>("matches");
    const QString pnu = QStringLiteral("4719025025100120003");
    const QString mountain = QStringLiteral("4719025025200120003");
    QTest::newRow("exact-no-title") << QStringLiteral("경북 구미시 고아읍 봉한리 12-3") << QStringLiteral("경상북도 구미시 고아읍 봉한리 12-3") << pnu << true;
    QTest::newRow("normalize-lot") << QStringLiteral("경북  구미시 고아읍 봉한리 0012 - 0003번지") << QStringLiteral("경상북도 구미시 고아읍 봉한리 12-3") << pnu << true;
    QTest::newRow("mountain") << QStringLiteral("경북 구미시 고아읍 봉한리 산 12-3") << QStringLiteral("경상북도 구미시 고아읍 봉한리 산12-3") << mountain << true;
    QTest::newRow("wrong-mountain") << QStringLiteral("경북 구미시 고아읍 봉한리 12-3") << QStringLiteral("경상북도 구미시 고아읍 봉한리 산12-3") << mountain << false;
    QTest::newRow("wrong-sub") << QStringLiteral("경북 구미시 고아읍 봉한리 12-4") << QStringLiteral("경상북도 구미시 고아읍 봉한리 12-3") << pnu << false;
    QTest::newRow("wrong-main") << QStringLiteral("경북 구미시 고아읍 봉한리 112-3") << QStringLiteral("경상북도 구미시 고아읍 봉한리 12-3") << pnu << false;
    QTest::newRow("wrong-ri") << QStringLiteral("경북 구미시 고아읍 괴평리 12-3") << QStringLiteral("경상북도 구미시 고아읍 봉한리 12-3") << pnu << false;
    QTest::newRow("wrong-city") << QStringLiteral("경북 상주시 고아읍 봉한리 12-3") << QStringLiteral("경상북도 구미시 고아읍 봉한리 12-3") << pnu << false;
    QTest::newRow("added-general-gu") << QStringLiteral("경기 수원시 이의동 12-3") << QStringLiteral("경기도 수원시 영통구 이의동 12-3") << pnu << true;
    QTest::newRow("explicit-wrong-gu") << QStringLiteral("경기 수원시 장안구 이의동 12-3") << QStringLiteral("경기도 수원시 영통구 이의동 12-3") << pnu << false;
    QTest::newRow("numbered-dong") << QStringLiteral("서울 성동구 성수동1가 12-3") << QStringLiteral("서울특별시 성동구 성수동1가 12-3") << pnu << true;
    QTest::newRow("sejongro-legal-dong") << QStringLiteral("서울 종로구 세종로 1-68") << QStringLiteral("서울특별시 종로구 세종로 1-68") << QStringLiteral("1111011900100010068") << true;
    QTest::newRow("wrong-numbered-dong") << QStringLiteral("서울 성동구 성수동2가 12-3") << QStringLiteral("서울특별시 성동구 성수동1가 12-3") << pnu << false;
    QTest::newRow("sejong") << QStringLiteral("세종 세종시 나성동 12-3") << QStringLiteral("세종특별자치시 나성동 12-3") << pnu << true;
    QTest::newRow("renamed-province") << QStringLiteral("강원도 춘천시 온의동 12-3") << QStringLiteral("강원특별자치도 춘천시 온의동 12-3") << pnu << true;
    QTest::newRow("bad-pnu") << QStringLiteral("경북 구미시 고아읍 봉한리 12-3") << QStringLiteral("경상북도 구미시 고아읍 봉한리 12-3") << mountain << false;
    QTest::newRow("missing-pnu") << QStringLiteral("경북 구미시 고아읍 봉한리 12-3") << QStringLiteral("경상북도 구미시 고아읍 봉한리 12-3") << QString() << false;
  }

  void parcelMatches() {
    QFETCH(QString, input);
    QFETCH(QString, returned);
    QFETCH(QString, pnu);
    QFETCH(bool, matches);
    LocalServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    const QJsonObject item{{"id", pnu}, {"address", QJsonObject{{"parcel", returned}}},
                           {"point", QJsonObject{{"x", "128.3"}, {"y", "36.2"}}}};
    const QByteArray body = QJsonDocument(QJsonObject{{"response", QJsonObject{
      {"status", "OK"}, {"result", QJsonObject{{"items", QJsonArray{item, item}}}}}}}).toJson();
    server.handler = [body](QTcpSocket* socket, const QString&) { LocalServer::respond(socket, body); };
    auto network = std::make_unique<LocalNetwork>(server.serverPort());
    auto* observed = network.get();
    LocationSearch service(std::move(network), 3000);
    QSignalSpy finished(&service, &LocationSearch::finished);
    QSignalSpy failed(&service, &LocationSearch::failed);
    service.searchParcel(input);
    QTRY_COMPARE_WITH_TIMEOUT(finished.size() + failed.size(), 1, 3000);
    QCOMPARE(finished.size(), matches ? 1 : 0);
    QCOMPARE(server.requests.size(), 1); // Never fall back to place/OSM.
    QCOMPARE(observed->queries.first().queryItemValue("type"), QStringLiteral("ADDRESS"));
    QCOMPARE(observed->queries.first().queryItemValue("category"), QStringLiteral("PARCEL"));
    QCOMPARE(observed->queries.first().queryItemValue("crs"), QStringLiteral("EPSG:4326"));
    if (matches) {
      const auto hits = qvariant_cast<QVector<LocationHit>>(finished.first().first());
      QCOMPARE(hits.size(), 1); // Same PNU duplicate removed.
      QCOMPARE(hits.first().title, returned);
      QCOMPARE(hits.first().pnu, pnu);
      QCOMPARE(hits.first().lon, 128.3);
    } else QVERIFY(failed.first().first().toString().contains(QStringLiteral("정확히 일치")));
  }

  void incompleteParcelNeverRequestsNetwork() {
    LocalServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    auto network = std::make_unique<LocalNetwork>(server.serverPort());
    auto* observed = network.get();
    LocationSearch service(std::move(network), 3000);
    QSignalSpy failed(&service, &LocationSearch::failed);
    const QStringList inputs{QStringLiteral("경북 구미시 고아읍 12-3"),
      QStringLiteral("경북 봉한리 12-3"), QStringLiteral("경북 구미시 봉한리 0"),
      QStringLiteral("경북 구미시 봉한리 12-3-4"), QStringLiteral("경북 구미시 봉한리 12번 건물")};
    for (const auto& input : inputs) service.searchParcel(input);
    QCOMPARE(failed.size(), inputs.size());
    QVERIFY(observed->queries.isEmpty());
  }

  void parcelMissingKeyDoesNotUsePlaceSearch() {
    LocalServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    auto network = std::make_unique<LocalNetwork>(server.serverPort());
    auto* observed = network.get();
    LocationSearch service(std::move(network), 3000);
    QSignalSpy failed(&service, &LocationSearch::failed);
    qputenv("VWORLD_API_KEY", " "); // Nonempty override avoids reading machine settings.
    service.searchParcel(QStringLiteral("경북 구미시 고아읍 봉한리 12-3"));
    qputenv("VWORLD_API_KEY", "local-test-key");
    QCOMPARE(failed.size(), 1);
    QVERIFY(failed.first().first().toString().contains(QStringLiteral("API 키")));
    QVERIFY(observed->queries.isEmpty());
  }

  void parcelUncertainResultsDoNotMove_data() {
    QTest::addColumn<QString>("problem");
    for (const auto& name : {"different-pnus", "truncated-records", "truncated-pages", "bad-point"})
      QTest::newRow(name) << QString::fromLatin1(name);
  }

  void parcelUncertainResultsDoNotMove() {
    QFETCH(QString, problem);
    const QString address = QStringLiteral("경상북도 구미시 고아읍 봉한리 12-3");
    QJsonObject item{{"id", "4719025025100120003"}, {"address", QJsonObject{{"parcel", address}}},
                     {"point", QJsonObject{{"x", "128.3"}, {"y", "36.2"}}}};
    if (problem == QLatin1String("bad-point")) item["point"] = QJsonObject{{"x", "36.2"}, {"y", "128.3"}};
    QJsonArray items{item};
    if (problem == QLatin1String("different-pnus")) {
      item["id"] = QStringLiteral("4719025026100120003");
      items.append(item);
    }
    QJsonObject response{{"status", "OK"}, {"result", QJsonObject{{"items", items}}}};
    if (problem == QLatin1String("truncated-records")) response["record"] = QJsonObject{{"total", "1001"}};
    if (problem == QLatin1String("truncated-pages")) response["page"] = QJsonObject{{"total", 2}};
    const auto body = QJsonDocument(QJsonObject{{"response", response}}).toJson();
    LocalServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    server.handler = [body](QTcpSocket* socket, const QString&) { LocalServer::respond(socket, body); };
    LocationSearch service(std::make_unique<LocalNetwork>(server.serverPort()), 3000);
    QSignalSpy failed(&service, &LocationSearch::failed);
    QSignalSpy finished(&service, &LocationSearch::finished);
    service.searchParcel(address);
    QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 3000);
    QCOMPARE(finished.size(), 0);
    QCOMPARE(server.requests.size(), 1);
  }

  void parcelErrorsNeverFallbackAndAllowRetry_data() {
    QTest::addColumn<QByteArray>("body");
    QTest::addColumn<int>("status");
    QTest::newRow("key-error") << serviceError << 200;
    QTest::newRow("http-error") << serviceError << 503;
    QTest::newRow("malformed") << QByteArray("not json") << 200;
    QTest::newRow("not-found") << QByteArray(R"({"response":{"status":"NOT_FOUND"}})") << 200;
  }

  void parcelErrorsNeverFallbackAndAllowRetry() {
    QFETCH(QByteArray, body);
    QFETCH(int, status);
    LocalServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    server.handler = [body, status](QTcpSocket* socket, const QString&) { LocalServer::respond(socket, body, status); };
    LocationSearch service(std::make_unique<LocalNetwork>(server.serverPort()), 3000);
    QSignalSpy failed(&service, &LocationSearch::failed);
    QSignalSpy finished(&service, &LocationSearch::finished);
    for (int i = 1; i <= 2; ++i) {
      service.searchParcel(QStringLiteral("경북 구미시 고아읍 봉한리 12-3"));
      QTRY_COMPARE_WITH_TIMEOUT(failed.size(), i, 3000);
      QCOMPARE(server.requests.size(), i);
      QVERIFY(!failed.last().first().toString().contains(QStringLiteral("private")));
    }
    QCOMPARE(finished.size(), 0);
  }

  void initTestCase() {
    if (qEnvironmentVariableIsSet("KA_HGIS_LIVE_PARCEL")) {
      m_liveKey = qgetenv("VWORLD_API_KEY");
      if (m_liveKey.trimmed().isEmpty()) {
        QSettings secrets(QStringLiteral("config/secrets.ini"), QSettings::IniFormat);
        m_liveKey = secrets.value(QStringLiteral("vworld/apiKey")).toString().toUtf8();
        if (m_liveKey.trimmed().isEmpty())
          m_liveKey = secrets.value(QStringLiteral("apiKey")).toString().toUtf8();
      }
      if (m_liveKey.trimmed().isEmpty()) {
        const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
        for (const QString& relative : {QStringLiteral("ka-hgis/ka-hgis/ka-hgis-vworld.ini"),
                                        QStringLiteral("ka-hgis/ka-hgis-vworld.ini")}) {
          QSettings saved(QDir(base).filePath(relative), QSettings::IniFormat);
          m_liveKey = saved.value(QStringLiteral("VWorld/ApiKey")).toString().toUtf8();
          if (!m_liveKey.trimmed().isEmpty()) break;
        }
      }
      if (m_liveKey.trimmed().isEmpty()) {
        QSettings saved(QStringLiteral("ka-hgis"), QStringLiteral("ka-hgis"));
        m_liveKey = saved.value(QStringLiteral("VWorld/ApiKey")).toString().toUtf8();
      }
    }
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setApplicationName(QStringLiteral("ka-network-test-")
                                        + QUuid::createUuid().toString(QUuid::Id128));
    m_settingsDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QVERIFY(QDir().mkpath(m_settingsDir));
    QSettings settings(QDir(m_settingsDir).filePath(QStringLiteral("ka-hgis-vworld.ini")),
                       QSettings::IniFormat);
    settings.setValue(QStringLiteral("VWorld/ApiKey"), QStringLiteral("local-test-key"));
    settings.sync();
    m_oldKey = qgetenv("VWORLD_API_KEY");
    m_hadKey = qEnvironmentVariableIsSet("VWORLD_API_KEY");
    qputenv("VWORLD_API_KEY", "local-test-key");
  }

  void cleanupTestCase() {
    if (m_hadKey) qputenv("VWORLD_API_KEY", m_oldKey);
    else qunsetenv("VWORLD_API_KEY");
    QFile::remove(QDir(m_settingsDir).filePath(QStringLiteral("ka-hgis-vworld.ini")));
    QDir().rmdir(m_settingsDir);
  }

  void liveParcelSearch() {
    if (!qEnvironmentVariableIsSet("KA_HGIS_LIVE_PARCEL") || m_liveKey.trimmed().isEmpty())
      QSKIP("Opt-in live public-address check requires a local key; normal tests stay offline.");
    LocationSearch service;
    QSignalSpy failed(&service, &LocationSearch::failed);
    QSignalSpy finished(&service, &LocationSearch::finished);
    qputenv("VWORLD_API_KEY", m_liveKey);
    service.searchParcel(QStringLiteral("서울특별시 종로구 세종로 1-68"));
    qputenv("VWORLD_API_KEY", "local-test-key");
    QTRY_VERIFY_WITH_TIMEOUT(!failed.isEmpty() || !finished.isEmpty(), 25000);
    QVERIFY2(failed.isEmpty(), failed.isEmpty() ? "" : qPrintable(failed.first().first().toString()));
    QCOMPARE(finished.size(), 1);
    const auto hits = qvariant_cast<QVector<LocationHit>>(finished.first().first());
    QCOMPARE(hits.size(), 1);
    QCOMPARE(hits.first().pnu, QStringLiteral("1111011900100010068"));
    QVERIFY(hits.first().lon > 126.9 && hits.first().lon < 127.1);
    QVERIFY(hits.first().lat > 37.5 && hits.first().lat < 37.7);
  }

  void locationDeadlineReleasesPendingAndAllowsRetry() {
    LocalServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    auto network = std::make_unique<LocalNetwork>(server.serverPort());
    LocalNetwork* observed = network.get();
    LocationSearch service(std::move(network), 400);
    QSignalSpy failed(&service, &LocationSearch::failed);
    QSignalSpy finished(&service, &LocationSearch::finished);
    service.search(QStringLiteral("first"));
    QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(), 1, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 3000);
    QVERIFY(failed.first().first().toString().contains(QStringLiteral("다시")));
    QCOMPARE(finished.size(), 0);
    server.handler = [](QTcpSocket* socket, const QString& path) {
      LocalServer::respond(socket, path == QLatin1String("/vworld") ? serviceError : locationResult);
    };
    service.search(QStringLiteral("retry"));
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 3000);
    QCOMPARE(failed.size(), 1);
    for (int timeout : observed->transferTimeouts) QCOMPARE(timeout, 400);
  }

  void fallbackKeepsPendingUntilItsResult() {
    LocalServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    QPointer<QTcpSocket> fallback;
    server.handler = [&fallback](QTcpSocket* socket, const QString& path) {
      if (path == QLatin1String("/vworld")) LocalServer::respond(socket, serviceError);
      else fallback = socket;
    };
    LocationSearch service(std::make_unique<LocalNetwork>(server.serverPort()), 3000);
    QSignalSpy failed(&service, &LocationSearch::failed);
    QSignalSpy finished(&service, &LocationSearch::finished);
    service.search(QStringLiteral("first"));
    QTRY_VERIFY_WITH_TIMEOUT(fallback, 3000);
    service.search(QStringLiteral("conflicting search"));
    QCOMPARE(failed.size(), 1);
    QCOMPARE(server.requests.size(), 2);
    LocalServer::respond(fallback, locationResult);
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 3000);
    QCOMPARE(server.requests.size(), 2);
    const QVector<LocationHit> hits = qvariant_cast<QVector<LocationHit>>(finished.first().first());
    QCOMPARE(hits.size(), 1);
    QCOMPARE(hits.first().lon, 127.1);
  }

  void fallbackFailureIsNotSuccess() {
    LocalServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    server.handler = [](QTcpSocket* socket, const QString& path) {
      LocalServer::respond(socket, serviceError, path == QLatin1String("/vworld") ? 503 : 200);
    };
    LocationSearch service(std::make_unique<LocalNetwork>(server.serverPort()), 3000);
    QSignalSpy failed(&service, &LocationSearch::failed);
    QSignalSpy finished(&service, &LocationSearch::finished);
    service.search(QStringLiteral("test"));
    QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 3000);
    QCOMPARE(finished.size(), 0);
    QVERIFY(!failed.first().first().toString().contains(QStringLiteral("private")));
    service.search(QStringLiteral("retry"));
    QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 2, 3000);
    QCOMPARE(server.requests.size(), 4);
  }

  void deadlineStopsFallbackEvenWhileBytesKeepArriving() {
    LocalServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    int receivedRequests = 0;
    server.handler = [&receivedRequests](QTcpSocket* socket, const QString& path) {
      ++receivedRequests;
      if (path == QLatin1String("/vworld")) {
        LocalServer::respond(socket, serviceError);
        return;
      }
      socket->write("HTTP/1.1 200 OK\r\nContent-Length: 1000000\r\n\r\n[");
      auto* trickle = new QTimer(socket);
      QObject::connect(trickle, &QTimer::timeout, socket, [socket]() {
        if (socket->state() == QAbstractSocket::ConnectedState) socket->write(" ");
      });
      trickle->start(20);
    };
    LocationSearch service(std::make_unique<LocalNetwork>(server.serverPort()), 500);
    QSignalSpy failed(&service, &LocationSearch::failed);
    QSignalSpy finished(&service, &LocationSearch::finished);
    service.search(QStringLiteral("test"));
    QTRY_COMPARE_WITH_TIMEOUT(receivedRequests, 2, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 3000);
    QCOMPARE(finished.size(), 0);
    QVERIFY(failed.first().first().toString().contains(QStringLiteral("시간이 초과")));
  }

  void invalidCoordinatesCannotBecomeSuccessfulLocation() {
    LocalServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    server.handler = [](QTcpSocket* socket, const QString& path) {
      LocalServer::respond(socket, path == QLatin1String("/vworld") ? serviceError
        : QByteArray(R"([{"display_name":"invalid","lat":"not-a-number","lon":"127"}])"));
    };
    LocationSearch service(std::make_unique<LocalNetwork>(server.serverPort()), 3000);
    QSignalSpy failed(&service, &LocationSearch::failed);
    QSignalSpy finished(&service, &LocationSearch::finished);
    service.search(QStringLiteral("test"));
    QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 3000);
    QCOMPARE(finished.size(), 0);
  }

  void roadAddressUsesRoadCategory() {
    const QString address = QStringLiteral("안동시 풍천면 하회종가길 40");
    QVERIFY(LocationSearch::isRoadAddress(address));
    const QByteArray body = QJsonDocument(QJsonObject{{"response", QJsonObject{
      {"status", "OK"},
      {"result", QJsonObject{{"items", QJsonArray{
        QJsonObject{{"title", "하회종가"},
                    {"address", QJsonObject{{"road", address}, {"parcel", "경상북도 안동시 풍천면 하회리 615"}}},
                    {"point", QJsonObject{{"x", "128.517"}, {"y", "36.539"}}}},
        QJsonObject{{"title", "다른 건물"},
                    {"address", QJsonObject{{"road", "안동시 풍천면 하회종가길 42"}}},
                    {"point", QJsonObject{{"x", "128.518"}, {"y", "36.540"}}}}
      }}}}}}}).toJson();
    LocalServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    server.handler = [body](QTcpSocket* socket, const QString&) { LocalServer::respond(socket, body); };
    auto network = std::make_unique<LocalNetwork>(server.serverPort());
    auto* observed = network.get();
    LocationSearch service(std::move(network), 3000);
    QSignalSpy finished(&service, &LocationSearch::finished);
    QSignalSpy failed(&service, &LocationSearch::failed);
    qputenv("VWORLD_API_KEY", "local-test-key");
    service.search(address);
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 3000);
    QCOMPARE(failed.size(), 0);
    QCOMPARE(server.requests.size(), 1);
    QCOMPARE(observed->queries.first().queryItemValue(QStringLiteral("type")), QStringLiteral("ADDRESS"));
    QCOMPARE(observed->queries.first().queryItemValue(QStringLiteral("category")), QStringLiteral("ROAD"));
    const auto hits = qvariant_cast<QVector<LocationHit>>(finished.first().first());
    QCOMPARE(hits.size(), 2);
    QCOMPARE(hits.first().title, address);
    QCOMPARE(hits.first().lon, 128.517);
  }

  void roadAddressDoesNotFallBackWhenMissing() {
    LocalServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    server.handler = [](QTcpSocket* socket, const QString&) {
      LocalServer::respond(socket, QByteArray(R"({"response":{"status":"NOT_FOUND"}})"));
    };
    auto network = std::make_unique<LocalNetwork>(server.serverPort());
    LocationSearch service(std::move(network), 3000);
    QSignalSpy failed(&service, &LocationSearch::failed);
    QSignalSpy finished(&service, &LocationSearch::finished);
    qputenv("VWORLD_API_KEY", "local-test-key");
    service.search(QStringLiteral("안동시 풍천면 하회종가길 40"));
    QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 3000);
    QCOMPARE(finished.size(), 0);
    QCOMPARE(server.requests.size(), 1);
    QVERIFY(failed.first().first().toString().contains(QStringLiteral("도로명주소")));
  }

  void locationCancelAndDestructionDisconnectReplies() {
    LocalServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    auto service = std::make_unique<LocationSearch>(
      std::make_unique<LocalNetwork>(server.serverPort()), 3000);
    QSignalSpy failed(service.get(), &LocationSearch::failed);
    QSignalSpy finished(service.get(), &LocationSearch::finished);
    service->search(QStringLiteral("cancel"));
    QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(), 1, 3000);
    service->cancel();
    service->search(QStringLiteral("new request"));
    QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(), 2, 3000);
    service.reset();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(failed.size(), 0);
    QCOMPARE(finished.size(), 0);
  }

  void boundaryDeadlineAndServiceErrorAllowRetry() {
    LocalServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    AdminBoundaryService service(std::make_unique<LocalNetwork>(server.serverPort()), 400);
    QSignalSpy failed(&service, &AdminBoundaryService::failed);
    QSignalSpy fetched(&service, &AdminBoundaryService::fetched);
    service.fetchEmd(QStringLiteral("test"), QStringLiteral("city"), QStringLiteral("dong"));
    QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(), 1, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 3000);
    QCOMPARE(fetched.size(), 0);
    server.handler = [](QTcpSocket* socket, const QString&) { LocalServer::respond(socket, serviceError); };
    service.fetchEmd(QStringLiteral("test"), QStringLiteral("city"), QStringLiteral("dong"));
    QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 2, 3000);
    QVERIFY(failed.last().first().toString().contains(QStringLiteral("API 키")));
    QVERIFY(!failed.last().first().toString().contains(QStringLiteral("private")));
    QCOMPARE(fetched.size(), 0);
    server.handler = [](QTcpSocket* socket, const QString&) { LocalServer::respond(socket, boundaryResult); };
    service.fetchEmd(QStringLiteral("test"), QStringLiteral("city"), QStringLiteral("dong"));
    QTRY_COMPARE_WITH_TIMEOUT(fetched.size(), 1, 3000);
    QCOMPARE(failed.size(), 2);
  }

  void boundaryCancelAndDestructionDisconnectReplies() {
    LocalServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    auto service = std::make_unique<AdminBoundaryService>(
      std::make_unique<LocalNetwork>(server.serverPort()), 3000);
    QSignalSpy failed(service.get(), &AdminBoundaryService::failed);
    QSignalSpy fetched(service.get(), &AdminBoundaryService::fetched);
    service->fetchEmd(QStringLiteral("test"), QStringLiteral("city"), QStringLiteral("dong"));
    QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(), 1, 3000);
    service->cancel();
    service->fetchEmd(QStringLiteral("test"), QStringLiteral("city"), QStringLiteral("dong"));
    QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(), 2, 3000);
    service.reset();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(failed.size(), 0);
    QCOMPARE(fetched.size(), 0);
  }

private:
  QByteArray m_liveKey;
  QString m_settingsDir;
  QByteArray m_oldKey;
  bool m_hadKey = false;
};

QTEST_GUILESS_MAIN(NetworkServicesTest)
#include "test_network_services.moc"
