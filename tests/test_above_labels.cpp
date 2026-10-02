#include <QtTest>
#include <QImage>
#include <QPainter>
#include <QDir>
#include <cmath>

#include "app/KaAboveLabelsOverlay.h"
#include "core/LayerOps.h"

#include <qgsapplication.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgslinesymbol.h>
#include <qgsmapcanvas.h>
#include <qgssinglesymbolrenderer.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorlayer.h>

namespace {
bool orange(QRgb pixel) {
  return qRed(pixel) > 130 && qRed(pixel) < 230 && qGreen(pixel) < 120 && qBlue(pixel) < 60;
}

QRect orangeBounds(const QImage& image) {
  QRect bounds;
  for (int y = 0; y < image.height(); ++y)
    for (int x = 0; x < image.width(); ++x)
      if (orange(image.pixel(x, y))) bounds |= QRect(x, y, 1, 1);
  return bounds;
}
}

class TestAboveLabels : public QObject {
  Q_OBJECT
private slots:
  void overlayMatchesCanvas_data();
  void overlayMatchesCanvas();
  void cacheUsesCanvasPixelsAndRedrawsOnlyOnChange();
};

void TestAboveLabels::overlayMatchesCanvas_data() {
  QTest::addColumn<QString>("operation");
  QTest::newRow("initial") << QStringLiteral("initial");
  QTest::newRow("pan") << QStringLiteral("pan");
  QTest::newRow("zoom") << QStringLiteral("zoom");
  QTest::newRow("resize") << QStringLiteral("resize");
}

void TestAboveLabels::overlayMatchesCanvas() {
  QFETCH(QString, operation);
  QgsVectorLayer ring(QStringLiteral("LineString?crs=EPSG:5187"), QStringLiteral("500m"),
                      QStringLiteral("memory"));
  QVERIFY(ring.isValid());
  QgsFeature feature;
  feature.setGeometry(QgsGeometry::fromWkt(QStringLiteral(
      "LINESTRING(200400 450250,200600 450250,200600 450400,200400 450400,200400 450250)")));
  QgsFeatureList features{feature};
  QVERIFY(ring.dataProvider()->addFeatures(features));
  ring.updateExtents();
  auto symbol = QgsLineSymbol::createSimple({{QStringLiteral("line_color"), QStringLiteral("194,65,12,255")},
                                            {QStringLiteral("line_width"), QStringLiteral("1.15")}});
  ring.setRenderer(new QgsSingleSymbolRenderer(symbol.release()));
  ring.setCustomProperty(QStringLiteral("rendering/renderAboveLabels"), true);

  QgsMapCanvas canvas;
  canvas.setFrameShape(QFrame::NoFrame);
  canvas.resize(800, 600);
  canvas.setParallelRenderingEnabled(false);
  canvas.setCachingEnabled(true);
  canvas.setPreviewJobsEnabled(false);
  canvas.setCanvasColor(Qt::white);
  canvas.setDestinationCrs(ring.crs());
  auto* overlay = new KaAboveLabelsOverlay(&canvas);
  canvas.setLayers({&ring});
  canvas.show();
  QCoreApplication::processEvents();
  canvas.setExtent(QgsRectangle(200000, 450000, 200800, 450600));
  LayerOps::applyCanvasScreenDpi(&canvas);
  QSignalSpy rendered(&canvas, &QgsMapCanvas::mapCanvasRefreshed);
  canvas.refresh();
  QTRY_VERIFY_WITH_TIMEOUT(!rendered.isEmpty(), 10000);
  QTRY_VERIFY_WITH_TIMEOUT(!canvas.isDrawing(), 10000);
  overlay->setLayers({&ring});
  // Fill the cache before moving/resizing, as in an existing field workspace.
  (void)canvas.grab();

  overlay->setVisible(false);
  rendered.clear();
  if (operation == QLatin1String("pan"))
    canvas.setExtent(QgsRectangle(200100, 450050, 200900, 450650));
  else if (operation == QLatin1String("zoom"))
    canvas.setExtent(QgsRectangle(200200, 450150, 200700, 450525));
  else if (operation == QLatin1String("resize"))
    canvas.resize(1000, 650);
  LayerOps::applyCanvasScreenDpi(&canvas);
  canvas.refresh();
  QTRY_VERIFY_WITH_TIMEOUT(!rendered.isEmpty(), 10000);
  QTRY_VERIFY_WITH_TIMEOUT(!canvas.isDrawing(), 10000);
  const QImage base = canvas.grab().toImage().convertToFormat(QImage::Format_RGB32);
  overlay->setVisible(true);
  const QImage combined = canvas.grab().toImage().convertToFormat(QImage::Format_RGB32);
  const QString output = qEnvironmentVariable("QA_OUTPUT_DIR");
  if (!output.isEmpty()) {
    QDir().mkpath(output);
    base.save(QDir(output).filePath(operation + QStringLiteral("-base.png")));
    combined.save(QDir(output).filePath(operation + QStringLiteral("-overlay.png")));
  }
  const QRect before = orangeBounds(base), after = orangeBounds(combined);
  QVERIFY(!before.isEmpty());
  qInfo() << "DPR" << canvas.devicePixelRatioF() << "DPI" << canvas.mapSettings().outputDpi()
          << "base" << before << "with overlay" << after;
  QVERIFY2(std::abs(before.left() - after.left()) <= 3 &&
           std::abs(before.top() - after.top()) <= 3 &&
           std::abs(before.right() - after.right()) <= 3 &&
           std::abs(before.bottom() - after.bottom()) <= 3,
           "The second pass must not add a shifted or resized copy of the same boundary");
  int outside = 0, count = 0;
  for (int y = 0; y < combined.height(); ++y) {
    for (int x = 0; x < combined.width(); ++x) {
      if (!orange(combined.pixel(x, y))) continue;
      ++count;
      bool nearby = false;
      for (int dy = -3; dy <= 3 && !nearby; ++dy)
        for (int dx = -3; dx <= 3 && !nearby; ++dx)
          if (base.rect().contains(x + dx, y + dy) && orange(base.pixel(x + dx, y + dy))) nearby = true;
      if (!nearby) ++outside;
    }
  }
  QVERIFY2(count > 0 && outside == 0, qPrintable(QStringLiteral("misaligned boundary pixels: %1/%2").arg(outside).arg(count)));
}

