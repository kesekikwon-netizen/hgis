#include <QtTest>
#include <QDateTime>
#include <QElapsedTimer>
#include <QEvent>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QUrlQuery>
#include <QPointer>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>
#include <QSemaphore>
#include <QScopeGuard>
#include <atomic>
#include <memory>
#include <gdal.h>
#include "app/KaReferenceDownloadJob.h"
#include <qgsrasterlayer.h>
#include <qgsrasterdataprovider.h>
#include <qgsrasterblock.h>

#include "core/GeologyMapService.h"
#include "core/LayerOps.h"
#include "core/ReferenceTiledFetch.h"
#include "core/RiverMapService.h"
#include "core/SoilMapService.h"
#include "core/TilePackService.h"
#include <qgsapplication.h>
#include <qgscoordinatetransformcontext.h>
#include <qgscoordinatetransform.h>
#include <qgsfeedback.h>
#include <qgsexception.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgsmapcanvas.h>
#include <qgsnetworkaccessmanager.h>
#include <qgsproject.h>
#include <qgsrectangle.h>
#include <qgsvectorlayer.h>

class TestReferenceDownload : public QObject {
  Q_OBJECT
private slots:
  void preparedFilesAreOwnedUntilAccepted();
  void partialSoilFailurePreservesOriginal();
  void riverPageLimitIsFailure();
  void riverRetriesDroppedPieceOnce();
  void geologySplitsPartialResponseIntoQuarters();
  void tiledFetchMergesAndBoundsRetries();
  void steadySlowResponseOutlivesIdleTimeout();
  void failedRegistrationPreservesOldLayer();
  void cancelledPreparationCanRetry();
  void noResponseTimesOut();
  void workerCancellationLeavesGuiResponsive();
  void geologyServerErrorIsNotNoDataFallback();
  void geologyResponseCrsPreservesSurveyLocation_data();
  void geologyResponseCrsPreservesSurveyLocation();
  void trickleResponseHasAbsoluteDeadline();
  void localCapabilitiesPreserveWmsEndpoint();
  void qgisExceptionBecomesFailure();
  void tilePackWorkerHonoursDownloadOutcome_data();
  void tilePackWorkerHonoursDownloadOutcome();
};

namespace {
const QgsRectangle kExtent(200000, 450000, 200100, 450100);
QByteArray soilData() {
  return R"({"type":"FeatureCollection","crs":{"type":"name","properties":{"name":"EPSG:5186"}},"features":[{"type":"Feature","properties":{"soil_type_geo":"04"},"geometry":{"type":"Polygon","coordinates":[[[200000,450000],[200010,450000],[200010,450010],[200000,450010],[200000,450000]]]}}]})";
}
QByteArray riverData() {
  return QStringLiteral(R"({"type":"FeatureCollection","features":[{"type":"Feature","properties":{"riv_nm":"시험하천","riv_level":"국가하천"},"geometry":{"type":"Polygon","coordinates":[[[127,37],[127.001,37],[127.001,37.001],[127,37.001],[127,37]]]}}]})").toUtf8();
}
ReferenceDownload soilSuccess() {
  return [](const QNetworkRequest& request, QByteArray* body, QString*, QgsFeedback*) {
    const QUrlQuery query(request.url());
    if (query.queryItemValue(QStringLiteral("resultType")) == QLatin1String("hits"))
      *body = query.queryItemValue(QStringLiteral("typeNames")).endsWith(QLatin1String("SOIL_1"))
          ? QByteArray("<FeatureCollection numberMatched=\"1\"/>")
          : QByteArray("<FeatureCollection numberMatched=\"0\"/>");
    else *body = soilData();
    return true;
  };
}
QByteArray wmsCapabilities(const QString& layerName, const QString& endpoint) {
  return QStringLiteral(R"(<WMS_Capabilities version="1.3.0" xmlns="http://www.opengis.net/wms" xmlns:xlink="http://www.w3.org/1999/xlink">
<Service><Name>WMS</Name><Title>Test</Title></Service><Capability>
<Request><GetMap><Format>image/png</Format><DCPType><HTTP><Get><OnlineResource xlink:href="%1"/></Get></HTTP></DCPType></GetMap></Request>
<Layer><Title>Root</Title><CRS>EPSG:4326</CRS><EX_GeographicBoundingBox><westBoundLongitude>124</westBoundLongitude><eastBoundLongitude>130</eastBoundLongitude><southBoundLatitude>33</southBoundLatitude><northBoundLatitude>39</northBoundLatitude></EX_GeographicBoundingBox>
<Layer><Name>%2</Name><Title>Test layer</Title><CRS>EPSG:4326</CRS><BoundingBox CRS="EPSG:4326" minx="33" miny="124" maxx="39" maxy="130"/></Layer></Layer>
</Capability></WMS_Capabilities>)").arg(endpoint, layerName).toUtf8();
}
QByteArray readFile(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return {};
  return file.readAll();
}
}



void TestReferenceDownload::preparedFilesAreOwnedUntilAccepted() {
  QTemporaryDir directory;
  const QString base = directory.filePath(QStringLiteral("soil.gpkg"));
  QString generation;
  {
    const auto result = SoilMapService::prepare(kExtent, base, {}, nullptr, soilSuccess());
    QVERIFY2(result.isReady(), qPrintable(result.error));
    generation = result.gpkgPath;
    QVERIFY(QFile::exists(generation));
    QVERIFY(!QFile::exists(base));
    QgsVectorLayer check(generation + QStringLiteral("|layername=soil_map"), {}, QStringLiteral("ogr"));
    QVERIFY(check.isValid());
    QCOMPARE(check.featureCount(), 1);
    QCOMPARE(check.crs().authid(), QStringLiteral("EPSG:5186"));
  }
  QVERIFY(!QFile::exists(generation));
}

