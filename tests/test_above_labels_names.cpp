// 유적 이름이 「오외도지석묘1호호」처럼 두 번 겹쳐 찍히던 일(2026-10-02 R83). 덧그림(KaAboveLabelsOverlay)은
// 본 지도에서 빠진 유산·조사 레이어를 라벨 위에 다시 그리고 이름도 직접 쓴다. 레이어 창 연결(bridge)이 그
// 레이어를 본 지도에 다시 넣으면 본 지도와 덧그림이 같은 이름을 서로 다른 자리에 한 번씩 썼다.
#include <QtTest>
#include <QImage>

#include "app/KaAboveLabelsOverlay.h"
#include "core/LayerOps.h"

#include <qgsapplication.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgslabelingresults.h>
#include <qgslayertree.h>
#include <qgslayertreemapcanvasbridge.h>
#include <qgsmapcanvas.h>
#include <qgsmarkersymbol.h>
#include <qgspallabeling.h>
#include <qgsproject.h>
#include <qgssinglesymbolrenderer.h>
#include <qgstextformat.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayerlabeling.h>

namespace {
const QString kPointUri = QStringLiteral("Point?crs=EPSG:5187&field=name:string");

bool ink(QRgb pixel) { return qRed(pixel) < 90 && qGreen(pixel) < 90 && qBlue(pixel) < 90; }

// Dark text pixels the overlay added: ink in the combined picture with no ink within 1 px in the base map.
int addedInk(const QImage& base, const QImage& combined) {
  int added = 0;
  for (int y = 0; y < combined.height(); ++y)
    for (int x = 0; x < combined.width(); ++x) {
      if (!ink(combined.pixel(x, y))) continue;
      bool nearby = false;
      for (int dy = -1; dy <= 1 && !nearby; ++dy)
        for (int dx = -1; dx <= 1 && !nearby; ++dx)
          nearby = base.rect().contains(x + dx, y + dy) && ink(base.pixel(x + dx, y + dy));
      if (!nearby) ++added;
    }
  return added;
}

// A named point with an opaque purple marker, so the marker never counts as text ink.
void addNamedPoint(QgsVectorLayer& layer, const QgsPointXY& at, const QString& name) {
  QgsFeature feature(layer.fields());
  feature.setGeometry(QgsGeometry::fromPointXY(at));
  feature.setAttribute(0, name);
  QgsFeatureList features{feature};
  QVERIFY(layer.dataProvider()->addFeatures(features));
  layer.updateExtents();
  auto marker = QgsMarkerSymbol::createSimple({{QStringLiteral("color"), QStringLiteral("142,68,173,255")},
                                              {QStringLiteral("outline_color"), QStringLiteral("142,68,173,255")}});
  layer.setRenderer(new QgsSingleSymbolRenderer(marker.release()));
}

void prepareCanvas(QgsMapCanvas& canvas) {
  canvas.setFrameShape(QFrame::NoFrame);
  canvas.resize(800, 600);
  canvas.setParallelRenderingEnabled(false);
  canvas.setPreviewJobsEnabled(false);
  canvas.setCanvasColor(Qt::white);
  canvas.setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
}

void renderAndWait(QgsMapCanvas& canvas) {
  QSignalSpy rendered(&canvas, &QgsMapCanvas::mapCanvasRefreshed);
  canvas.refresh();
  QTRY_VERIFY_WITH_TIMEOUT(!rendered.isEmpty(), 10000);
  QTRY_VERIFY_WITH_TIMEOUT(!canvas.isDrawing(), 10000);
}
}  // namespace

class TestAboveLabelsNames : public QObject {
  Q_OBJECT
private slots:
  void sameNameLabelledOnceWhenCanvasAlsoDrawsLayer();
  void layerTreeBridgeKeepsOverlayLayersOffTheMap();
};

