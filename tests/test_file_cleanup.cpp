// 지우려는 파일을 앱 안의 읽는 쪽(지도가 그리는 동안 연 GPKG 손잡이)이 아직 열고 있으면 지금은 지우지 못한다.
// FileCleanup::removeWhenFree 는 그때 잠시 뒤 다시 지운다: 같은 도면을 다시 불러오면 옛 변환본이 남던 일(R89).
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "core/FileCleanup.h"

#include <qgsapplication.h>
#include <qgscoordinatetransformcontext.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectorlayer.h>

namespace {

QString writeGpkg(const QString& path) {
  QgsVectorLayer mem(QStringLiteral("LineString?crs=EPSG:5186"), QStringLiteral("t"), QStringLiteral("memory"));
  QgsFeature f(mem.fields());
  f.setGeometry(QgsGeometry::fromPolylineXY({QgsPointXY(0, 0), QgsPointXY(10, 0)}));
  mem.dataProvider()->addFeature(f);
  QgsVectorFileWriter::SaveVectorOptions options;
  options.driverName = QStringLiteral("GPKG");
  options.layerName = QStringLiteral("lines");
  return QgsVectorFileWriter::writeAsVectorFormatV3(&mem, path, QgsCoordinateTransformContext(), options) ==
                 QgsVectorFileWriter::NoError
             ? path
             : QString();
}

}  // namespace

class TestFileCleanup : public QObject {
  Q_OBJECT
 private slots:
  void removeWhenFree_removesAtOnceWhenNobodyHoldsIt() {
    QTemporaryDir tmp;
    const QString gpkg = writeGpkg(tmp.filePath(QStringLiteral("옛 변환본.gpkg")));
    QVERIFY(!gpkg.isEmpty());
    QVERIFY(FileCleanup::removeWhenFree(gpkg));
    QVERIFY(!QFileInfo::exists(gpkg));
  }

  void removeWhenFree_removesLaterOnceTheReaderCloses() {
    QTemporaryDir tmp;
    const QString gpkg = writeGpkg(tmp.filePath(QStringLiteral("옛 변환본.gpkg")));
    auto reader = std::make_unique<QgsVectorLayer>(gpkg + QStringLiteral("|layername=lines"), QStringLiteral("읽는 쪽"),
                                                   QStringLiteral("ogr"));
    QgsFeature f;
    QgsFeatureIterator it = reader->getFeatures();
    QVERIFY(it.nextFeature(f));  // 지도가 그리는 중처럼 읽는 손잡이가 열려 있다
    QVERIFY(!FileCleanup::removeWhenFree(gpkg));
    QVERIFY(QFileInfo::exists(gpkg));
    it.close();
    reader.reset();
    QTRY_VERIFY_WITH_TIMEOUT(!QFileInfo::exists(gpkg), 15000);
  }
};

int main(int argc, char** argv) {
  QStandardPaths::setTestModeEnabled(true);
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  int result = 0;
  {
    TestFileCleanup tests;
    result = QTest::qExec(&tests, argc, argv);
  }
  QgsApplication::exitQgis();
  return result;
}

#include "test_file_cleanup.moc"