void TestReferenceDownload::partialSoilFailurePreservesOriginal() {
  QTemporaryDir directory;
  const QString base = directory.filePath(QStringLiteral("soil.gpkg"));
  QFile original(base);
  QVERIFY(original.open(QIODevice::WriteOnly));
  original.write("original-reference");
  original.close();
  const auto success = soilSuccess();
  const ReferenceDownload failSecond = [success](const QNetworkRequest& request, QByteArray* body,
                                               QString* error, QgsFeedback* feedback) {
    const QUrlQuery query(request.url());
    if (query.queryItemValue(QStringLiteral("typeNames")).endsWith(QLatin1String("SOIL_2"))) {
      *error = QStringLiteral("시험 서버 연결 실패");
      return false;
    }
    return success(request, body, error, feedback);
  };
  const auto result = SoilMapService::prepare(kExtent, base, {}, nullptr, failSecond);
  QCOMPARE(result.status, PreparedReferenceMap::Status::Failed);
  QVERIFY(!result.error.isEmpty());
  QCOMPARE(readFile(base), QByteArray("original-reference"));
}

void TestReferenceDownload::riverPageLimitIsFailure() {
  QTemporaryDir directory;
  QJsonObject page = QJsonDocument::fromJson(riverData()).object();
  QJsonArray features;
  const QJsonValue feature = page.value(QStringLiteral("features")).toArray().first();
  for (int i = 0; i < 1000; ++i) features.append(feature);
  page.insert(QStringLiteral("features"), features);
  const QByteArray fullPage = QJsonDocument(page).toJson(QJsonDocument::Compact);
  int calls = 0;
  const auto result = RiverMapService::prepare(kExtent, QStringLiteral("TEST_KEY"),
      directory.filePath(QStringLiteral("river.gpkg")), {}, nullptr,
      [&](const QNetworkRequest&, QByteArray* body, QString*, QgsFeedback*) {
        ++calls;
        *body = fullPage;
        return true;
      });
  // A full page set is split into quarters. Every piece here stays full, so the
  // download descends whole -> quarter -> leaf once (10 pages each) and fails
  // instead of silently truncating; the old map is kept.
  QCOMPARE(calls, 30);
  QCOMPARE(result.status, PreparedReferenceMap::Status::Failed);
  QVERIFY(result.error.contains(QStringLiteral("양을 넘었습니다")));
}

void TestReferenceDownload::riverRetriesDroppedPieceOnce() {
  QTemporaryDir directory;
  int calls = 0;
  const auto result = RiverMapService::prepare(kExtent, QStringLiteral("TEST_KEY"),
      directory.filePath(QStringLiteral("river.gpkg")), {}, nullptr,
      [&](const QNetworkRequest&, QByteArray* body, QString* error, QgsFeedback*) {
        if (++calls == 1) {
          *error = QStringLiteral("시험 연결 끊김");
          return false;
        }
        *body = riverData();
        return true;
      });
  QVERIFY2(result.isReady(), qPrintable(result.error));
  QCOMPARE(calls, 2);
}

void TestReferenceDownload::geologySplitsPartialResponseIntoQuarters() {
  QTemporaryDir directory;
  const QString base = directory.filePath(QStringLiteral("geology.gpkg"));
  int featureRequests = 0;
  int quarterSerial = 0;
  const auto feature = [](const QString& id, double x) {
    const double y = 450000.;
    // A braced list with one QJsonArray element copies it instead of nesting it
    // (the ring would become the polygon), so the ring list is wrapped explicitly.
    const QJsonArray ring{QJsonArray{x, y}, QJsonArray{x + 10, y}, QJsonArray{x + 10, y + 10},
                          QJsonArray{x, y + 10}, QJsonArray{x, y}};
    QJsonArray rings;
    rings.append(ring);
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("Feature")},
        {QStringLiteral("id"), id},
        {QStringLiteral("properties"), QJsonObject{{QStringLiteral("기호"), QStringLiteral("Qa")},
            {QStringLiteral("지층"), QStringLiteral("시험 충적층")}, {QStringLiteral("시대"), QStringLiteral("제4기")}}},
        {QStringLiteral("geometry"), QJsonObject{{QStringLiteral("type"), QStringLiteral("Polygon")},
            {QStringLiteral("coordinates"), rings}}}};
  };
  const QJsonObject crs{{QStringLiteral("type"), QStringLiteral("name")},
      {QStringLiteral("properties"), QJsonObject{{QStringLiteral("name"), QStringLiteral("EPSG:5186")}}}};
  const auto result = GeologyMapService::prepare(kExtent, base, {}, nullptr,
      [&](const QNetworkRequest& request, QByteArray* body, QString*, QgsFeedback*) {
        const QUrlQuery query(request.url());
        if (query.queryItemValue(QStringLiteral("request")) == QLatin1String("GetMap")) {
          QImage image(query.queryItemValue(QStringLiteral("width")).toInt(),
                       query.queryItemValue(QStringLiteral("height")).toInt(), QImage::Format_ARGB32);
          image.fill(QColor(249, 249, 127));
          QBuffer buffer(body);
          return buffer.open(QIODevice::WriteOnly) && image.save(&buffer, "PNG");
        }
        ++featureRequests;
        const QStringList bbox = query.queryItemValue(QStringLiteral("bbox")).split(QLatin1Char(','));
        const double width = bbox.value(3).toDouble() - bbox.value(1).toDouble();
        QJsonObject collection{{QStringLiteral("type"), QStringLiteral("FeatureCollection")},
                               {QStringLiteral("crs"), crs}};
        if (width > 0.6) {
          // The server truncated the 80 km request: 1 of 5 matched features.
          collection.insert(QStringLiteral("numberMatched"), 5);
          collection.insert(QStringLiteral("features"), QJsonArray{feature(QStringLiteral("litho.1"), 200000.)});
        } else {
          // Every quarter repeats the boundary polygon litho.1 and adds its own.
          const int serial = ++quarterSerial;
          collection.insert(QStringLiteral("numberMatched"), 2);
          collection.insert(QStringLiteral("features"), QJsonArray{feature(QStringLiteral("litho.1"), 200000.),
              feature(QStringLiteral("litho.q%1").arg(serial), 200000. + 20. * serial)});
        }
        *body = QJsonDocument(collection).toJson(QJsonDocument::Compact);
        return true;
      });
  const auto releaseProviders = qScopeGuard([] {
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  });
  QVERIFY2(result.isReady(), qPrintable(result.error));
  QCOMPARE(featureRequests, 5);
  QgsVectorLayer check(result.gpkgPath + QStringLiteral("|layername=geology_map"), {}, QStringLiteral("ogr"));
  QVERIFY(check.isValid());
  QCOMPARE(check.featureCount(), 5);  // the shared boundary polygon appears once
}

