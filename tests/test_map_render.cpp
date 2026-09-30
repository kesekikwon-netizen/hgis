#include <QtTest>
#include <QBuffer>
#include <QDir>
#include <QImage>
#include <QNetworkProxy>
#include <QPointer>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUuid>

#include <qgsapplication.h>
#include <qgscoordinatetransform.h>
#include <qgsmapcanvas.h>
#include <qgsnetworkaccessmanager.h>
#include <qgsnetworkreply.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>

#include "core/LayerOps.h"

namespace {
const QColor oldTileColor(183, 92, 38);
const QColor finalTileColor(31, 118, 174);

QByteArray png(const QColor& color) {
  QImage image(256, 256, QImage::Format_RGB32);
  image.fill(color);
  QByteArray bytes;
  QBuffer buffer(&bytes);
  buffer.open(QIODevice::WriteOnly);
  image.save(&buffer, "PNG");
  return bytes;
}

// All state belongs to the GUI thread. Withholding HTTP responses establishes
// the in-flight render without depending on network speed or a fixed sleep.
class ControlledTiles : public QTcpServer {
public:
  ControlledTiles() : oldPng(png(oldTileColor)), finalPng(png(finalTileColor)) {
    connect(this, &QTcpServer::newConnection, this, [this] {
      while (hasPendingConnections()) {
        auto* socket = nextPendingConnection();
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
          const QByteArray request = socket->property("request").toByteArray() + socket->readAll();
          socket->setProperty("request", request);
          if (!request.contains("\r\n\r\n") || socket->property("handled").toBool()) return;
          socket->setProperty("handled", true);
          if (finalPhase) {
            ++finalRequests;
            respond(socket, finalPng);
          } else {
            ++initialRequests;
            if (initialRequests == 1) {
              firstPath = QString::fromLatin1(request.split(' ').value(1));
              respond(socket, oldPng);
            } else {
              held.append(QPointer<QTcpSocket>(socket));
            }
          }
        });
      }
    });
    connect(QgsNetworkAccessManager::instance(),
            qOverload<QgsNetworkReplyContent>(&QgsNetworkAccessManager::finished),
            this, [this](const QgsNetworkReplyContent& reply) {
      const QUrl url = reply.request().url();
      if (url.port() == serverPort() && url.path() == firstPath && !firstPath.isEmpty())
        firstReplyCompleted = true;
    });
  }

  static void respond(QTcpSocket* socket, const QByteArray& payload) {
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) return;
    socket->write("HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nCache-Control: no-store\r\n"
                  "Connection: close\r\nContent-Length: " + QByteArray::number(payload.size()) +
                  "\r\n\r\n" + payload);
    socket->disconnectFromHost();
  }

  void releaseOldResponses() {
    const auto pending = held;
    held.clear();
    for (const auto& socket : pending) {
      if (socket) respond(socket, oldPng);
    }
  }

  QByteArray oldPng;
  QByteArray finalPng;
  QString firstPath;
  QList<QPointer<QTcpSocket>> held;
  bool firstReplyCompleted = false;
  bool finalPhase = false;
  int initialRequests = 0;
  int finalRequests = 0;
  int rendersStarted = 0;
};

struct RenderCleanup {
  ControlledTiles& server;
  QgsMapCanvas& canvas;
  ~RenderCleanup() {
    server.finalPhase = true;
    server.releaseOldResponses();
    QObject::disconnect(&canvas, nullptr, &server, nullptr);
    canvas.stopRendering();
  }
};
}  // namespace

class TestMapRender : public QObject {
  Q_OBJECT
private slots:
  void panDuringTileDownload_data();
  void panDuringTileDownload();
  // Last: installs the app's tile request identity for the rest of the process.
  void tileRequests_sendRefererToVworldOnly();
};

void TestMapRender::panDuringTileDownload_data() {
  QTest::addColumn<QString>("workCrs");
  QTest::newRow("work-5186") << QStringLiteral("EPSG:5186");
  QTest::newRow("work-5187") << QStringLiteral("EPSG:5187");
}

