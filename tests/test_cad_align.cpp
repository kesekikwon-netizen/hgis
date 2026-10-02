// 도면(CAD 변환본) 정합: 같은 도면의 레이어를 모두 같이 맞추고 원본 옆 _aligned.gpkg 를 만들지 않는다.
// 맞추는 중에 「맞추기」를 다시 누르거나 같은 도면을 다시 불러와도 꺼지지 않고, 작업 좌표계가 바뀐 뒤에도 제자리에 쓴다.
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

#include "app/KaAlignMapTool.h"
#include "core/CadDrawingLayers.h"
#include "core/CadDrawingStore.h"

#include <qgsapplication.h>
#include <qgscoordinatetransform.h>
#include <qgsfeature.h>
#include <qgslayertree.h>
#include <qgsmapcanvas.h>
#include <qgsmapmouseevent.h>
#include <qgsmaptopixel.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

namespace {

const QgsRectangle kExtent(200000, 450000, 200400, 450300);
const QString kDrawingId = QStringLiteral("drawing-1");

QgsCoordinateReferenceSystem crs(const char* authId) { return QgsCoordinateReferenceSystem(QString::fromLatin1(authId)); }
QgsCoordinateReferenceSystem workCrs() { return crs("EPSG:5186"); }

// 400×300 픽셀에 400×300 m: 한 픽셀이 1 m 다.
struct Canvas {
  QgsMapCanvas canvas;
  Canvas() {
    canvas.setRenderFlag(false);
    canvas.setDestinationCrs(workCrs());
    canvas.resize(400, 300);
    canvas.mapSettings().setOutputSize(QSize(400, 300));
    canvas.setExtent(kExtent);
  }
  QPoint px(double x, double y) {
    const QgsPointXY p = canvas.getCoordinateTransform()->transform(x, y);
    return QPoint(qRound(p.x()), qRound(p.y()));
  }
};

// 왼쪽(도면 숫자) 점, 이어서 오른쪽 지도의 같은 점.
void pickPair(KaAlignMapTool& tool, Canvas& c, double sx, double sy, double mx, double my) {
  tool.setSourcePoint(sx, sy);
  QgsMapMouseEvent press(&c.canvas, QEvent::MouseButtonPress, c.px(mx, my), Qt::LeftButton, Qt::LeftButton);
  tool.canvasPressEvent(&press);
}

// 로컬 숫자 도면(선 하나·글자 하나)을 drawingCrs 변환본으로 써서 참조 지도 「로컬 (도면)」 묶음으로 올린다.
QList<QgsVectorLayer*> addLocalDrawing(const QTemporaryDir& dir, QString* gpkgOut,
                                       const QgsCoordinateReferenceSystem& drawingCrs = workCrs()) {
  CadDrawing drawing;
  drawing.entities = {
      CadEntity{.kind = CadKind::Line, .geometry = QgsGeometry::fromPolylineXY({QgsPointXY(10, 10), QgsPointXY(90, 10)})},
      CadEntity{.kind = CadKind::Text, .geometry = QgsGeometry::fromPointXY(QgsPointXY(50, 50)),
                .text = QStringLiteral("374-1전"), .textHeight = 2.5, .textAngle = 30}};
  *gpkgOut = dir.filePath(QStringLiteral("로컬.gpkg"));
  QString error;
  const CadStoreInfo info{QStringLiteral("C:/x/로컬.dxf"), QStringLiteral("ab"), QString(), QString()};
  if (!CadDrawingStore::write(drawing, info, drawingCrs, QgsProject::instance()->transformContext(), *gpkgOut, &error))
    return {};
  return CadDrawingLayers::addToProject(QgsProject::instance(), *gpkgOut, QStringLiteral("로컬 (도면)"), kDrawingId,
                                        &error);
}

QgsVectorLayer* named(const QList<QgsVectorLayer*>& layers, const QString& name) {
  for (QgsVectorLayer* layer : layers)
    if (layer->name() == name) return layer;
  return nullptr;
}

bool shown(const QgsMapLayer* layer) {
  const QgsLayerTreeLayer* node = QgsProject::instance()->layerTreeRoot()->findLayer(layer->id());
  return node && node->itemVisibilityChecked();
}

QgsPointXY firstPoint(const QString& gpkg, const QString& table) {
  QgsVectorLayer layer(gpkg + QStringLiteral("|layername=") + table, table, QStringLiteral("ogr"));
  QgsFeature f;
  layer.getFeatures().nextFeature(f);
  return QgsPointXY(f.geometry().vertexAt(0));
}

qsizetype cloneCount() { return QgsProject::instance()->mapLayersByName(QStringLiteral("선 맞춤")).size(); }

bool noAlignedCopy(const QTemporaryDir& dir) {
  const QStringList pattern{QStringLiteral("*_aligned*.gpkg")};
  return QDir(dir.path()).entryList(pattern, QDir::Files).isEmpty() &&
         QDir::current().entryList(pattern, QDir::Files).isEmpty();
}

}  // namespace