void TestAboveLabels::cacheUsesCanvasPixelsAndRedrawsOnlyOnChange() {
  QgsVectorLayer ring(QStringLiteral("LineString?crs=EPSG:5187"), QStringLiteral("구역"), QStringLiteral("memory"));
  QgsFeature feature;
  feature.setGeometry(QgsGeometry::fromWkt(QStringLiteral("LINESTRING(200400 450250,200600 450400)")));
  QgsFeatureList features{feature};
  QVERIFY(ring.dataProvider()->addFeatures(features));
  ring.updateExtents();
  QgsMapCanvas canvas;
  canvas.setFrameShape(QFrame::NoFrame);
  canvas.resize(640, 480);
  canvas.setParallelRenderingEnabled(false);
  canvas.setPreviewJobsEnabled(false);
  canvas.setDestinationCrs(ring.crs());
  auto* overlay = new KaAboveLabelsOverlay(&canvas);
  canvas.setLayers({&ring});
  canvas.show();
  QCoreApplication::processEvents();
  canvas.setExtent(QgsRectangle(200000, 450000, 200800, 450600));
  LayerOps::applyCanvasScreenDpi(&canvas);
  QSignalSpy rendered(&canvas, &QgsMapCanvas::mapCanvasRefreshed);
  canvas.refresh();
  QTRY_VERIFY_WITH_TIMEOUT(!rendered.isEmpty(), 10000);
  overlay->setLayers({&ring});
  (void)canvas.grab();
  const qreal dpr = canvas.mapSettings().devicePixelRatio();
  qInfo() << "DPR" << dpr << "cache" << overlay->cachePixelSize();
  // Same physical resolution as the base map: sharp at 125-200 %.
  QCOMPARE(overlay->cacheDevicePixelRatio(), dpr);
  QVERIFY(qAbs(overlay->cachePixelSize().width() - qRound(canvas.width() * dpr)) <= 1);
  const int built = overlay->rebuildCount();
  QVERIFY(built >= 1);
  // A finished base-map render alone does not redraw the overlay.
  rendered.clear();
  canvas.refresh();
  QTRY_VERIFY_WITH_TIMEOUT(!rendered.isEmpty(), 10000);
  (void)canvas.grab();
  QCOMPARE(overlay->rebuildCount(), built);
  // Handing in the same list again (the label stack is re-applied often) keeps it too.
  overlay->setLayers({&ring});
  (void)canvas.grab();
  QCOMPARE(overlay->rebuildCount(), built);
  // A real change of the layer does.
  ring.triggerRepaint();
  (void)canvas.grab();
  QCOMPARE(overlay->rebuildCount(), built + 1);
}

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH", "D:/OSGeo4W/apps/qgis-dev"), true);
  QgsApplication::initQgis();
  TestAboveLabels test;
  const int result = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis();
  return result;
}

#include "test_above_labels.moc"