void TestMapRender::panDuringTileDownload() {
  QFETCH(QString, workCrs);
  // Opt-in A/B diagnostic for the upstream partial-render cancellation crash.
  // CTest leaves this unset, matching the application's safe render policy.
  const bool partialOutput = qEnvironmentVariableIntValue("KA_HGIS_TEST_PARTIAL_OUTPUT") == 1;
  ControlledTiles server;
  QVERIFY(server.listen(QHostAddress::LocalHost, 0));
  QgsProject project;
  project.setCrs(QgsCoordinateReferenceSystem(workCrs));
  const QString uniquePath = QUuid::createUuid().toString(QUuid::WithoutBraces);
  const QString uri = QStringLiteral(
      "type=xyz&url=http://127.0.0.1:%1/%2/%7Bz%7D/%7Bx%7D/%7By%7D.png"
      "&zmin=9&zmax=9&crs=EPSG:3857&tilePixelRatio=1")
      .arg(server.serverPort()).arg(uniquePath);
  auto* layer = new QgsRasterLayer(uri, QStringLiteral("controlled local tiles"), QStringLiteral("wms"));
  const QPointer<QgsRasterLayer> liveLayer(layer);
  QVERIFY2(layer->isValid(), qPrintable(layer->error().message()));
  project.addMapLayer(layer, false);
  const QString layerId = layer->id();

  QgsMapCanvas canvas;
  canvas.setRenderFlag(false);
  canvas.resize(512, 512);
  canvas.setDestinationCrs(project.crs());
  canvas.setLayers({layer});
  canvas.setParallelRenderingEnabled(false);
  canvas.setPreviewJobsEnabled(false); // Isolate the active canvas job.
  canvas.setCachingEnabled(true);
  auto flags = canvas.mapSettings().flags();
  flags.setFlag(Qgis::MapSettingsFlag::RenderPartialOutput, partialOutput);
  canvas.setMapSettingsFlags(flags);
  const RenderCleanup cleanup{server, canvas};

  // A tile-grid crossing yields four initial requests in Web Mercator. Work
  // CRS transformation exercises the real QgsRasterProjector path as well.
  constexpr double halfWorld = 20037508.342789244;
  constexpr double tileSpan = 2.0 * halfWorld / 512.0;
  const double x = -halfWorld + 439.0 * tileSpan;
  const double y = halfWorld - 202.0 * tileSpan;
  const QgsCoordinateTransform transform(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:3857")),
                                         project.crs(), project.transformContext());
  const auto extentAt = [&](double centerX) {
    return transform.transformBoundingBox(QgsRectangle(centerX - 0.45 * tileSpan, y - 0.45 * tileSpan,
                                                        centerX + 0.45 * tileSpan, y + 0.45 * tileSpan));
  };
  canvas.setExtent(extentAt(x));
  connect(&canvas, &QgsMapCanvas::renderStarting, &server, [&server] {
    ++server.rendersStarted;
    if (server.finalPhase) server.releaseOldResponses();
  });
  QSignalSpy refreshed(&canvas, &QgsMapCanvas::mapCanvasRefreshed);
  canvas.show();
  canvas.setRenderFlag(true);
  canvas.refresh();
  QTRY_VERIFY_WITH_TIMEOUT(server.initialRequests >= 2 && server.firstReplyCompleted, 10000);
  QVERIFY(canvas.isDrawing());
  QVERIFY(!server.held.isEmpty());
  const int firstRenderCount = server.rendersStarted;
  const int firstRefreshCount = refreshed.count();

  // A disjoint extent requires new requests, so the final image cannot pass by
  // merely retaining the old cached image. QGIS cancels the previous job.
  server.finalPhase = true;
  canvas.setExtent(extentAt(x + 4.0 * tileSpan));
  canvas.refresh();
  QTRY_VERIFY_WITH_TIMEOUT(server.rendersStarted > firstRenderCount, 10000);
  QTRY_VERIFY_WITH_TIMEOUT(server.finalRequests > 0 && !canvas.isDrawing() &&
                          refreshed.count() > firstRefreshCount, 10000);
  QVERIFY(liveLayer);
  QCOMPARE(project.mapLayer(layerId), layer);
  QCOMPARE(canvas.layers(), QList<QgsMapLayer*>{layer});
  QCOMPARE(canvas.mapSettings().destinationCrs().authid(), workCrs);
  QCOMPARE(canvas.mapSettings().testFlag(Qgis::MapSettingsFlag::RenderPartialOutput), partialOutput);

  const QImage image = canvas.grab().toImage();
  QVERIFY(!image.isNull());
  for (const QPoint fraction : {QPoint(1, 1), QPoint(1, 3), QPoint(2, 2), QPoint(3, 1), QPoint(3, 3)}) {
    const QColor actual = image.pixelColor(image.width() * fraction.x() / 4,
                                           image.height() * fraction.y() / 4);
    QVERIFY2(qAbs(actual.red() - finalTileColor.red()) <= 2 &&
             qAbs(actual.green() - finalTileColor.green()) <= 2 &&
             qAbs(actual.blue() - finalTileColor.blue()) <= 2,
             qPrintable(QStringLiteral("Final map pixel is %1; expected %2").arg(actual.name(), finalTileColor.name())));
  }
  bool eventLoopAlive = false;
  QMetaObject::invokeMethod(&canvas, [&eventLoopAlive] { eventLoopAlive = true; }, Qt::QueuedConnection);
  QTRY_VERIFY_WITH_TIMEOUT(eventLoopAlive, 1000);
  const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
  if (!output.isEmpty()) {
    QVERIFY(QDir().mkpath(output));
    QVERIFY(image.save(QDir(output).filePath(QStringLiteral("map-render-%1-partial-%2.png")
                          .arg(workCrs.section(QLatin1Char(':'), 1)).arg(partialOutput ? 1 : 0))));
  }
}