void TestReferenceDownload::tiledFetchMergesAndBoundsRetries() {
  QString error;
  const QByteArray merged = ReferenceTiledFetch::mergeFeatureCollections({
      R"({"type":"FeatureCollection","numberMatched":2,"features":[{"type":"Feature","id":"a","properties":{},"geometry":null},{"type":"Feature","properties":{"n":1},"geometry":null}]})",
      R"({"type":"FeatureCollection","numberMatched":2,"features":[{"type":"Feature","id":"a","properties":{},"geometry":null},{"type":"Feature","properties":{"n":1},"geometry":null},{"type":"Feature","id":"b","properties":{},"geometry":null}]})"},
      &error);
  const QJsonObject object = QJsonDocument::fromJson(merged).object();
  QCOMPARE(object.value(QStringLiteral("features")).toArray().size(), 3);
  QCOMPARE(object.value(QStringLiteral("numberMatched")).toInt(), 3);
  QVERIFY(ReferenceMapPreparation::validateCompleteFeatureCollection(merged, &error));

  int requests = 0;
  const auto dropped = ReferenceTiledFetch::fetch(QgsRectangle(0, 0, 1, 1), [&](const QgsRectangle&) {
    ++requests;
    return ReferenceTiledFetch::Piece{ReferenceTiledFetch::Outcome::Transient, {}, QStringLiteral("끊김")};
  });
  QVERIFY(!dropped.ok);
  QCOMPARE(requests, 2);  // one retry, never an endless loop
  QVERIFY(dropped.error.contains(QStringLiteral("끊김")));

  requests = 0;
  const auto fatal = ReferenceTiledFetch::fetch(QgsRectangle(0, 0, 1, 1), [&](const QgsRectangle&) {
    ++requests;
    return ReferenceTiledFetch::Piece{ReferenceTiledFetch::Outcome::Fatal, {}, QStringLiteral("잘못된 응답")};
  });
  QVERIFY(!fatal.ok);
  QCOMPARE(requests, 1);  // a bad server answer is neither split nor repeated

  requests = 0;
  ReferenceTiledFetch::Options spent;
  spent.budgetMs = 0;  // no time left: the first request runs, nothing more
  const auto overBudget = ReferenceTiledFetch::fetch(QgsRectangle(0, 0, 1, 1), [&](const QgsRectangle&) {
    ++requests;
    return ReferenceTiledFetch::Piece{ReferenceTiledFetch::Outcome::TooLarge, {}, QStringLiteral("많음")};
  }, nullptr, spent);
  QVERIFY(!overBudget.ok);
  QCOMPARE(requests, 1);
}

