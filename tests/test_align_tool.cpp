#include <QtTest>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "app/KaAlignMapTool.h"
#include "core/CadDrawingLayers.h"
#include "core/CadDrawingStore.h"
#include "core/GeorefService.h"

#include <qgsapplication.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransformcontext.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgslayertree.h>
#include <qgsmapcanvas.h>
#include <qgsmapmouseevent.h>
#include <qgsmaptopixel.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>
#include <qgsrectangle.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectorlayer.h>

// 「맞추기」 tool contracts: 「되돌리기」 really puts a raster back (from the backup taken before
// the first write) and says honestly what happened; saving an aligned CAD never deletes an
// older <name>_aligned.gpkg.
class TestAlignTool : public QObject {
  Q_OBJECT
private slots:
  void rasterRestore_beforeAnyWrite_saysNothingChanged();
  void rasterRestore_afterMove_putsTheImageBack();
  void vectorSave_keepsAnOlderAlignedFile();
  void drawingSave_movesEveryTableAndWritesNoAlignedCopy();
  void drawingCancel_removesTheCloneAndShowsEverything();
  void cleanup() { QgsProject::instance()->removeAllMapLayers(); }
};

namespace {

const QgsRectangle kExtent(200000, 450000, 200400, 450300);

QgsCoordinateReferenceSystem workCrs() {
  return QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186"));
}

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

QString writeScan(const QTemporaryDir& dir) {
  const QString png = dir.filePath(QStringLiteral("scan.png"));
  QImage img(80, 40, QImage::Format_RGB32);
  img.fill(Qt::white);
  return img.save(png) ? png : QString();
}

// Left point (image pixels) then the matching right point on the map canvas.
void pickPair(KaAlignMapTool& tool, Canvas& c, double sx, double sy, double mx, double my) {
  tool.setSourcePoint(sx, sy);
  QgsMapMouseEvent press(&c.canvas, QEvent::MouseButtonPress, c.px(mx, my), Qt::LeftButton,
                         Qt::LeftButton);
  tool.canvasPressEvent(&press);
}

// 로컬 숫자 도면(선 하나·글자 하나)을 변환본으로 써서 참조 지도 「로컬 (도면)」 묶음으로 올린다.
QList<QgsVectorLayer*> addLocalDrawing(const QTemporaryDir& dir, QString* gpkgOut) {
  CadDrawing drawing;
  drawing.entities = {
      CadEntity{.kind = CadKind::Line, .geometry = QgsGeometry::fromPolylineXY({QgsPointXY(10, 10), QgsPointXY(90, 10)})},
      CadEntity{.kind = CadKind::Text, .geometry = QgsGeometry::fromPointXY(QgsPointXY(50, 50)),
                .text = QStringLiteral("374-1전"), .textHeight = 2.5, .textAngle = 30}};
  *gpkgOut = dir.filePath(QStringLiteral("로컬.gpkg"));
  QString error;
  if (!CadDrawingStore::write(drawing, CadStoreInfo{QStringLiteral("C:/x/로컬.dxf"), QStringLiteral("ab"), QString(), QString()},
                              workCrs(), QgsProject::instance()->transformContext(), *gpkgOut, &error))
    return {};
  return CadDrawingLayers::addToProject(QgsProject::instance(), *gpkgOut, QStringLiteral("로컬 (도면)"),
                                        QStringLiteral("drawing-1"), &error);
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

}  // namespace

void TestAlignTool::rasterRestore_beforeAnyWrite_saysNothingChanged() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString png = writeScan(dir);
  QVERIFY(!png.isEmpty());
  auto* rl = new QgsRasterLayer(png, QStringLiteral("scan"), QStringLiteral("gdal"));
  QVERIFY(rl->isValid());
  QgsProject::instance()->addMapLayer(rl);
  Canvas c;
  KaAlignMapTool tool(&c.canvas);
  tool.setBackupRoot(dir.filePath(QStringLiteral("정합백업")));
  QString err;
  QVERIFY2(tool.beginLayer(rl, workCrs(), &err), qPrintable(err));