class TestCadAlign : public QObject {
  Q_OBJECT
 private slots:
  void init() { m_dir = std::make_unique<QTemporaryDir>(); }
  // 레이어를 먼저 닫고 임시 폴더를 지운다. Windows 는 열린 GPKG 가 든 폴더를 지우지 못한다.
  void cleanup() {
    QgsProject::instance()->removeAllMapLayers();
    m_dir.reset();
  }

  void save_movesEveryTableAndWritesNoAlignedCopy() {
    const QTemporaryDir& dir = *m_dir;
    QString gpkg;
    const QList<QgsVectorLayer*> layers = addLocalDrawing(dir, &gpkg);
    QgsVectorLayer* lines = named(layers, QStringLiteral("선"));
    QgsVectorLayer* texts = named(layers, QStringLiteral("글자"));
    QVERIFY(lines && texts);
    Canvas c;
    KaAlignMapTool tool(&c.canvas);
    QString err;
    QVERIFY2(tool.beginLayer(lines, workCrs(), &err), qPrintable(err));
    QVERIFY(!shown(texts));  // 맞추는 동안 같은 도면의 다른 레이어는 숨긴다
    QCOMPARE(cloneCount(), 1);
    pickPair(tool, c, 0, 0, 200100, 450100);
    pickPair(tool, c, 100, 0, 200200, 450100);
    QString saved;
    QVERIFY2(tool.saveAligned(&saved, &err), qPrintable(err));
    QCOMPARE(QFileInfo(saved).absoluteFilePath(), QFileInfo(gpkg).absoluteFilePath());
    QVERIFY(!tool.hasSession());  // 도면은 저장하며 끝난다: 앱은 이것을 보고 맞추기 줄을 닫는다
    QCOMPARE(cloneCount(), 0);
    QVERIFY(shown(lines) && shown(texts));
    QVERIFY(noAlignedCopy(dir));
    const QgsPointXY start = firstPoint(gpkg, QStringLiteral("lines"));  // (10, 10) → (200110, 450110)
    QVERIFY2(start.distance(QgsPointXY(200110, 450110)) < 1, qPrintable(start.toString(2)));
    const QgsPointXY label = firstPoint(gpkg, QStringLiteral("texts"));  // (50, 50) → (200150, 450150)
    QVERIFY2(label.distance(QgsPointXY(200150, 450150)) < 1, qPrintable(label.toString(2)));
  }

  void cancel_removesTheCloneAndShowsEverything() {
    const QTemporaryDir& dir = *m_dir;
    QString gpkg;
    const QList<QgsVectorLayer*> layers = addLocalDrawing(dir, &gpkg);
    QgsVectorLayer* lines = named(layers, QStringLiteral("선"));
    QgsVectorLayer* texts = named(layers, QStringLiteral("글자"));
    QVERIFY(lines && texts);
    Canvas c;
    KaAlignMapTool tool(&c.canvas);
    QString err;
    QVERIFY2(tool.beginLayer(lines, workCrs(), &err), qPrintable(err));
    pickPair(tool, c, 0, 0, 200100, 450100);
    tool.endSession();
    QCOMPARE(cloneCount(), 0);
    QVERIFY(shown(lines) && shown(texts));
    QCOMPARE(firstPoint(gpkg, QStringLiteral("lines")), QgsPointXY(10, 10));
  }