void TestAboveLabelsNames::sameNameLabelledOnceWhenCanvasAlsoDrawsLayer() {
  QgsVectorLayer heritage(kPointUri, QStringLiteral("국가지정유산"), QStringLiteral("memory"));
  addNamedPoint(heritage, QgsPointXY(200500, 450300), QStringLiteral("오외도지석묘1호"));
  QVERIFY(LayerOps::applyNameAttributeLabels(&heritage, QStringLiteral("name"), 7., false));

  QgsMapCanvas canvas;
  prepareCanvas(canvas);
  auto* overlay = new KaAboveLabelsOverlay(&canvas);
  canvas.setLayers({&heritage});
  canvas.show();
  QCoreApplication::processEvents();
  canvas.setExtent(QgsRectangle(200300, 450150, 200700, 450450));
  LayerOps::applyCanvasScreenDpi(&canvas);
  renderAndWait(canvas);
  // A jibun label on the name's preferred spot makes an engine that also sees jibun move the name.
  QgsRectangle preferred;
  for (const QgsLabelPosition& label : canvas.labelingResults()->allLabels())
    if (label.layerID == heritage.id()) preferred = label.labelRect;
  QVERIFY(!preferred.isEmpty());
  QgsVectorLayer jibun(kPointUri, QStringLiteral("연속지적도"), QStringLiteral("memory"));
  addNamedPoint(jibun, preferred.center(), QStringLiteral("산12-3"));
  QgsPalLayerSettings jibunLabel;
  jibunLabel.fieldName = QStringLiteral("name");
  jibunLabel.placement = Qgis::LabelPlacement::OverPoint;
  jibunLabel.priority = 10;
  QgsTextFormat format;
  format.setSize(9);
  format.setColor(QColor(31, 35, 40));
  jibunLabel.setFormat(format);
  jibun.setLabeling(new QgsVectorLayerSimpleLabeling(jibunLabel));
  jibun.setLabelsEnabled(true);

  const auto added = [&] {
    overlay->setVisible(false);
    const QImage base = canvas.grab().toImage().convertToFormat(QImage::Format_RGB32);
    overlay->setVisible(true);
    return addedInk(base, canvas.grab().toImage().convertToFormat(QImage::Format_RGB32));
  };
  // syncMapCanvas state: the main map leaves the layer out, so the overlay writes its name.
  canvas.setLayers({&jibun});
  overlay->setLayers({&heritage});
  renderAndWait(canvas);
  const int alone = added();
  QVERIFY2(alone > 30, qPrintable(QStringLiteral("overlay name ink: %1").arg(alone)));

  // A moment where the main map draws the layer again (before it is taken out): it labels the name itself.
  canvas.setLayers({&jibun, &heritage});
  renderAndWait(canvas);
  bool labelled = false;
  for (const QgsLabelPosition& label : canvas.labelingResults()->allLabels())
    labelled = labelled || (label.layerID == heritage.id() && label.labelText == QStringLiteral("오외도지석묘1호"));
  QVERIFY(labelled);
  const int twice = added();
  QVERIFY2(twice <= 3, qPrintable(QStringLiteral("the overlay must not write a second copy of a name the map "
                                                 "already labels: %1 extra ink pixels").arg(twice)));
}

// 원인: 레이어 창 연결은 체크를 바꾸거나 묶음을 더할 때마다 보이는 레이어를 모두 본 지도 목록에 다시 넣는다.
// 덧그림 대상(지정유산 등)은 그때마다 다시 빠져야 이름을 덧그림 한 곳만 쓴다.
void TestAboveLabelsNames::layerTreeBridgeKeepsOverlayLayersOffTheMap() {
  QgsProject* project = QgsProject::instance();
  auto* heritage = new QgsVectorLayer(kPointUri, QStringLiteral("지정유산"), QStringLiteral("memory"));
  addNamedPoint(*heritage, QgsPointXY(200500, 450300), QStringLiteral("오외도지석묘1호"));
  QVERIFY(LayerOps::applyNameAttributeLabels(heritage, QStringLiteral("name"), 7., false));
  auto* other = new QgsVectorLayer(kPointUri, QStringLiteral("참고 점"), QStringLiteral("memory"));
  addNamedPoint(*other, QgsPointXY(200400, 450200), QString());
  project->addMapLayers({heritage, other});
  QVERIFY(LayerOps::layersDrawnAboveLabels(project).contains(heritage));

  QgsMapCanvas canvas;
  prepareCanvas(canvas);
  new KaAboveLabelsOverlay(&canvas);
  QgsLayerTreeMapCanvasBridge* bridge =
      KaAboveLabelsOverlay::makeLayerTreeBridge(project->layerTreeRoot(), &canvas, &canvas);
  QVERIFY(bridge);
  canvas.setLayers(LayerOps::sheetBasePaintLayers(project));
  QVERIFY(!canvas.layers().contains(heritage));

  // Unchecking an unrelated layer makes the bridge reset the whole map list.
  QSignalSpy reset(bridge, &QgsLayerTreeMapCanvasBridge::canvasLayersChanged);
  project->layerTreeRoot()->findLayer(other->id())->setItemVisibilityChecked(false);
  QTRY_VERIFY_WITH_TIMEOUT(!reset.isEmpty(), 5000);
  QVERIFY(!canvas.layers().contains(other));
  // While the map is drawing the layer is taken out after the draw, so give it a moment.
  QTRY_VERIFY2_WITH_TIMEOUT(!canvas.layers().contains(heritage), "the overlay paints this layer and writes its "
                            "names; the main map must not get it back from the layer tree bridge", 5000);
  project->removeAllMapLayers();
}

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH", "D:/OSGeo4W/apps/qgis-dev"), true);
  QgsApplication::initQgis();
  TestAboveLabelsNames test;
  const int result = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis();
  return result;
}

#include "test_above_labels_names.moc"