void TestReferenceDownload::steadySlowResponseOutlivesIdleTimeout() {
  QTcpServer server;
  QVERIFY(server.listen(QHostAddress::LocalHost));
  QObject::connect(&server, &QTcpServer::newConnection, &server, [&] {
    auto* socket = server.nextPendingConnection();
    socket->write("HTTP/1.1 200 OK\r\nContent-Length: 12\r\nConnection: close\r\n\r\n");
    auto sent = std::make_shared<int>(0);
    auto* steady = new QTimer(socket);
    connect(steady, &QTimer::timeout, socket, [socket, steady, sent] {
      socket->write("x");
      if (++*sent >= 12) { steady->stop(); socket->disconnectFromHost(); }
    });
    steady->start(60);  // 12 bytes over ~720 ms, never silent for the 400 ms idle limit
  });
  // One URL per run: QgsBlockingNetworkRequest stores every successful reply in
  // the QGIS disk cache with a 30 s expiry, so a second request for the same URL
  // would be answered from the cache instantly instead of by the slow stream.
  const QString base = QStringLiteral("http://127.0.0.1:%1/steady/%2/")
                           .arg(server.serverPort()).arg(QDateTime::currentMSecsSinceEpoch());
  struct Outcome { bool ok = false; QByteArray body; qint64 elapsed = 0; ReferenceTransferFailure failure{}; };
  const auto run = [base](Outcome& outcome, const ReferenceTransferLimits& limits, const QString& name) {
    const QNetworkRequest request(QUrl(base + name));
    QElapsedTimer timer;
    timer.start();
    QString error;
    outcome.ok = ReferenceMapPreparation::download(request, &outcome.body, &error, nullptr, {},
                                                   limits, &outcome.failure);
    outcome.elapsed = timer.elapsed();
  };
  auto separated = std::make_shared<Outcome>();
  auto absolute = std::make_shared<Outcome>();
  bool complete = false;
  auto* task = new KaReferenceDownloadJob(QStringLiteral("느린 전송 검사"),
      [separated, absolute, run](QgsFeedback*) {
        run(*separated, ReferenceTransferLimits{400, 5000}, QStringLiteral("separated"));  // idle and total separated
        run(*absolute, ReferenceTransferLimits{400, 500}, QStringLiteral("absolute"));      // same stream, short total cap
        return PreparedReferenceMap{};
      }, [&](const PreparedReferenceMap&) { complete = true; });
  const QPointer<KaReferenceDownloadJob> guard(task);
  QgsApplication::taskManager()->addTask(task);
  QTRY_VERIFY_WITH_TIMEOUT(complete, 10000);
  QTRY_VERIFY_WITH_TIMEOUT(guard.isNull(), 2000);
  QVERIFY2(separated->ok, qPrintable(QString::number(separated->elapsed)));
  QCOMPARE(separated->body, QByteArray(12, 'x'));
  QVERIFY(separated->elapsed > 400);  // it outlived the idle limit because data kept arriving
  QCOMPARE(separated->failure, ReferenceTransferFailure::None);
  QVERIFY(!absolute->ok);
  QCOMPARE(absolute->failure, ReferenceTransferFailure::Timeout);
  QVERIFY(absolute->elapsed < 1500);
}

void TestReferenceDownload::failedRegistrationPreservesOldLayer() {
  QgsProject project;
  auto* old = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
      QStringLiteral("수계도(하천망)"), QStringLiteral("memory"));
  project.addMapLayer(old);
  const QString oldId = old->id();
  PreparedReferenceMap broken;
  broken.status = PreparedReferenceMap::Status::Ready;
  broken.gpkgPath = QStringLiteral("/nonexistent/ka-hgis-test-reference.gpkg");
  QString error;
  QVERIFY(!RiverMapService::addPrepared(&project, nullptr, broken, &error));
  QCOMPARE(project.mapLayer(oldId), old);
  QCOMPARE(project.mapLayers().size(), 1);
  QVERIFY(!error.isEmpty());
}

void TestReferenceDownload::cancelledPreparationCanRetry() {
  QTemporaryDir directory;
  const QString base = directory.filePath(QStringLiteral("river.gpkg"));
  QgsFeedback feedback;
  int calls = 0;
  const auto cancelled = RiverMapService::prepare(kExtent, QStringLiteral("TEST_KEY"), base, {}, &feedback,
      [&](const QNetworkRequest&, QByteArray*, QString*, QgsFeedback*) {
        ++calls;
        feedback.cancel();
        return false;
      });
  QCOMPARE(cancelled.status, PreparedReferenceMap::Status::Cancelled);
  QCOMPARE(calls, 1);
  const auto retry = RiverMapService::prepare(kExtent, QStringLiteral("TEST_KEY"), base, {}, nullptr,
      [](const QNetworkRequest&, QByteArray* body, QString*, QgsFeedback*) {
        *body = riverData();
        return true;
      });
  QVERIFY2(retry.isReady(), qPrintable(retry.error));
  QgsProject project;
  QString retainedPath = retry.gpkgPath;
  QString error;
  QVERIFY2(RiverMapService::addPrepared(&project, nullptr, retry, &error), qPrintable(error));
  QVERIFY(QFile::exists(retainedPath));
  QVERIFY(!QFile::exists(base));
}

void TestReferenceDownload::noResponseTimesOut() {
  QTcpServer server;
  QVERIFY(server.listen(QHostAddress::LocalHost));
  QNetworkRequest request(QUrl(QStringLiteral("http://127.0.0.1:%1/map").arg(server.serverPort())));
  QByteArray body;
  QString error;
  QElapsedTimer timer;
  timer.start();
  QVERIFY(!ReferenceMapPreparation::download(request, &body, &error, nullptr, {}, 100));
  QVERIFY2(timer.elapsed() < 5000, qPrintable(QString::number(timer.elapsed())));
  QVERIFY(!error.isEmpty());
}