void TestMapRender::tileRequests_sendRefererToVworldOnly() {
  // F059: the fake localhost Referer authenticates VWorld keys only. Other tile servers
  // (here a local one standing in for OSM/Google) get the identifying User-Agent, no Referer.
  LayerOps::ensureTileNetworkIdentity();
  QTcpServer server;
  QVERIFY(server.listen(QHostAddress::LocalHost, 0));
  QByteArray captured;
  const QByteArray tile = png(finalTileColor);
  connect(&server, &QTcpServer::newConnection, &server, [&] {
    while (server.hasPendingConnections()) {
      QTcpSocket* socket = server.nextPendingConnection();
      connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
      connect(socket, &QTcpSocket::readyRead, socket, [&captured, &tile, socket] {
        captured += socket->readAll();
        if (!captured.contains("\r\n\r\n") || socket->property("answered").toBool()) return;
        socket->setProperty("answered", true);
        ControlledTiles::respond(socket, tile);
      });
    }
  });
  const QUrl url(QStringLiteral("http://127.0.0.1:%1/%2/9/439/202.png")
                     .arg(server.serverPort())
                     .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
  QNetworkReply* reply = QgsNetworkAccessManager::instance()->get(QNetworkRequest(url));
  QVERIFY(reply);
  QTRY_VERIFY_WITH_TIMEOUT(reply->isFinished(), 10000);
  reply->deleteLater();
  QVERIFY2(captured.contains("\r\n\r\n"), captured.constData());
  const QByteArray lower = captured.toLower();
  QVERIFY2(!lower.contains("\r\nreferer:"), captured.constData());
  QVERIFY2(lower.contains("ka-hgis/0.3"), captured.constData());
}

int main(int argc, char** argv) {
  QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
  QgsApplication app(argc, argv, false);
  const QString prefix = qEnvironmentVariable("QGIS_PREFIX_PATH", QStringLiteral("D:/OSGeo4W/apps/qgis-dev"));
  QgsApplication::setPrefixPath(prefix, true);
  QgsApplication::setPluginPath(prefix + QStringLiteral("/plugins"));
  QgsApplication::initQgis();
  QgsNetworkAccessManager::instance();
  TestMapRender tests;
  const int result = QTest::qExec(&tests, argc, argv);
  QgsApplication::exitQgis();
  return result;
}

#include "test_map_render.moc"
