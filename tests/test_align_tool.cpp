#include <QtTest>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "app/KaAlignMapTool.h"
#include "core/GeorefService.h"

#include <qgsapplication.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransformcontext.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
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
  void vectorRestartOnTheClone_keepsOneClone();
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
// 선 하나짜리 GPKG(도면 변환본이 아닌 일반 벡터)를 프로젝트에 올린다.
QgsVectorLayer* addLineGpkg(const QTemporaryDir& dir) {
  QgsVectorLayer mem(QStringLiteral("LineString?crs=EPSG:5186"), QStringLiteral("cad"),
                     QStringLiteral("memory"));
  QgsFeature f(mem.fields());
  f.setGeometry(QgsGeometry::fromPolylineXY({QgsPointXY(0, 0), QgsPointXY(10, 5)}));
  mem.dataProvider()->addFeature(f);
  const QString gpkg = dir.filePath(QStringLiteral("cad.gpkg"));
  QgsVectorFileWriter::SaveVectorOptions opt;
  opt.driverName = QStringLiteral("GPKG");
  QString werr;
  if (QgsVectorFileWriter::writeAsVectorFormatV3(&mem, gpkg, QgsCoordinateTransformContext(), opt, &werr) !=
      QgsVectorFileWriter::NoError)
    return nullptr;
  auto* cad = new QgsVectorLayer(gpkg, QStringLiteral("cad"), QStringLiteral("ogr"));
  QgsProject::instance()->addMapLayer(cad);
  return cad->isValid() ? cad : nullptr;
}

void pickPair(KaAlignMapTool& tool, Canvas& c, double sx, double sy, double mx, double my) {
  tool.setSourcePoint(sx, sy);
  QgsMapMouseEvent press(&c.canvas, QEvent::MouseButtonPress, c.px(mx, my), Qt::LeftButton,
                         Qt::LeftButton);
  tool.canvasPressEvent(&press);
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
  QgsVectorLayer* cad = addLineGpkg(dir);
  QVERIFY(cad);

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

#include "test_align_tool.moc"

// 도면이 아닌 벡터를 맞추는 중에 「맞추기」를 다시 누르면(지금 레이어 = 맞춤 복제본) 그 복제본을 계속 맞춘다.
// 원본으로 되돌려 복제본을 하나 더 만들지 않는다.
void TestAlignTool::vectorRestartOnTheClone_keepsOneClone() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QgsVectorLayer* cad = addLineGpkg(dir);
  QVERIFY(cad);
  Canvas c;
  KaAlignMapTool tool(&c.canvas);
  QString err;
  QVERIFY2(tool.beginLayer(cad, workCrs(), &err), qPrintable(err));
  QgsMapLayer* clone = tool.targetLayer();
  QVERIFY(clone && clone != cad);
  const QString cloneName = clone->name();
  QVERIFY2(tool.beginLayer(clone, workCrs(), &err), qPrintable(err));
  QCOMPARE(tool.targetLayer(), clone);
  QCOMPARE(QgsProject::instance()->mapLayersByName(cloneName).size(), 1);
  tool.endSession();
  QgsProject::instance()->removeAllMapLayers();  // files close before the temp dir goes
}

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