void TestReferenceDownload::workerCancellationLeavesGuiResponsive() {
  QTcpServer server;
  QVERIFY(server.listen(QHostAddress::LocalHost));
  const QNetworkRequest request(QUrl(QStringLiteral("http://127.0.0.1:%1/cancel").arg(server.serverPort())));
  bool complete = false;
  bool workerAffinity = false;
  bool guiCallback = false;
  PreparedReferenceMap::Status status = PreparedReferenceMap::Status::Failed;
  auto* task = new KaReferenceDownloadJob(QStringLiteral("취소 검사"),
      [&](QgsFeedback* feedback) {
        workerAffinity = feedback->thread() == QThread::currentThread();
        PreparedReferenceMap result;
        QByteArray body;
        ReferenceMapPreparation::download(request, &body, &result.error, feedback, {}, 1000);
        ReferenceMapPreparation::cancelled(result, feedback);
        return result;
      }, [&](const PreparedReferenceMap& result) {
        status = result.status;
        guiCallback = QThread::currentThread() == QCoreApplication::instance()->thread();
        complete = true;
      });
  const QPointer<KaReferenceDownloadJob> guard(task);
  QgsApplication::taskManager()->addTask(task);
  QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(), 2000);
  task->cancel();
  QTRY_VERIFY_WITH_TIMEOUT(complete, 2500);
  QCOMPARE(status, PreparedReferenceMap::Status::Cancelled);
  QVERIFY(workerAffinity);
  QVERIFY(guiCallback);
  QTRY_VERIFY_WITH_TIMEOUT(guard.isNull(), 2000);
}

void TestReferenceDownload::trickleResponseHasAbsoluteDeadline() {
  QTcpServer server;
  QVERIFY(server.listen(QHostAddress::LocalHost));
  QObject::connect(&server, &QTcpServer::newConnection, &server, [&] {
    auto* socket = server.nextPendingConnection();
    socket->write("HTTP/1.1 200 OK\r\nContent-Length: 100000\r\n\r\n");
    auto* trickle = new QTimer(socket);
    connect(trickle, &QTimer::timeout, socket, [socket] { socket->write("x"); });
    trickle->start(20);
  });
  const QNetworkRequest request(QUrl(QStringLiteral("http://127.0.0.1:%1/trickle").arg(server.serverPort())));
  bool complete = false;
  QString error;
  qint64 elapsed = 0;
  auto* task = new KaReferenceDownloadJob(QStringLiteral("전체 시간 제한 검사"),
      [&](QgsFeedback* feedback) {
        PreparedReferenceMap result;
        QByteArray body;
        QElapsedTimer timer;
        timer.start();
        ReferenceMapPreparation::download(request, &body, &result.error, feedback, {}, 150);
        elapsed = timer.elapsed();
        return result;
      }, [&](const PreparedReferenceMap& result) { error = result.error; complete = true; });
  const QPointer<KaReferenceDownloadJob> guard(task);
  QgsApplication::taskManager()->addTask(task);
  QTRY_VERIFY_WITH_TIMEOUT(complete, 2000);
  QVERIFY(elapsed < 1500);
  QVERIFY(error.contains(QStringLiteral("응답이 늦어")));
  QTRY_VERIFY_WITH_TIMEOUT(guard.isNull(), 2000);
}

void TestReferenceDownload::localCapabilitiesPreserveWmsEndpoint() {
  QTcpServer server;
  QVERIFY(server.listen(QHostAddress::LocalHost));
  QTemporaryDir directory;
  const QByteArray xml = wmsCapabilities(QStringLiteral("test"),
      QStringLiteral("http://127.0.0.1:%1/maps?").arg(server.serverPort()));
  const QString path = directory.filePath(QStringLiteral("capabilities.xml"));
  QString error;
  QVERIFY(ReferenceMapPreparation::writeResponse(path, xml, &error));
  const QString uri = QStringLiteral("crs=EPSG:4326&format=image/png&layers=test&styles=&url=%1")
      .arg(QString::fromLatin1(QUrl::toPercentEncoding(QUrl::fromLocalFile(path).toString())));
  QgsRasterLayer layer(uri, QStringLiteral("local capabilities"), QStringLiteral("wms"));
  QVERIFY2(layer.isValid(), qPrintable(layer.error().summary()));
  QVERIFY(!server.hasPendingConnections());
  QCOMPARE(layer.crs().authid(), QStringLiteral("EPSG:4326"));
  QVERIFY(layer.extent().width() > 0);
  // Metadata exposes the advertised GetMap endpoint despite the local capabilities URL.
  QVERIFY(layer.htmlMetadata().contains(QStringLiteral("127.0.0.1:%1/maps").arg(server.serverPort())));
}

void TestReferenceDownload::geologyServerErrorIsNotNoDataFallback() {
  QTemporaryDir directory;
  const auto failed = GeologyMapService::prepare(kExtent, directory.filePath(QStringLiteral("geology.gpkg")), {},
      nullptr, [](const QNetworkRequest&, QByteArray* body, QString*, QgsFeedback*) {
        *body = "<ServiceExceptionReport>unavailable</ServiceExceptionReport>";
        return true;
      });
  QCOMPARE(failed.status, PreparedReferenceMap::Status::Failed);
  QVERIFY(failed.rasterUri.isEmpty());
  const auto empty = GeologyMapService::prepare(kExtent, directory.filePath(QStringLiteral("geology.gpkg")), {},
      nullptr, [](const QNetworkRequest& request, QByteArray* body, QString*, QgsFeedback*) {
        if (request.url().query().contains(QLatin1String("GetCapabilities")))
          *body = wmsCapabilities(QStringLiteral("geoOpen:L_50K_Geology_Map"), QStringLiteral("https://data.kigam.re.kr/geoserver/ows?"));
        else *body = "{\"type\":\"FeatureCollection\",\"features\":[]}";
        return true;
      });
  QVERIFY2(empty.isReady(), qPrintable(empty.error));
  QVERIFY(!empty.rasterUri.isEmpty());
  QVERIFY(!empty.warnings.isEmpty());
  QVERIFY(empty.rasterUri.contains(QLatin1String("file")));
  QVERIFY(QFile::exists(QDir(empty.storage->path()).filePath(QStringLiteral("capabilities.xml"))));
  QgsRasterLayer check(empty.rasterUri, QStringLiteral("prepared local capabilities"), QStringLiteral("wms"));
  QVERIFY(check.isValid());
  QVERIFY(check.htmlMetadata().contains(QStringLiteral("https://data.kigam.re.kr/geoserver/ows")));
}