  // 맞추는 중에 「맞추기」를 다시 누르면 지금 레이어(맞춤 복제본)가 넘어온다. 지운 복제본을 쓰지 않고 그 도면으로 다시 시작한다.
  void restartOnTheClone_beginsAgainOnTheDrawing() {
    const QTemporaryDir& dir = *m_dir;
    QString gpkg;
    QgsVectorLayer* lines = named(addLocalDrawing(dir, &gpkg), QStringLiteral("선"));
    QVERIFY(lines);
    Canvas c;
    KaAlignMapTool tool(&c.canvas);
    QString err;
    QVERIFY2(tool.beginLayer(lines, workCrs(), &err), qPrintable(err));
    QVERIFY(tool.targetLayer() && tool.targetLayer() != lines);
    QVERIFY2(tool.beginLayer(tool.targetLayer(), workCrs(), &err), qPrintable(err));
    QCOMPARE(cloneCount(), 1);
    QCOMPARE(tool.sourceDisplayLayer(), static_cast<QgsMapLayer*>(lines));
    tool.endSession();
    QCOMPARE(cloneCount(), 0);
  }

  // 맞추는 중에 같은 도면을 다시 불러와 레이어가 바뀌면 저장하지 않고 알리며, 끝낼 때 맞춤 복제본도 남기지 않는다.
  void drawingReplacedDuringAlign_refusesToSaveAndCleansUp() {
    const QTemporaryDir& dir = *m_dir;
    QString gpkg;
    QgsVectorLayer* lines = named(addLocalDrawing(dir, &gpkg), QStringLiteral("선"));
    QVERIFY(lines);
    Canvas c;
    KaAlignMapTool tool(&c.canvas);
    QString err;
    QVERIFY2(tool.beginLayer(lines, workCrs(), &err), qPrintable(err));
    pickPair(tool, c, 0, 0, 200100, 450100);
    pickPair(tool, c, 100, 0, 200200, 450100);
    CadDrawingLayers::removeFromProject(QgsProject::instance(), kDrawingId);
    QString saved;
    err.clear();
    QVERIFY(!tool.saveAligned(&saved, &err));
    QVERIFY(!err.isEmpty());
    QVERIFY(noAlignedCopy(dir));
    tool.endSession();
    QCOMPARE(cloneCount(), 0);
  }

  // 작업 좌표계가 도면을 올린 뒤 바뀌어도(도면 5187, 작업 5186) 맞춘 자리를 도면 좌표계 숫자로 쓴다.
  void save_afterTheWorkCrsChanged_writesTheDrawingCrs() {
    const QTemporaryDir& dir = *m_dir;
    QString gpkg;
    QgsVectorLayer* lines = named(addLocalDrawing(dir, &gpkg, crs("EPSG:5187")), QStringLiteral("선"));
    QVERIFY(lines);
    Canvas c;
    KaAlignMapTool tool(&c.canvas);
    QString err;
    QVERIFY2(tool.beginLayer(lines, workCrs(), &err), qPrintable(err));
    pickPair(tool, c, 0, 0, 200100, 450100);
    pickPair(tool, c, 100, 0, 200200, 450100);
    QString saved;
    QVERIFY2(tool.saveAligned(&saved, &err), qPrintable(err));
    const QgsPointXY expected = QgsCoordinateTransform(workCrs(), crs("EPSG:5187"), QgsProject::instance()->transformContext())
                                    .transform(QgsPointXY(200110, 450110));
    const QgsPointXY start = firstPoint(gpkg, QStringLiteral("lines"));
    QVERIFY2(start.distance(expected) < 1, qPrintable(start.toString(2) + QStringLiteral(" / ") + expected.toString(2)));
  }

 private:
  std::unique_ptr<QTemporaryDir> m_dir;
};

int main(int argc, char** argv) {
  QStandardPaths::setTestModeEnabled(true);
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  int result = 0;
  {
    TestCadAlign tests;
    result = QTest::qExec(&tests, argc, argv);
  }
  QgsApplication::exitQgis();
  return result;
}

#include "test_cad_align.moc"
