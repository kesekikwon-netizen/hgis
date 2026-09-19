// 15만 합성 도형으로 필지 렌더·조사 열기·조판 진입 상한을 고정한다.
// 원본 현장 GPKG는 쓰지 않는다. 생성 시간은 예산에 넣지 않는다.
#include "core/LayerOps.h"
#include "core/LayoutService.h"
#include "core/SurveyProjectFactory.h"
#include "core/SurveyStorage.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QImage>
#include <QTemporaryDir>
#include <QtTest>

#include <qgsapplication.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransformcontext.h>
#include <qgsfeature.h>
#include <qgsfield.h>
#include <qgsfields.h>
#include <qgsgeometry.h>
#include <qgslayoutexporter.h>
#include <qgslayoutitemmap.h>
#include <qgslayoutmanager.h>
#include <qgsmaprenderersequentialjob.h>
#include <qgsmapsettings.h>
#include <qgsprintlayout.h>
#include <qgsproject.h>
#include <qgsrectangle.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectorlayer.h>

namespace {

constexpr int kFeatureCount = 150000;
constexpr int kGridCols = 400;
constexpr qint64 kParcelRenderBudgetMs = 8000;
constexpr qint64 kSurveyOpenBudgetMs = 1000;
constexpr qint64 kLayoutEnterBudgetMs = 10000;

QgsRectangle gridExtent() {
  const int rows = (kFeatureCount + kGridCols - 1) / kGridCols;
  return QgsRectangle(200000.0, 450000.0, 200000.0 + kGridCols * 8.0, 450000.0 + rows * 8.0);
}

bool writeGridGpkg(const QString& path, const QString& layerName, QString* errorOut) {
  QgsFields fields;
  fields.append(QgsField(QStringLiteral("kind"), QMetaType::Type::QString));
  QgsVectorFileWriter::SaveVectorOptions options;
  options.driverName = QStringLiteral("GPKG");
  options.layerName = layerName;
  options.fileEncoding = QStringLiteral("UTF-8");
  options.layerOptions << QStringLiteral("SPATIAL_INDEX=YES");
  if (QFileInfo::exists(path))
    options.actionOnExistingFile = QgsVectorFileWriter::CreateOrOverwriteLayer;
  const QgsCoordinateReferenceSystem crs(QStringLiteral("EPSG:5187"));
  QgsCoordinateTransformContext context;
  std::unique_ptr<QgsVectorFileWriter> writer(
      QgsVectorFileWriter::create(path, fields, Qgis::WkbType::Polygon, crs, context, options));
  if (!writer || writer->hasError() != QgsVectorFileWriter::NoError) {
    if (errorOut) *errorOut = writer ? writer->errorMessage() : QStringLiteral("writer");
    return false;
  }
  QgsFeature feature(fields);
  feature.setAttribute(0, QStringLiteral("perf"));
  for (int i = 0; i < kFeatureCount; ++i) {
    const int col = i % kGridCols;
    const int row = i / kGridCols;
    const double x = 200000.0 + col * 8.0;
    const double y = 450000.0 + row * 8.0;
    feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(x, y, x + 6.0, y + 6.0)));
    if (!writer->addFeature(feature)) {
      if (errorOut) *errorOut = QStringLiteral("addFeature %1").arg(i);
      return false;
    }
  }
  if (writer->hasError() != QgsVectorFileWriter::NoError || !writer->flushBuffer()) {
    if (errorOut) *errorOut = writer->errorMessage();
    return false;
  }
  return true;
}

void assertUnderBudget(const char* label, qint64 elapsedMs, qint64 budgetMs) {
  qInfo().noquote() << label << "elapsed_ms=" << elapsedMs << "budget_ms=" << budgetMs;
  QVERIFY2(elapsedMs <= budgetMs,
           qPrintable(QStringLiteral("%1 %2ms > %3ms").arg(QString::fromUtf8(label)).arg(elapsedMs).arg(budgetMs)));
}

}  // namespace

class TestPerf : public QObject {
  Q_OBJECT
private slots:
  void initTestCase();
  void parcelRender_150k_underBudget();
  void surveyOpen_150k_underBudget();
  void layoutEnter_underBudget();

private:
  QTemporaryDir m_dir;
  QString m_parcelGpkg;
  QString m_surveyGpkg;
};

void TestPerf::initTestCase() {
  QVERIFY2(m_dir.isValid(), "QTemporaryDir");
  QString err;
  m_parcelGpkg = m_dir.filePath(QStringLiteral("parcels.gpkg"));
  QVERIFY2(writeGridGpkg(m_parcelGpkg, QStringLiteral("parcels"), &err), qPrintable(err));

  m_surveyGpkg = SurveyProjectFactory::createNewSurvey(m_dir.path(), QStringLiteral("perf150k"), &err,
                                                       QStringLiteral("EPSG:5187"));
  QVERIFY2(!m_surveyGpkg.isEmpty(), qPrintable(err));
  QVERIFY2(writeGridGpkg(m_surveyGpkg, QStringLiteral("feature_poly"), &err), qPrintable(err));
}

