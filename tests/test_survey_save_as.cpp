// 「다른 이름으로 저장」이 원본 조사 파일을 바꾸지 않는다: 원본의 마지막 저장본을 새 파일로 복사하고 레이어를
// 새 파일로 옮긴 뒤에야 아직 저장하지 않은 편집을 쓴다(SurveySaveAs::moveToCopy, MainWindow::saveProjectAs).
// 예전에는 편집을 원본에 먼저 쓰고 복사해, 원본에 저장하지 않은 그림이 들어갔다(2026-10-02 조사 R19).
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "core/SurveySaveAs.h"

#include <qgsapplication.h>
#include <qgscoordinatetransformcontext.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgsproject.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectorlayer.h>

namespace {

// 조사구역 하나(「저장한 구역」)가 든 GPKG.
QString writeSurvey(const QString& path) {
  QgsVectorLayer mem(QStringLiteral("Polygon?crs=EPSG:5187&field=name:string"), QStringLiteral("t"),
                     QStringLiteral("memory"));
  QgsFeature f(mem.fields());
  f.setGeometry(QgsGeometry::fromRect(QgsRectangle(0, 0, 10, 10)));
  f.setAttributes(QgsAttributes(QVector<QVariant>{QStringLiteral("저장한 구역")}));
  mem.dataProvider()->addFeature(f);
  QgsVectorFileWriter::SaveVectorOptions options;
  options.driverName = QStringLiteral("GPKG");
  options.layerName = QStringLiteral("survey_area");
  return QgsVectorFileWriter::writeAsVectorFormatV3(&mem, path, QgsCoordinateTransformContext(), options) ==
                 QgsVectorFileWriter::NoError
             ? path
             : QString();
}

QStringList names(const QString& gpkg) {
  QgsVectorLayer layer(gpkg + QStringLiteral("|layername=survey_area"), QStringLiteral("x"), QStringLiteral("ogr"));
  QStringList out;
  QgsFeature f;
  for (QgsFeatureIterator it = layer.getFeatures(); it.nextFeature(f);) out << f.attribute(QStringLiteral("name")).toString();
  out.sort();
  return out;
}

}  // namespace

class TestSurveySaveAs : public QObject {
  Q_OBJECT
 private slots:
  void moveToCopy_savesEditsIntoTheCopyOnly() {
    QTemporaryDir tmp;
    const QString original = writeSurvey(tmp.filePath(QStringLiteral("조사.gpkg")));
    QVERIFY(!original.isEmpty());
    QgsProject project;
    auto* layer = new QgsVectorLayer(original + QStringLiteral("|layername=survey_area"), QStringLiteral("조사구역"),
                                     QStringLiteral("ogr"));
    QVERIFY(layer->isValid());
    project.addMapLayer(layer);
    QVERIFY(layer->startEditing());
    QgsFeature drawn(layer->fields());
    drawn.setGeometry(QgsGeometry::fromRect(QgsRectangle(20, 20, 30, 30)));
    drawn.setAttribute(QStringLiteral("name"), QStringLiteral("저장 안 한 구역"));
    QVERIFY(layer->addFeature(drawn));

    const QString copy = tmp.filePath(QStringLiteral("조사_복사본.gpkg"));
    QString error;
    QVERIFY2(SurveySaveAs::moveToCopy(&project, original, copy, &error), qPrintable(error));
    QVERIFY(layer->source().startsWith(copy));
    QVERIFY(layer->isModified());  // 편집은 아직 레이어에 있다: 다음 저장에 새 파일로만 간다
    QVERIFY(layer->commitChanges());
    QCOMPARE(names(original), QStringList({QStringLiteral("저장한 구역")}));
    QCOMPARE(names(copy), QStringList({QStringLiteral("저장 안 한 구역"), QStringLiteral("저장한 구역")}));
    project.removeAllMapLayers();
  }

  void moveToCopy_leavesOtherFilesAlone() {
    QTemporaryDir tmp;
    const QString original = writeSurvey(tmp.filePath(QStringLiteral("조사.gpkg")));
    const QString other = writeSurvey(tmp.filePath(QStringLiteral("참고.gpkg")));
    QgsProject project;
    auto* reference = new QgsVectorLayer(other + QStringLiteral("|layername=survey_area"), QStringLiteral("참고"),
                                         QStringLiteral("ogr"));
    project.addMapLayer(reference);
    QString error;
    QVERIFY2(SurveySaveAs::moveToCopy(&project, original, tmp.filePath(QStringLiteral("새.gpkg")), &error),
             qPrintable(error));
    QVERIFY(reference->source().startsWith(other));
    project.removeAllMapLayers();
  }
};

int main(int argc, char** argv) {
  QStandardPaths::setTestModeEnabled(true);
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  int result = 0;
  {
    TestSurveySaveAs tests;
    result = QTest::qExec(&tests, argc, argv);
  }
  QgsApplication::exitQgis();
  return result;
}

#include "test_survey_save_as.moc"