void TestReferenceDownload::geologyResponseCrsPreservesSurveyLocation_data() {
  QTest::addColumn<QString>("coordinateCrs");
  QTest::addColumn<QString>("declaredCrs");
  QTest::addColumn<bool>("ready");
  QTest::newRow("requested-5186") << QStringLiteral("EPSG:5186") << QStringLiteral("EPSG:5186") << true;
  QTest::newRow("response-5187") << QStringLiteral("EPSG:5187") << QStringLiteral("EPSG:5187") << true;
  QTest::newRow("response-4326") << QStringLiteral("EPSG:4326") << QStringLiteral("urn:ogc:def:crs:EPSG::4326") << true;
  QTest::newRow("geojson-default-4326") << QStringLiteral("EPSG:4326") << QString() << true;
  QTest::newRow("unknown-declared-crs") << QStringLiteral("EPSG:5187") << QStringLiteral("EPSG:999999") << false;
  QTest::newRow("invalid-geographic-coordinates") << QStringLiteral("EPSG:5187") << QStringLiteral("EPSG:4326") << false;
}

void TestReferenceDownload::geologyResponseCrsPreservesSurveyLocation() {
  QFETCH(QString, coordinateCrs);
  QFETCH(QString, declaredCrs);
  QFETCH(bool, ready);
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString base = directory.filePath(QStringLiteral("geology.gpkg"));
  QFile original(base);
  QVERIFY(original.open(QIODevice::WriteOnly));
  original.write("previous-reference");
  original.close();

  // A synthetic square at the inspected Daegu survey location; no field file is opened.
  const QgsCoordinateReferenceSystem surveyCrs(QStringLiteral("EPSG:5187"));
  const QgsCoordinateReferenceSystem outputCrs(QStringLiteral("EPSG:5186"));
  const QgsPointXY center(135803.5234375, 412903.0625);
  const QgsRectangle surveyExtent(center.x() - 50, center.y() - 50, center.x() + 50, center.y() + 50);
  const QgsCoordinateTransformContext context;
  const QgsCoordinateTransform responseTransform(surveyCrs, QgsCoordinateReferenceSystem(coordinateCrs), context);
  const QgsCoordinateTransform toOutput(surveyCrs, outputCrs, context);
  QgsGeometry responseGeometry = QgsGeometry::fromRect(surveyExtent);
  QCOMPARE(responseGeometry.transform(responseTransform), Qgis::GeometryOperationResult::Success);
  QJsonObject response{
      {QStringLiteral("type"), QStringLiteral("FeatureCollection")},
      {QStringLiteral("features"), QJsonArray{QJsonObject{
          {QStringLiteral("type"), QStringLiteral("Feature")},
          {QStringLiteral("properties"), QJsonObject{
              {QStringLiteral("기호"), QStringLiteral("Qa")},
              {QStringLiteral("지층"), QStringLiteral("시험 충적층")},
              {QStringLiteral("시대"), QStringLiteral("제4기")}}},
          {QStringLiteral("geometry"), QJsonDocument::fromJson(responseGeometry.asJson(15).toUtf8()).object()}}}}};
  if (!declaredCrs.isEmpty()) {
    response.insert(QStringLiteral("crs"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("name")},
        {QStringLiteral("properties"), QJsonObject{{QStringLiteral("name"), declaredCrs}}}});
  }
  const QByteArray responseBytes = QJsonDocument(response).toJson(QJsonDocument::Compact);
  int featureRequests = 0;
  int colorRequests = 0;
  const auto result = GeologyMapService::prepare(toOutput.transformBoundingBox(surveyExtent), base, context,
      nullptr, [&](const QNetworkRequest& request, QByteArray* body, QString*, QgsFeedback*) {
        const QUrlQuery query(request.url());
        if (query.queryItemValue(QStringLiteral("request")) == QLatin1String("GetFeature")) {
          ++featureRequests;
          if (query.queryItemValue(QStringLiteral("srsName")) != QLatin1String("EPSG:5186")) return false;
          *body = responseBytes;
          return true;
        }
        if (query.queryItemValue(QStringLiteral("request")) != QLatin1String("GetMap")) return false;
        ++colorRequests;
        QImage image(query.queryItemValue(QStringLiteral("width")).toInt(),
                     query.queryItemValue(QStringLiteral("height")).toInt(), QImage::Format_ARGB32);
        image.fill(QColor(249, 249, 127));
        QBuffer buffer(body);
        return buffer.open(QIODevice::WriteOnly) && image.save(&buffer, "PNG");
      });
  // QGIS releases OGR feature-source handles with deleteLater, including failed
  // preparation. Drain those releases after the local project dies and before
  // either temporary directory tries to remove its GeoJSON/GPKG files.
  const auto releaseProviders = qScopeGuard([&result] {
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    if (result.storage) result.storage->setAutoRemove(true);
  });
  QCOMPARE(featureRequests, 1);
  QCOMPARE(readFile(base), QByteArray("previous-reference"));
  if (!ready) {
    QCOMPARE(result.status, PreparedReferenceMap::Status::Failed);
    QVERIFY(!result.error.isEmpty());
    QVERIFY(result.rasterUri.isEmpty());
    QCOMPARE(colorRequests, 0);
    return;
  }
  QVERIFY2(result.isReady(), qPrintable(result.error));
  QCOMPARE(colorRequests, 1);
  QVERIFY(result.officialColors.contains(QStringLiteral("Qa")));
  {
    QgsProject project;
    project.setCrs(surveyCrs);
    project.setTransformContext(context);
    // A local placeholder avoids fetching hillshade while testing registration.
    auto* shade = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187"),
        GeologyMapService::reliefLayerTitle(), QStringLiteral("memory"));
    project.addMapLayer(shade);
    QgsMapCanvas canvas;
    canvas.freeze(true);
    canvas.setDestinationCrs(surveyCrs);
    canvas.setExtent(surveyExtent);
    const QgsRectangle before = canvas.extent();
    QString error;
    auto* layer = qobject_cast<QgsVectorLayer*>(GeologyMapService::addPrepared(&project, &canvas, result, &error));
    QVERIFY2(layer, qPrintable(error));
    QCOMPARE(layer->crs(), outputCrs);
    QCOMPARE(project.crs(), surveyCrs);
    QCOMPARE(canvas.mapSettings().destinationCrs(), surveyCrs);
    QCOMPARE(canvas.extent(), before);
    QCOMPARE(layer->featureCount(), 1);
    QVERIFY(LayerOps::isReferenceLayer(layer));
    QgsFeature feature;
    QVERIFY(layer->getFeatures().nextFeature(feature));
    QgsGeometry geometry = feature.geometry();
    QVERIFY(geometry.contains(QgsGeometry::fromPointXY(toOutput.transform(center))));
    QCOMPARE(geometry.transform(QgsCoordinateTransform(layer->crs(), surveyCrs, project.transformContext())),
             Qgis::GeometryOperationResult::Success);
    QVERIFY(geometry.contains(QgsGeometry::fromPointXY(center)));
    QVERIFY(geometry.centroid().asPoint().distance(center) < 0.01);
  }
}