  QString message;
  QVERIFY(tool.restoreOriginals(&message));
  QVERIFY2(message.contains(QStringLiteral("아직 바뀌지 않았습니다")), qPrintable(message));
  QVERIFY(tool.backupFolder().isEmpty());
  QVERIFY(!QFile::exists(GeorefService::worldFilePathFor(png)));
  QVERIFY2(!QFileInfo::exists(dir.filePath(QStringLiteral("정합백업"))),
           "nothing was written, so nothing is backed up");
  tool.endSession();
  QgsProject::instance()->removeAllMapLayers();  // files close before the temp dir goes
}

void TestAlignTool::rasterRestore_afterMove_putsTheImageBack() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString png = writeScan(dir);
  QVERIFY(!png.isEmpty());
  auto* rl = new QgsRasterLayer(png, QStringLiteral("scan"), QStringLiteral("gdal"));
  QVERIFY(rl->isValid());
  QgsProject::instance()->addMapLayer(rl);
  const QString id = rl->id();
  QVERIFY(GeorefService::looksUnreferencedRaster(rl));
  Canvas c;
  KaAlignMapTool tool(&c.canvas);
  tool.setBackupRoot(dir.filePath(QStringLiteral("정합백업")));
  QString err;
  QVERIFY2(tool.beginLayer(rl, workCrs(), &err), qPrintable(err));

  pickPair(tool, c, 0, 0, 200100, 450200);
  pickPair(tool, c, 80, 40, 200260, 450120);
  QCOMPARE(tool.pairCount(), 2);
  QVERIFY2(tool.applyMove(&err), qPrintable(err));
  QCOMPARE(tool.targetLayer(), rl);  // same layer object, not a rebuilt one
  QVERIFY2(!GeorefService::looksUnreferencedRaster(rl), qPrintable(rl->extent().toString(2)));
  const QString wld = GeorefService::worldFilePathFor(png);
  QVERIFY(QFile::exists(wld));
  const QString backup = tool.backupFolder();
  QVERIFY2(!backup.isEmpty(), "the first write is preceded by a backup");
  QCOMPARE(QDir::cleanPath(QFileInfo(backup).absolutePath()),
           QDir::cleanPath(dir.filePath(QStringLiteral("정합백업"))));

  QString message;
  QVERIFY2(tool.restoreOriginals(&message), qPrintable(message));
  QVERIFY2(message.contains(QStringLiteral("되돌렸습니다")), qPrintable(message));
  QVERIFY(!QFile::exists(wld));
  QVERIFY(!QFile::exists(GeorefService::prjPathFor(png)));
  QCOMPARE(rl->id(), id);
  QVERIFY2(QgsProject::instance()->mapLayer(id) == rl, "restore keeps the layer in the project");
  QVERIFY2(GeorefService::looksUnreferencedRaster(rl), qPrintable(rl->extent().toString(2)));
  QCOMPARE(tool.pairCount(), 0);
  QVERIFY(QFileInfo::exists(backup));  // the backup stays on disk for the record
  tool.endSession();
  QgsProject::instance()->removeAllMapLayers();  // files close before the temp dir goes
}

void TestAlignTool::vectorSave_keepsAnOlderAlignedFile() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QgsVectorLayer mem(QStringLiteral("LineString?crs=EPSG:5186"), QStringLiteral("cad"),
                     QStringLiteral("memory"));
  QgsFeature f(mem.fields());
  f.setGeometry(QgsGeometry::fromPolylineXY({QgsPointXY(0, 0), QgsPointXY(10, 5)}));
  QVERIFY(mem.dataProvider()->addFeature(f));
  const QString gpkg = dir.filePath(QStringLiteral("cad.gpkg"));
  QgsVectorFileWriter::SaveVectorOptions opt;
  opt.driverName = QStringLiteral("GPKG");
  QString werr;
  QCOMPARE(QgsVectorFileWriter::writeAsVectorFormatV3(&mem, gpkg, QgsCoordinateTransformContext(),
                                                      opt, &werr),
           QgsVectorFileWriter::NoError);
  auto* cad = new QgsVectorLayer(gpkg, QStringLiteral("cad"), QStringLiteral("ogr"));
  QVERIFY(cad->isValid());
  QgsProject::instance()->addMapLayer(cad);

  const QString older = dir.filePath(QStringLiteral("cad_aligned.gpkg"));
  {
    QFile o(older);
    QVERIFY(o.open(QIODevice::WriteOnly));
    o.write("older result");
  }

  Canvas c;
  KaAlignMapTool tool(&c.canvas);
  QString err;
  QVERIFY2(tool.beginLayer(cad, workCrs(), &err), qPrintable(err));
  QVERIFY(tool.fitToDisplay());
  QString saved;
  QVERIFY2(tool.saveAligned(&saved, &err), qPrintable(err));
  QCOMPARE(QFileInfo(saved).fileName(), QStringLiteral("cad_aligned_2.gpkg"));
  QVERIFY(QFile::exists(saved));
  {
    QFile o(older);
    QVERIFY(o.open(QIODevice::ReadOnly));
    QCOMPARE(o.readAll(), QByteArray("older result"));
  }
  // Saving again in the same session rewrites its own copy instead of a third file.
  QString again;
  QVERIFY2(tool.saveAligned(&again, &err), qPrintable(err));
  QCOMPARE(again, saved);
  QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("cad_aligned_3.gpkg"))));
  tool.endSession();
  QgsProject::instance()->removeAllMapLayers();  // files close before the temp dir goes
}

