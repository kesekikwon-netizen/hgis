// 도형선택에서 Ctrl+C · Ctrl+V 로 지적 필지를 같은 자리에
// 복사한다(docs/intent/2026-10-10-cadastral-own-section-copy-paste.md). 복사·붙이기 자체는 test_parcel_copy.cpp.
#include <QApplication>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QtTest>

#include <qgsapplication.h>
#include <qgsmapcanvas.h>
#include <qgsmaptoolpan.h>
#include <qgsproject.h>
#include <qgsrectangle.h>
#include <qgsvectorlayer.h>

#include "app/KaCaptureMapTool.h"
#include "app/KaFeatureSelectTool.h"
#include "app/KaParcelCopyKeys.h"
#include "core/LayerOps.h"
#include "core/ParcelClipboard.h"
#include "core/SnapTraceOps.h"
#include "parcel_copy_fixture.h"
#include <QFile>
#include <qgsmapmouseevent.h>
#include <qgsmaptopixel.h>

using namespace ParcelCopyFixture;

class ParcelCopyKeysTest : public QObject {
  Q_OBJECT
 private slots:
  void holdingCtrlVPastesOnce() {
    QgsProject* project = QgsProject::instance();
    project->clear();
    QgsVectorLayer* received = receivedMap(*project, QStringLiteral("EPSG:5187"), square(200000, 450000, 10));
    QgsMapCanvas canvas;
    canvas.setRenderFlag(false);
    canvas.setLayers({received});
    KaParcelCopyKeys keys(&canvas);
    KaFeatureSelectTool tool(&canvas);
    canvas.setMapTool(&tool);
    received->selectByIds({firstFid(received)});
    pressCtrl(canvas, Qt::Key_C);
    pressCtrl(canvas, Qt::Key_V);
    for (int i = 0; i < 5; ++i) {  // 키를 누르고 있으면 오는 반복 입력
      QKeyEvent repeat(QEvent::KeyPress, Qt::Key_V, Qt::ControlModifier, QString(), true);
      QApplication::sendEvent(&canvas, &repeat);
    }
    QCOMPARE(int(featuresOf(ParcelClipboard::copyLayer(project)).size()), 1);
    canvas.setLayers({});
    project->clear();
  }

  void ctrlCAndCtrlVInSelectToolPasteWhereTheParcelWas() {
    QgsProject* project = QgsProject::instance();
    project->clear();
    QgsVectorLayer* received = receivedMap(*project, QStringLiteral("EPSG:5187"), square(200000, 450000, 10));

    QgsMapCanvas canvas;
    canvas.setRenderFlag(false);
    canvas.setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    canvas.resize(640, 480);
    canvas.setExtent(QgsRectangle(199990, 449990, 200020, 450020));
    canvas.setLayers({received});
    KaParcelCopyKeys keys(&canvas);
    KaFeatureSelectTool tool(&canvas);
    canvas.setMapTool(&tool);
    received->selectByIds({firstFid(received)});

    pressCtrl(canvas, Qt::Key_C);
    pressCtrl(canvas, Qt::Key_V);

    QgsVectorLayer* copy = ParcelClipboard::copyLayer(project);
    QVERIFY2(copy, "Ctrl+V in 도형선택 pastes the copied parcel");
    QCOMPARE(int(featuresOf(copy).size()), 1);
    canvas.setLayers({});
    project->clear();
  }

  void ctrlCAndCtrlVDoNothingOutsideSelectTool() {
    QgsProject* project = QgsProject::instance();
    project->clear();
    QgsVectorLayer* received = receivedMap(*project, QStringLiteral("EPSG:5187"), square(200000, 450000, 10));
    addParcel(received, square(200020, 450000, 10), QStringLiteral("2222222222222222222"));
    QgsFeatureId parcelA = -1;  // 왼쪽 필지
    QgsFeatureId parcelB = -1;  // 오른쪽 필지
    QgsFeature feature;
    for (auto it = received->getFeatures(); it.nextFeature(feature);)
      (feature.geometry().boundingBox().xMinimum() < 200010 ? parcelA : parcelB) = feature.id();

    QgsMapCanvas canvas;
    canvas.setRenderFlag(false);
    canvas.setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    canvas.resize(640, 480);
    canvas.setExtent(QgsRectangle(199990, 449990, 200040, 450020));
    canvas.setLayers({received});
    KaParcelCopyKeys keys(&canvas);
    KaFeatureSelectTool tool(&canvas);
    canvas.setMapTool(&tool);
    received->selectByIds({parcelA});
    pressCtrl(canvas, Qt::Key_C);  // 도형선택에서는 A를 담는다
    pressCtrl(canvas, Qt::Key_V);
    QVERIFY2(ParcelClipboard::copyLayer(project), "the select tool copies and pastes A first");

    QgsMapToolPan pan(&canvas);
    canvas.setMapTool(&pan);
    received->selectByIds({parcelB});
    pressCtrl(canvas, Qt::Key_C);  // 도형선택이 아니므로 B를 담지 않는다
    pressCtrl(canvas, Qt::Key_V);  // 붙이지도 않는다

    canvas.setMapTool(&tool);
    pressCtrl(canvas, Qt::Key_V);  // 담긴 것은 아직 A뿐이다
    QgsVectorLayer* copy = ParcelClipboard::copyLayer(project);
    QVERIFY(copy);
    const QList<QgsFeature> copies = featuresOf(copy);
    QCOMPARE(int(copies.size()), 2);  // A 두 번, B는 없다
    for (const QgsFeature& copied : copies)
      QVERIFY(copied.geometry().centroid().distance(square(200000, 450000, 10).centroid()) < 0.01);
    canvas.setLayers({});
    project->clear();
  }