void TestReferenceDownload::tilePackWorkerHonoursDownloadOutcome_data() {
  QTest::addColumn<int>("mode");
  QTest::newRow("complete-offline-pixels") << 0;
  QTest::newRow("http-403-preserves-original") << 1;
  QTest::newRow("http-503-preserves-original") << 2;
  QTest::newRow("cancel-during-gdal-without-event-loop") << 3;
  QTest::newRow("late-cancel-after-atomic-save-stays-ready") << 4;
}

void TestReferenceDownload::tilePackWorkerHonoursDownloadOutcome() {
  QFETCH(int, mode);
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString outputPath = directory.filePath(QStringLiteral("saved.mbtiles"));
  const QByteArray originalBytes("previous-offline-file");
  {
    QFile original(outputPath);
    QVERIFY(original.open(QIODevice::WriteOnly));
    QCOMPARE(original.write(originalBytes), qint64(originalBytes.size()));
  }

  QImage tile(256, 256, QImage::Format_RGB32);
  tile.fill(QColor(41, 113, 173));
  QByteArray png;
  QBuffer buffer(&png);
  QVERIFY(buffer.open(QIODevice::WriteOnly));
  QVERIFY(tile.save(&buffer, "PNG"));
  QTcpServer server;
  QVERIFY(server.listen(QHostAddress::LocalHost));
  TilePackService::Options options;
  options.urlTemplate = QStringLiteral("http://127.0.0.1:%1/{z}/{x}/{y}.png").arg(server.serverPort());
  options.minZoom = 0;
  options.maxZoom = 1;
  options.jpeg = false;
  struct State {
    PreparedReferenceMap result;
    bool complete = false; // GUI-only; worker state below is synchronized.
    std::atomic_bool workerAffinity{false};
    std::atomic_bool callbackCancellationObserved{false};
    QSemaphore fileCommitted;
    QSemaphore finishAllowed;
  };
  const auto state = std::make_shared<State>();
  auto* task = new KaReferenceDownloadJob(QStringLiteral("오프라인 지도 시험"),
      [state, options, outputPath, mode](QgsFeedback* feedback, const std::function<bool()>& cancelRequested) {
        state->workerAffinity.store(feedback->thread() == QThread::currentThread() &&
                                    QThread::currentThread() != QCoreApplication::instance()->thread());
        PreparedReferenceMap result;
        const double half = TilePackService::webMercatorHalfWorld();
        const auto checkCancel = [&] {
          const bool cancelled = cancelRequested();
          if (cancelled) state->callbackCancellationObserved.store(true);
          return cancelled;
        };
        // GDAL runs synchronously here. No worker Qt event processing services
        // the job's cancellation timer; only the explicit callback can cancel.
        const bool ok = TilePackService::build(options, -half, -half, half, half,
            outputPath, &result.error, feedback, checkCancel);
        if (ok) {
          result.rasterUri = outputPath;
          result.status = PreparedReferenceMap::Status::Ready;
          result.outputCommitted = true;
          if (mode == 4) {
            state->fileCommitted.release();
            state->finishAllowed.tryAcquire(1, 5000);
          }
        } else if (cancelRequested()) {
          result.status = PreparedReferenceMap::Status::Cancelled;
        }
        return result;
      }, [state](const PreparedReferenceMap& result) {
        state->result = result;
        state->complete = true;
      });
  const QPointer<KaReferenceDownloadJob> guard(task);
  int receivedRequests = 0;
  QObject::connect(&server, &QTcpServer::newConnection, &server, [&server, guard, png, mode, &receivedRequests] {
    while (auto* socket = server.nextPendingConnection()) {
      auto request = std::make_shared<QByteArray>();
      QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
      QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket, request, guard, png, mode, &receivedRequests] {
        if (socket->property("responseSent").toBool()) return;
        request->append(socket->readAll());
        if (!request->contains("\r\n\r\n")) return;
        socket->setProperty("responseSent", true);
        ++receivedRequests;
        if (mode == 3 && guard) guard->cancel();
        const bool failure = mode == 1 || mode == 2;
        const QByteArray status = mode == 1 ? QByteArray("403 Forbidden") :
                                  mode == 2 ? QByteArray("503 Service Unavailable") : QByteArray("200 OK");
        const QByteArray payload = failure ? QByteArray("test server failure") : png;
        socket->write("HTTP/1.1 " + status + "\r\nContent-Type: image/png\r\nContent-Length: " +
                      QByteArray::number(payload.size()) + "\r\nConnection: close\r\n\r\n" + payload);
        socket->disconnectFromHost();
      });
    }
  });
  QgsApplication::taskManager()->addTask(task);
  if (mode == 4) {
    QTRY_VERIFY_WITH_TIMEOUT(state->fileCommitted.available() > 0 || state->complete, 10000);
    const bool savedBeforeCancel = state->fileCommitted.tryAcquire();
    if (guard) guard->cancel();
    state->finishAllowed.release();
    QTRY_VERIFY_WITH_TIMEOUT(state->complete, 5000);
    QVERIFY2(savedBeforeCancel, qPrintable(state->result.error));
  } else {
    QTRY_VERIFY_WITH_TIMEOUT(state->complete, 10000);
  }
  QTRY_VERIFY_WITH_TIMEOUT(guard.isNull(), 2000);
  QVERIFY(state->workerAffinity.load());
  QVERIFY(receivedRequests > 0);
  if (mode == 0 || mode == 4) {
    QVERIFY2(state->result.isReady(), qPrintable(state->result.error));
    QVERIFY(state->result.outputCommitted);
    QVERIFY(readFile(outputPath) != originalBytes);
    // Verify actual saved pixels and the zoomed-out overview, with the server
    // closed: the output must be usable entirely offline.
    server.close();
    struct Close { void operator()(void* ds) const { if (ds) GDALClose(ds); } };
    std::unique_ptr<void, Close> dataset(GDALOpen(outputPath.toUtf8().constData(), GA_ReadOnly));
    QVERIFY(dataset);
    QVERIFY(GDALGetRasterCount(dataset.get()) >= 3);
    const int x = GDALGetRasterXSize(dataset.get()) / 2;
    const int y = GDALGetRasterYSize(dataset.get()) / 2;
    const unsigned char expected[] = {41, 113, 173};
    for (int band = 1; band <= 3; ++band) {
      const auto rasterBand = GDALGetRasterBand(dataset.get(), band);
      unsigned char value = 0;
      QCOMPARE(GDALRasterIO(rasterBand, GF_Read, x, y, 1, 1, &value, 1, 1, GDT_Byte, 0, 0), CE_None);
      QCOMPARE(value, expected[band - 1]);
      QVERIFY(GDALGetOverviewCount(rasterBand) >= 1);
    }
  } else {
    QCOMPARE(state->result.status, mode == 3 ? PreparedReferenceMap::Status::Cancelled : PreparedReferenceMap::Status::Failed);
    QVERIFY(!state->result.error.isEmpty());
    QVERIFY(!state->result.outputCommitted);
    QCOMPARE(readFile(outputPath), originalBytes);
    if (mode == 3) QVERIFY(state->callbackCancellationObserved.load());
  }
  QCOMPARE(QDir(directory.path()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).size(), 1);
}