// 도면 정합 저장은 변환본의 모든 표에 같은 변환을 쓰고 원본 옆 _aligned.gpkg 를 만들지 않는다.
void TestAlignTool::drawingSave_movesEveryTableAndWritesNoAlignedCopy() {
  QTemporaryDir dir;
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
  QVERIFY(!QgsProject::instance()->mapLayersByName(QStringLiteral("선 맞춤")).isEmpty());
  pickPair(tool, c, 0, 0, 200100, 450100);
  pickPair(tool, c, 100, 0, 200200, 450100);
  QString saved;
  QVERIFY2(tool.saveAligned(&saved, &err), qPrintable(err));
  QCOMPARE(QFileInfo(saved).absoluteFilePath(), QFileInfo(gpkg).absoluteFilePath());
  QVERIFY(QgsProject::instance()->mapLayersByName(QStringLiteral("선 맞춤")).isEmpty());
  QVERIFY(shown(lines) && shown(texts));
  QVERIFY(QDir(dir.path()).entryList({QStringLiteral("*_aligned*.gpkg")}, QDir::Files).isEmpty());
  const QgsPointXY start = firstPoint(gpkg, QStringLiteral("lines"));  // (10, 10) → (200110, 450110)
  QVERIFY2(start.distance(QgsPointXY(200110, 450110)) < 1, qPrintable(start.toString(2)));
  const QgsPointXY label = firstPoint(gpkg, QStringLiteral("texts"));  // (50, 50) → (200150, 450150)
  QVERIFY2(label.distance(QgsPointXY(200150, 450150)) < 1, qPrintable(label.toString(2)));
  tool.endSession();
  QgsProject::instance()->removeAllMapLayers();  // files close before the temp dir goes
}

// 저장하지 않고 끝내면 맞춤 복제본을 빼고 도면 레이어를 모두 다시 보이며, 변환본은 그대로다.
void TestAlignTool::drawingCancel_removesTheCloneAndShowsEverything() {
  QTemporaryDir dir;
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
  QVERIFY(QgsProject::instance()->mapLayersByName(QStringLiteral("선 맞춤")).isEmpty());
  QVERIFY(shown(lines) && shown(texts));
  QCOMPARE(firstPoint(gpkg, QStringLiteral("lines")), QgsPointXY(10, 10));
  QgsProject::instance()->removeAllMapLayers();
}

#include "test_align_tool.moc"

int main(int argc, char** argv) {
  QStandardPaths::setTestModeEnabled(true);
  QgsApplication app(argc, argv, false);
  const QString prefix = qEnvironmentVariable(
      "QGIS_PREFIX_PATH", QFile::exists(QStringLiteral("D:/OSGeo4W/apps/qgis-dev"))
                              ? QStringLiteral("D:/OSGeo4W/apps/qgis-dev")
                              : QStringLiteral("C:/OSGeo4W/apps/qgis-dev"));
  QgsApplication::setPrefixPath(prefix, true);
  QgsApplication::setPluginPath(prefix + QStringLiteral("/plugins"));
  QgsApplication::initQgis();
  TestAlignTool tc;
  const int rc = QTest::qExec(&tc, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