void TestPerf::parcelRender_150k_underBudget() {
  QgsVectorLayer layer(m_parcelGpkg + QStringLiteral("|layername=parcels"), QStringLiteral("parcels"),
                       QStringLiteral("ogr"));
  QVERIFY(layer.isValid());
  QCOMPARE(int(layer.featureCount()), kFeatureCount);

  QgsMapSettings settings;
  settings.setLayers({&layer});
  settings.setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
  settings.setOutputSize(QSize(1200, 1200));
  settings.setOutputDpi(96.0);
  settings.setBackgroundColor(Qt::white);
  settings.setExtent(gridExtent());

  QElapsedTimer timer;
  timer.start();
  // 앱은 WMS 중첩 크래시를 피하려고 순차 렌더를 쓴다.
  // https://qgis.org/pyqgis/master/core/QgsMapRendererSequentialJob.html
  QgsMapRendererSequentialJob job(settings);
  job.start();
  job.waitForFinished();
  const qint64 ms = timer.elapsed();
  const QImage image = job.renderedImage();
  QVERIFY(!image.isNull());
  int dark = 0;
  for (int y = 0; y < image.height(); y += 8) {
    for (int x = 0; x < image.width(); x += 8) {
      if (qGray(image.pixel(x, y)) < 200) ++dark;
    }
  }
  QVERIFY2(dark > 50, "렌더 결과가 비어 있다");
  assertUnderBudget("parcel_render", ms, kParcelRenderBudgetMs);
}

void TestPerf::surveyOpen_150k_underBudget() {
  QString err;
  QVERIFY2(SurveyStorage::validateForOpen(m_surveyGpkg, &err), qPrintable(err));
  QgsProject project;
  project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));

  QElapsedTimer timer;
  timer.start();
  const int added = LayerOps::addNonEmptyDomainLayers(&project, m_surveyGpkg);
  const qint64 ms = timer.elapsed();
  QVERIFY2(added >= 1, "조사 도메인 레이어가 안 올라왔다");
  auto* features = LayerOps::findByLayerKey(&project, QStringLiteral("feature_poly"));
  QVERIFY(features);
  QCOMPARE(int(features->featureCount()), kFeatureCount);
  assertUnderBudget("survey_open", ms, kSurveyOpenBudgetMs);
}

void TestPerf::layoutEnter_underBudget() {
  QgsProject project;
  project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
  auto* layer = new QgsVectorLayer(m_parcelGpkg + QStringLiteral("|layername=parcels"),
                                   QStringLiteral("parcels"), QStringLiteral("ogr"));
  QVERIFY(layer->isValid());
  project.addMapLayer(layer);

  QElapsedTimer timer;
  timer.start();
  QString err;
  QVERIFY2(!LayoutService::createBlankSheet(&project, 297.0, 210.0, QStringLiteral("user_sheet"), &err)
                .isEmpty(),
           qPrintable(err));
  auto* layout = dynamic_cast<QgsPrintLayout*>(
      project.layoutManager()->layoutByName(QStringLiteral("user_sheet")));
  QVERIFY(layout);
  auto* map = new QgsLayoutItemMap(layout);
  map->setId(QStringLiteral("ka_map"));
  map->attemptSetSceneRect(QRectF(20.0, 20.0, 120.0, 80.0));
  map->setCrs(project.crs());
  map->setKeepLayerSet(true);
  map->setLayers(QList<QgsMapLayer*>{layer});
  map->zoomToExtent(gridExtent());
  if (map->scene() != layout)
    layout->addLayoutItem(map);
  LayoutService::applySingleRasterPassRendering(layout);
  // https://qgis.org/pyqgis/master/core/QgsLayoutExporter.html
  QgsLayoutExporter exporter(layout);
  const QImage page = exporter.renderPageToImage(0, QSize(1188, 840), 96.0);
  const qint64 ms = timer.elapsed();
  QVERIFY2(!page.isNull(), "조판 페이지가 비었다");
  QVERIFY(layout->itemById(QStringLiteral("ka_map")));
  assertUnderBudget("layout_enter", ms, kLayoutEnterBudgetMs);
}

#include "test_perf.moc"

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  const QString prefix = qEnvironmentVariable("QGIS_PREFIX_PATH");
  if (!prefix.isEmpty()) {
    QgsApplication::setPrefixPath(prefix, true);
    QgsApplication::setPluginPath(prefix + QStringLiteral("/plugins"));
  }
  QgsApplication::initQgis();
  TestPerf test;
  const int rc = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