void TestReferenceDownload::qgisExceptionBecomesFailure() {
  bool complete = false;
  bool guiCallback = false;
  PreparedReferenceMap captured;
  auto* task = new KaReferenceDownloadJob(QStringLiteral("QGIS 예외 검사"),
      [](QgsFeedback*) -> PreparedReferenceMap { throw QgsException(QStringLiteral("test")); },
      [&](const PreparedReferenceMap& result) {
        captured = result;
        guiCallback = QThread::currentThread() == QCoreApplication::instance()->thread();
        complete = true;
      });
  const QPointer<KaReferenceDownloadJob> guard(task);
  QgsApplication::taskManager()->addTask(task);
  QTRY_VERIFY_WITH_TIMEOUT(complete, 2000);
  QCOMPARE(captured.status, PreparedReferenceMap::Status::Failed);
  QVERIFY(!captured.error.isEmpty());
  QVERIFY(guiCallback);
  QTRY_VERIFY_WITH_TIMEOUT(guard.isNull(), 2000);
}

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  const QString prefix = qEnvironmentVariable("QGIS_PREFIX_PATH", QStringLiteral("D:/OSGeo4W/apps/qgis-dev"));
  QgsApplication::setPrefixPath(prefix, true);
  QgsApplication::setPluginPath(prefix + QStringLiteral("/plugins"));
  QgsApplication::initQgis();
  QgsNetworkAccessManager::instance();
  TestReferenceDownload tests;
  const int result = QTest::qExec(&tests, argc, argv);
  QgsApplication::exitQgis();
  return result;
}

#include "test_reference_download.moc"