  void snapTraceShowsNearbyVerticesOnlyWhenCloseAndFillsSkippedVertex() {
    QgsProject* project = QgsProject::instance();
    project->clear();
    auto* cad = new QgsVectorLayer(QStringLiteral("LineString?crs=EPSG:5187"), QStringLiteral("사업자도면"),
                                   QStringLiteral("memory"));
    LayerOps::markReferenceLayer(cad);
    cad->setCustomProperty(QStringLiteral("ka_hgis/cad_drawing"), QStringLiteral("cad1"));
    QgsFeature f(cad->fields());
    f.setGeometry(QgsGeometry::fromPolylineXY(
        {QgsPointXY(200000, 450000), QgsPointXY(200010, 450001.2), QgsPointXY(200020, 450000)}));
    cad->dataProvider()->addFeature(f);
    cad->updateExtents();
    project->addMapLayer(cad);

    QgsMapCanvas canvas;
    canvas.setRenderFlag(false);
    canvas.setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    canvas.resize(640, 480);
    canvas.mapSettings().setOutputSize(QSize(640, 480));
    canvas.setExtent(QgsRectangle(199980, 449980, 200060, 450040));
    canvas.setLayers({cad});
    auto px = [&](double x, double y) {
      const QgsPointXY p = canvas.getCoordinateTransform()->transform(x, y);
      return QPoint(qRound(p.x()), qRound(p.y()));
    };

    KaCaptureMapTool tool(&canvas);
    tool.setMode(KaCaptureMapTool::Mode::Line);
    tool.setSnapTrace(true);
    tool.setSnapPreview(true);

    QgsMapMouseEvent farMove(&canvas, QEvent::MouseMove, px(200050, 450035), Qt::NoButton, Qt::NoButton);
    tool.canvasMoveEvent(&farMove);
    QCOMPARE(tool.previewMarkerCount(), 0);  // 마우스가 멀리 있을 때는 자석점 미리보기를 숨겨 어지럽지 않음

    QgsMapMouseEvent nearMove(&canvas, QEvent::MouseMove, px(200010, 450002), Qt::NoButton, Qt::NoButton);
    tool.canvasMoveEvent(&nearMove);
    QVERIFY2(tool.previewMarkerCount() >= 1, "마우스가 가까이 갔을 때 주변 자석점이 미리 표시되어야 한다");

    QgsMapMouseEvent p1(&canvas, QEvent::MouseButtonPress, px(200000, 450000), Qt::LeftButton, Qt::LeftButton);
    tool.canvasPressEvent(&p1);
    QgsMapMouseEvent p2(&canvas, QEvent::MouseButtonPress, px(200020, 450000), Qt::LeftButton, Qt::LeftButton);
    tool.canvasPressEvent(&p2);
    QCOMPARE(tool.pointCount(), 3);  // 빠뜨린 중간 자석점 (200010, 450001.2)가 자동으로 채워짐

    // 지적도 위에 대략 그린 조사구역 도형이 버튼을 누르면 가장 가까운 지적선에 붙고 사이 꼭짓점도 채워져야 한다.
    auto* area = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187"), QStringLiteral("조사구역"),
                                    QStringLiteral("memory"));
    LayerOps::markSurveyLayer(area, QStringLiteral("survey_area"));
    {
      QgsFeature rough(area->fields());
      rough.setGeometry(QgsGeometry::fromPolygonXY(
          {{QgsPointXY(200001.2, 449999.1), QgsPointXY(200019.0, 450000.8), QgsPointXY(200020.8, 450019.2),
            QgsPointXY(199999.3, 450020.6), QgsPointXY(200001.2, 449999.1)}}));
      area->dataProvider()->addFeature(rough);
    }
    project->addMapLayer(area);
    QString snapMsg;
    QCOMPARE(SnapTraceOps::snapCanvasShapesToNearestLines(&canvas, nullptr, FID_NULL, &snapMsg), 1);
    QgsFeature snappedFeat;
    area->getFeatures().nextFeature(snappedFeat);
    const QgsPolygonXY snappedPoly = snappedFeat.geometry().asPolygon();
    QVERIFY(!snappedPoly.isEmpty());
    QCOMPARE(snappedPoly[0].size(), 6);  // 4개 모서리 스냅 + 빠진 중간 꼭짓점 (200010, 450001.2) 자동 삽입 + 닫힘점

