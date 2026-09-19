// 4-2: ParallelJob + provider_wms 중첩 루프 크래시를 자식 프로세스로 재현 시도한다.
// 재현돼도, 안 돼도 제품은 qgis/parallel_rendering=false를 유지한다.
// 공식: https://qgis.org/pyqgis/master/gui/QgsMapCanvas.html setParallelRenderingEnabled
#include <QBuffer>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QNetworkProxy>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUuid>
#include <QtTest>

#include <qgsapplication.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsmapcanvas.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>
#include <qgsrectangle.h>

namespace {

QByteArray tilePng() {
  QImage image(256, 256, QImage::Format_RGB32);
  image.fill(QColor(40, 90, 160));
  QByteArray bytes;
  QBuffer buffer(&bytes);
  buffer.open(QIODevice::WriteOnly);
  image.save(&buffer, "PNG");
  return bytes;
}

class HoldTiles : public QTcpServer {
public:
  HoldTiles() : payload(tilePng()) {
    connect(this, &QTcpServer::newConnection, this, [this] {
      while (hasPendingConnections()) {
        auto* socket = nextPendingConnection();
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
          const QByteArray request = socket->property("request").toByteArray() + socket->readAll();
          socket->setProperty("request", request);
          if (!request.contains("\r\n\r\n") || socket->property("handled").toBool()) return;
          socket->setProperty("handled", true);
          ++requests;
          held.append(QPointer<QTcpSocket>(socket));
        });
      }
    });
  }

  void releaseAll() {
    const auto pending = held;
    held.clear();
    for (const auto& socket : pending) {
      if (!socket || socket->state() != QAbstractSocket::ConnectedState) continue;
      socket->write("HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nConnection: close\r\n"
                    "Content-Length: " +
                    QByteArray::number(payload.size()) + "\r\n\r\n" + payload);
      socket->disconnectFromHost();
    }
  }

  QByteArray payload;
  QList<QPointer<QTcpSocket>> held;
  int requests = 0;
};

QString readUtf8(const QString& path) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
  return QString::fromUtf8(f.readAll());
}

bool productKeepsParallelOff(QString* why) {
  const QString boot = readUtf8(QStringLiteral("src/app/KaApplication.cpp"));
  const QString main = readUtf8(QStringLiteral("src/app/MainWindow.cpp"));
  const QString ops = readUtf8(QStringLiteral("src/core/LayerOps.cpp")) +
                      readUtf8(QStringLiteral("src/core/BasemapOps.cpp"));
  if (boot.isEmpty() || main.isEmpty() || ops.isEmpty()) {
    if (why) *why = QStringLiteral("source open fail (run from repo root)");
    return false;
  }
  if (!boot.contains(QStringLiteral("qgis/parallel_rendering"))) {
    if (why) *why = QStringLiteral("KaApplication missing qgis/parallel_rendering");
    return false;
  }
  if (!boot.contains(QStringLiteral("tileSettings.setValue(QStringLiteral(\"qgis/parallel_rendering\"), false)"))) {
    if (why) *why = QStringLiteral("boot must write parallel_rendering false");
    return false;
  }
  if (boot.contains(QStringLiteral("tileSettings.setValue(QStringLiteral(\"qgis/parallel_rendering\"), true)"))) {
    if (why) *why = QStringLiteral("KaApplication enables parallel_rendering");
    return false;
  }
  if (!main.contains(QStringLiteral("setParallelRenderingEnabled(false)"))) {
    if (why) *why = QStringLiteral("MainWindow must force sequential");
    return false;
  }
  if (!ops.contains(QStringLiteral("setParallelRenderingEnabled(false)"))) {
    if (why) *why = QStringLiteral("LayerOps must force sequential");
    return false;
  }
  return true;
}

int runParallelWmsChild() {
  HoldTiles server;
  if (!server.listen(QHostAddress::LocalHost, 0)) return 2;
  QgsProject project;
  project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  const QString uri = QStringLiteral(
      "type=xyz&url=http://127.0.0.1:%1/%2/%7Bz%7D/%7Bx%7D/%7By%7D.png"
      "&zmin=9&zmax=9&crs=EPSG:3857&tilePixelRatio=1")
                          .arg(server.serverPort())
                          .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
  auto* layer = new QgsRasterLayer(uri, QStringLiteral("repro-wms"), QStringLiteral("wms"));
  if (!layer->isValid()) return 3;
  project.addMapLayer(layer, false);

  QgsMapCanvas canvas;
  canvas.setRenderFlag(false);
  canvas.resize(256, 256);
  canvas.setDestinationCrs(project.crs());
  canvas.setLayers({layer});
  canvas.setParallelRenderingEnabled(true);
  canvas.setPreviewJobsEnabled(true);
  auto flags = canvas.mapSettings().flags();
  flags.setFlag(Qgis::MapSettingsFlag::RenderPartialOutput, true);
  canvas.setMapSettingsFlags(flags);
  constexpr double halfWorld = 20037508.342789244;
  canvas.setExtent(QgsRectangle(-halfWorld / 4.0, -halfWorld / 4.0, halfWorld / 4.0, halfWorld / 4.0));
  canvas.setRenderFlag(true);
  canvas.refresh();

  QElapsedTimer wait;
  wait.start();
  while (server.requests < 1 && wait.elapsed() < 8000)
    QCoreApplication::processEvents();
  canvas.stopRendering();
  server.releaseAll();
  wait.restart();
  while (wait.elapsed() < 1500)
    QCoreApplication::processEvents();
  return 0;
}

}  // namespace

class TestParallelRender : public QObject {
  Q_OBJECT
private slots:
  void productKeepsSequentialPolicy();
  void childParallelWms_recordsReproAndKeepsOff();
};

void TestParallelRender::productKeepsSequentialPolicy() {
  QString why;
  QVERIFY2(productKeepsParallelOff(&why), qPrintable(why));
}

void TestParallelRender::childParallelWms_recordsReproAndKeepsOff() {
  QProcess child;
  child.setProcessChannelMode(QProcess::MergedChannels);
  child.setProgram(QCoreApplication::applicationFilePath());
  child.setArguments({QStringLiteral("--parallel-wms-child")});
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
  child.setProcessEnvironment(env);
  child.start();
  QVERIFY2(child.waitForStarted(10000), "child start");
  const bool finished = child.waitForFinished(45000);
  if (!finished) child.kill();
  const bool crashed = finished && (child.exitStatus() == QProcess::CrashExit ||
                                    quint32(child.exitCode()) == 0xC0000005u);
  qInfo().noquote() << "parallel_wms_repro crashed=" << crashed << "finished=" << finished
                    << "exitStatus=" << int(child.exitStatus()) << "exitCode=" << child.exitCode();
  QString why;
  QVERIFY2(productKeepsParallelOff(&why), qPrintable(why));
}

#include "test_parallel_render.moc"

int main(int argc, char** argv) {
  QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
  QgsApplication app(argc, argv, false);
  const QString prefix = qEnvironmentVariable("QGIS_PREFIX_PATH");
  if (!prefix.isEmpty()) {
    QgsApplication::setPrefixPath(prefix, true);
    QgsApplication::setPluginPath(prefix + QStringLiteral("/plugins"));
  }
  QgsApplication::initQgis();
  int rc = 0;
  if (argc >= 2 && QByteArray(argv[1]) == "--parallel-wms-child") {
    rc = runParallelWmsChild();
  } else {
    TestParallelRender test;
    rc = QTest::qExec(&test, argc, argv);
  }
  QgsApplication::exitQgis();
  return rc;
}
