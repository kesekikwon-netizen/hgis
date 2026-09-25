// 15만 합성 도형으로 필지 렌더·조사 열기·조판 진입 상한을 고정한다.
// P3-2: 시작→홈 화면, A3 PDF 내보내기 예산을 추가한다.
// 원본 현장 GPKG는 쓰지 않는다. 생성 시간은 예산에 넣지 않는다.
#include "app/MainWindow.h"
#include "core/LayerOps.h"
#include "core/LayoutService.h"
#include "core/SurveyProjectFactory.h"
#include "core/SurveyStorage.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QImage>
#include <QSettings>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QWidget>
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

#include <algorithm>

namespace {

constexpr int kFeatureCount = 150000;
constexpr int kGridCols = 400;
constexpr qint64 kParcelRenderBudgetMs = 8000;
constexpr qint64 kSurveyOpenBudgetMs = 1000;
constexpr qint64 kLayoutEnterBudgetMs = 10000;
// P3-2: 기준 PC 5회 중앙값 × 1.3 (startup_home median 54→70, a3_pdf median 21→27).
constexpr qint64 kStartupHomeBudgetMs = 70;
constexpr qint64 kA3PdfExportBudgetMs = 27;

QgsRectangle gridExtent() {
  const int rows = (kFeatureCount + kGridCols - 1) / kGridCols;
  return QgsRectangle(200000.0, 450000.0, 200000.0 + kGridCols * 8.0, 450000.0 + rows * 8.0);
}

bool writeGridGpkg(const QString& path, const QString& layerName, int featureCount, QString* errorOut) {
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
  for (int i = 0; i < featureCount; ++i) {
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

qint64 medianOf(QList<qint64> samples) {
  std::sort(samples.begin(), samples.end());
  const int n = samples.size();
  if (n == 0) return 0;
  if (n % 2 == 1) return samples[n / 2];
  return (samples[n / 2 - 1] + samples[n / 2]) / 2;
}

}  // namespace

class TestPerf : public QObject {
  Q_OBJECT
private slots:
  void initTestCase();
  void parcelRender_150k_underBudget();
  void surveyOpen_150k_underBudget();
  void layoutEnter_underBudget();
  void startupHome_underBudget();
  void a3PdfExport_underBudget();

private:
  void ensureParcelGrid();
  void ensureSurveyGpkg();

  QTemporaryDir m_dir;
  QString m_parcelGpkg;
  QString m_surveyGpkg;
  bool m_parcelReady = false;
  bool m_surveyReady = false;
};

void TestPerf::initTestCase() {
  QVERIFY2(m_dir.isValid(), "QTemporaryDir");
}

void TestPerf::ensureParcelGrid() {
  if (m_parcelReady) return;
  QString err;
  m_parcelGpkg = m_dir.filePath(QStringLiteral("parcels.gpkg"));
  QVERIFY2(writeGridGpkg(m_parcelGpkg, QStringLiteral("parcels"), kFeatureCount, &err), qPrintable(err));
  m_parcelReady = true;
}

void TestPerf::ensureSurveyGpkg() {
  if (m_surveyReady) return;
  QString err;
  m_surveyGpkg = SurveyProjectFactory::createNewSurvey(m_dir.path(), QStringLiteral("perf150k"), &err,
                                                       QStringLiteral("EPSG:5187"));
  QVERIFY2(!m_surveyGpkg.isEmpty(), qPrintable(err));
  QVERIFY2(writeGridGpkg(m_surveyGpkg, QStringLiteral("feature_poly"), kFeatureCount, &err), qPrintable(err));
  m_surveyReady = true;
}

void TestPerf::parcelRender_150k_underBudget() {
  ensureParcelGrid();
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
  ensureSurveyGpkg();
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
  ensureParcelGrid();
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

void TestPerf::startupHome_underBudget() {
  // 제품: 시작 시 홈만 열고 조사·배경지도를 자동으로 올리지 않는다.
  // 5회 재어 중앙값×1.3을 예산으로 쓴다(측정은 한 프로세스에서 5회).
  QList<qint64> samples;
  samples.reserve(5);
  for (int i = 0; i < 5; ++i) {
    QElapsedTimer timer;
    timer.start();
    MainWindow window;
    window.show();
    auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("viewTabs"));
    QVERIFY2(tabs, "viewTabs");
    QTRY_COMPARE_WITH_TIMEOUT(tabs->tabText(tabs->currentIndex()), QStringLiteral("홈"), 10000);
    auto* startPage = window.findChild<QWidget*>(QStringLiteral("startPage"));
    QVERIFY2(startPage, "startPage");
    QVERIFY(startPage->isVisible());
    const qint64 ms = timer.elapsed();
    samples.append(ms);
    QCOMPARE(QgsProject::instance()->mapLayers().size(), 0);
    window.close();
    QCoreApplication::processEvents();
    qInfo().noquote() << "startup_home sample" << (i + 1) << "elapsed_ms=" << ms;
  }
  const qint64 med = medianOf(samples);
  const qint64 budget = kStartupHomeBudgetMs;
  qInfo().noquote() << "startup_home median_ms=" << med << "median_x1_3=" << qRound(med * 1.3)
                     << "budget_ms=" << budget;
  assertUnderBudget("startup_home", med, budget);
}

void TestPerf::a3PdfExport_underBudget() {
  // A3 가로(420×297) user_sheet PDF. 내보내기만 재고, 용지·레이어 준비는 예산 밖.
  QString err;
  const QString tinyGpkg = m_dir.filePath(QStringLiteral("a3_pdf_tiny.gpkg"));
  QVERIFY2(writeGridGpkg(tinyGpkg, QStringLiteral("feature_poly"), 40, &err), qPrintable(err));

  QgsProject project;
  project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
  auto* layer = new QgsVectorLayer(tinyGpkg + QStringLiteral("|layername=feature_poly"),
                                   QStringLiteral("feature_poly"), QStringLiteral("ogr"));
  QVERIFY(layer->isValid());
  LayerOps::markSurveyLayer(layer, QStringLiteral("feature_poly"));
  project.addMapLayer(layer);

  QVERIFY2(!LayoutService::createBlankSheet(&project, 420.0, 297.0, QStringLiteral("user_sheet"), &err)
                .isEmpty(),
           qPrintable(err));
  auto* layout = dynamic_cast<QgsPrintLayout*>(
      project.layoutManager()->layoutByName(QStringLiteral("user_sheet")));
  QVERIFY(layout);
  auto* map = new QgsLayoutItemMap(layout);
  map->setId(QStringLiteral("ka_map"));
  map->attemptSetSceneRect(QRectF(20.0, 20.0, 380.0, 250.0));
  map->setCrs(project.crs());
  map->setKeepLayerSet(true);
  map->setLayers(QList<QgsMapLayer*>{layer});
  map->zoomToExtent(layer->extent());
  if (map->scene() != layout)
    layout->addLayoutItem(map);
  LayoutService::markStudioSheetComposed(layout);
  LayoutService::applySingleRasterPassRendering(layout);

  QList<qint64> samples;
  samples.reserve(5);
  for (int i = 0; i < 5; ++i) {
    const QString pdf = m_dir.filePath(QStringLiteral("a3_perf_%1.pdf").arg(i));
    QFile::remove(pdf);
    QElapsedTimer timer;
    timer.start();
    const QString saved = LayoutService::exportLayoutPdf(&project, QStringLiteral("user_sheet"), pdf, &err);
    const qint64 ms = timer.elapsed();
    QVERIFY2(!saved.isEmpty(), qPrintable(err));
    QVERIFY2(QFileInfo(pdf).size() > 500, "A3 PDF가 비었다");
    samples.append(ms);
    qInfo().noquote() << "a3_pdf_export sample" << (i + 1) << "elapsed_ms=" << ms;
  }
  const qint64 med = medianOf(samples);
  const qint64 budget = kA3PdfExportBudgetMs;
  qInfo().noquote() << "a3_pdf_export median_ms=" << med << "median_x1_3=" << qRound(med * 1.3)
                     << "budget_ms=" << budget;
  assertUnderBudget("a3_pdf_export", med, budget);
}

#include "test_perf.moc"

int main(int argc, char** argv) {
  if (qEnvironmentVariableIsEmpty("KA_HGIS_LOG_DIR"))
    qputenv("KA_HGIS_LOG_DIR", QDir::temp().filePath(QStringLiteral("ka-hgis-test-logs")).toUtf8());
  QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
  QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
  QgsApplication app(argc, argv, true);
#ifdef Q_OS_WIN
  if (QGuiApplication::platformName() == QLatin1String("offscreen")) {
    const QDir windows(qEnvironmentVariable("WINDIR"));
    for (const QString& file : {QStringLiteral("malgun.ttf"), QStringLiteral("malgunbd.ttf")}) {
      const QString path = windows.filePath(QStringLiteral("Fonts/") + file);
      if (QFontDatabase::addApplicationFont(path) < 0) {
        qCritical().noquote() << "Could not load the installed QA font:" << path;
        return 1;
      }
    }
  }
#endif
  QTemporaryDir settings;
  if (!settings.isValid())
    return 1;
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
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