    QFile sketchSrc(QStringLiteral("src/app/KaDrawSketchTools.cpp"));
    if (sketchSrc.open(QIODevice::ReadOnly | QIODevice::Text)) {
      const QString code = QString::fromUtf8(sketchSrc.readAll());
      QVERIFY(code.contains(QLatin1String("actSnapToLines")) && code.contains(QStringLiteral("가까운 선에 붙이기")));
      QVERIFY(code.contains(QLatin1String("actSnapTrace")) && code.contains(QStringLiteral("선 따라 자동채움")));
      QVERIFY(code.contains(QLatin1String("actSnapPreview")) && code.contains(QStringLiteral("근접 자석점 표시")));
    }
    canvas.setLayers({});
    project->clear();
  }

  // 시 전체 지적(필지 수만 개)을 띄운 채 그리기 도구로 마우스를 움직여도 화면이 멎지 않아야 한다
  // (2026-10-16 「최신 포터블이 심하게 버벅인다」). 근접 자석점 표시·선 따라 자동채움은 마우스가
  // 움직일 때마다 돈다. 고치기 전: 1 px = 0.5 m 에서 30 ms, 2 m 에서 48 ms, 8 m 에서 98 ms.
  void snapTraceMouseMoveStaysFastOverDenseCadastral() {
    QgsProject* project = QgsProject::instance();
    project->clear();
    auto* cad = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187"), QStringLiteral("지적"),
                                   QStringLiteral("memory"));
    LayerOps::markCadastralLayer(cad);
    QgsFeatureList parcels;
    const double x0 = 200000, y0 = 450000, cell = 12.0;
    for (int i = 0; i < 160; ++i)
      for (int j = 0; j < 160; ++j) {
        QgsFeature f(cad->fields());
        const double x = x0 + i * cell, y = y0 + j * cell;
        f.setGeometry(QgsGeometry::fromPolygonXY({{QgsPointXY(x, y), QgsPointXY(x + cell, y),
                                                   QgsPointXY(x + cell, y + cell * 0.5), QgsPointXY(x + cell, y + cell),
                                                   QgsPointXY(x, y + cell), QgsPointXY(x, y)}}));
        parcels << f;
      }
    cad->dataProvider()->addFeatures(parcels);
    cad->dataProvider()->createSpatialIndex();  // 지적 GPKG처럼 공간 색인이 있다
    cad->updateExtents();
    project->addMapLayer(cad);

    QgsMapCanvas canvas;
    canvas.setRenderFlag(false);
    canvas.setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    canvas.resize(1600, 1000);
    canvas.mapSettings().setOutputSize(QSize(1600, 1000));
    canvas.setLayers({cad});
    const QgsPointXY mid(x0 + 960, y0 + 960);

    // 그리는 축척(1 px = 0.1~2 m)은 20 ms, 시 전체를 넓게 본 화면(1 px = 8 m)은 70 ms 안. 여유를 둔 기준이다.
    for (const double mupp : {8.0, 2.0, 0.5, 0.1}) {
      canvas.setExtent(QgsRectangle(mid.x() - 800 * mupp, mid.y() - 500 * mupp, mid.x() + 800 * mupp,
                                    mid.y() + 500 * mupp));
      QElapsedTimer t;
      t.start();
      for (int k = 0; k < 10; ++k) {
        const QgsPointXY cur(mid.x() + k * 3.1, mid.y() + k * 1.7);
        SnapTraceOps::nearbyVertices(&canvas, cur);
        SnapTraceOps::traceBetween(&canvas, QgsPointXY(mid.x() - 400 * mupp, mid.y()), cur);
      }
      const qint64 perMoveMs = t.elapsed() / 10;
      const qint64 limitMs = mupp > 4.0 ? 70 : 20;
      QVERIFY2(perMoveMs <= limitMs, qPrintable(QStringLiteral("마우스 한 번 움직임에 %1 ms (1 px = %2 m, 기준 %3 ms)")
                                                    .arg(perMoveMs)
                                                    .arg(mupp)
                                                    .arg(limitMs)));
    }
    canvas.setLayers({});
    project->clear();
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  ParcelCopyKeysTest test;
  const int rc = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
#include "test_parcel_copy_keys.moc"
