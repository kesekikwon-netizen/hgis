#include "app/KaCanvasGridOverlay.h"
#include "app/KaDemClassDialog.h"
#include "app/KaGridOriginButton.h"
#include "app/KaSurveyAreaDialog.h"
#include "app/KaSurveyContourDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QToolButton>
#include <QtTest>

#include <cmath>
#include <memory>
#include <vector>

#include <cpl_conv.h>
#include <gdal_priv.h>
#include <ogr_spatialref.h>
#include <qgsapplication.h>
#include <qgsgeometry.h>
#include <qgsmapcanvas.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorlayer.h>

namespace {

double luminance(const QColor& color) {
  auto channel = [](double v) { return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
  return 0.2126 * channel(color.redF()) + 0.7152 * channel(color.greenF()) + 0.0722 * channel(color.blueF());
}

double contrast(const QColor& a, const QColor& b) {
  const double la = luminance(a) + 0.05, lb = luminance(b) + 0.05;
  return std::max(la, lb) / std::min(la, lb);
}

QString writeSite(const QTemporaryDir& dir) {
  const QString path = dir.filePath(QStringLiteral("site.csv"));
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) return {};
  file.write("점번호,X,Y,표고\nP1,550000,200000,12.0\nP2,550010,200000,0\nP3,550000,200020,12.6\nP4,550030,200040,13.1\n");
  return path;
}

}  // namespace

class TestSurveyPointDialogs : public QObject {
  Q_OBJECT
private slots:
  void contourDialogShowsDiagnosticsAndHandsOverPoints();
  void fileCrsChoiceMovesThePoints();
  void surveyAreaSwatchesAreReadable();
  void surveyAreaLayerIsFoundById();
  void demDialogOffersTheViewFit();
  void gridOriginButtonMovesTheGrid();
};

void TestSurveyPointDialogs::contourDialogShowsDiagnosticsAndHandsOverPoints() {
  QTemporaryDir dir;
  const QString path = writeSite(dir);
  QVERIFY(!path.isEmpty());
  KaSurveyContourDialog dialog(QStringLiteral("EPSG:5186"));
  dialog.loadFile(path);
  auto* swap = dialog.findChild<QCheckBox*>(QStringLiteral("contourSwapAxes"));
  auto* exclude = dialog.findChild<QCheckBox*>(QStringLiteral("contourExcludeSuspicious"));
  auto* preview = dialog.findChild<QLabel*>(QStringLiteral("contourPreview"));
  QVERIFY(swap && exclude && preview);
  QVERIFY(swap->isChecked());
  QVERIFY(exclude->isEnabled() && !exclude->isChecked());
  QVERIFY2(preview->text().contains(QStringLiteral("의심점 1개")), qPrintable(preview->text()));
  SurveyContourJob job = dialog.job();
  QCOMPARE(job.points.size(), 4);
  QVERIFY(!job.excludeSuspicious);
  for (const SurveyPoint& point : job.points) QVERIFY(point.x < 300000.0 && point.y > 500000.0);
  exclude->setChecked(true);
  QVERIFY(dialog.job().excludeSuspicious);
  QVERIFY2(preview->text().startsWith(QStringLiteral("점 3개")), qPrintable(preview->text()));
  swap->setChecked(false);
  job = dialog.job();
  QVERIFY(!job.read.swapAxes);
  QVERIFY(job.points.first().x > 500000.0);
  swap->setChecked(true);
  dialog.setReferenceExtent(QgsRectangle(400000, 550000, 400100, 550100));
  QVERIFY2(preview->text().contains(QStringLiteral("떨어져")), qPrintable(preview->text()));
}

void TestSurveyPointDialogs::fileCrsChoiceMovesThePoints() {
  QTemporaryDir dir;
  const QString path = writeSite(dir);
  KaSurveyContourDialog dialog(QStringLiteral("EPSG:5186"));
  dialog.loadFile(path);
  auto* more = dialog.findChild<QToolButton*>(QStringLiteral("contourAdvancedToggle"));
  auto* advanced = dialog.findChild<QWidget*>(QStringLiteral("contourAdvanced"));
  auto* crs = dialog.findChild<QComboBox*>(QStringLiteral("contourFileCrs"));
  QVERIFY(more && advanced && crs);
  QVERIFY(advanced->isHidden());
  more->setChecked(true);
  QVERIFY(!advanced->isHidden());
  QVERIFY(dialog.job().read.sourceCrsAuthId.isEmpty());
  const double before = dialog.job().points.first().x;
  const int east = crs->findData(QStringLiteral("EPSG:5187"));
  QVERIFY(east > 0);
  crs->setCurrentIndex(east);
  const SurveyContourJob job = dialog.job();
  QCOMPARE(job.read.sourceCrsAuthId, QStringLiteral("EPSG:5187"));
  QVERIFY2(std::abs(job.points.first().x - before) > 100000.0, qPrintable(QString::number(job.points.first().x)));
  QCOMPARE(crs->findData(QStringLiteral("EPSG:5186")), -1);  // the work CRS is the first entry
}

void TestSurveyPointDialogs::surveyAreaSwatchesAreReadable() {
  KaSurveyAreaDialog dialog(nullptr, nullptr, QString());
  const auto swatches = dialog.findChildren<QPushButton*>(QStringLiteral("surveyAreaColor"));
  QCOMPARE(swatches.size(), 8);
  const QRegularExpression background(QStringLiteral("background-color:\\s*(#[0-9a-fA-F]{6})"));
  const QRegularExpression text(QStringLiteral(";\\s*color:\\s*(#[0-9a-fA-F]{6})"));
  for (QPushButton* swatch : swatches) {
    const auto fill = background.match(swatch->styleSheet());
    const auto ink = text.match(swatch->styleSheet());
    QVERIFY2(fill.hasMatch() && ink.hasMatch(), qPrintable(swatch->styleSheet()));
    const double ratio = contrast(QColor(fill.captured(1)), QColor(ink.captured(1)));
    QVERIFY2(ratio >= 4.5, qPrintable(swatch->text() + QStringLiteral(" %1").arg(ratio)));
  }
  auto* start = dialog.findChild<QPushButton*>(QStringLiteral("surveyAreaStart"));
  QVERIFY(start && !start->styleSheet().contains(QStringLiteral("#0284C7"), Qt::CaseInsensitive));
}

void TestSurveyPointDialogs::surveyAreaLayerIsFoundById() {
  QgsProject project;
  auto* layer = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"), QStringLiteral("조사구역"),
                                   QStringLiteral("memory"));
  QVERIFY(layer->isValid());
  layer->setCustomProperty(QStringLiteral("ka_hgis/layer_key"), QStringLiteral("survey_area"));
  QgsFeature feature(layer->fields());
  feature.setGeometry(QgsGeometry::fromWkt(QStringLiteral("POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))")));
  QVERIFY(layer->dataProvider()->addFeature(feature));
  QVERIFY(project.addMapLayer(layer));
  KaSurveyAreaDialog dialog(nullptr, &project, QString());
  QVERIFY(!dialog.isNewLayer());
  QCOMPARE(dialog.selectedExistingLayer(), layer);
  auto* combo = dialog.findChild<QComboBox*>(QStringLiteral("surveyAreaExisting"));
  QVERIFY(combo);
  QVERIFY2(combo->currentText().contains(QStringLiteral("100 ㎡")), qPrintable(combo->currentText()));
  project.removeMapLayer(layer->id());
  QCOMPARE(dialog.selectedExistingLayer(), static_cast<QgsVectorLayer*>(nullptr));
}

void TestSurveyPointDialogs::demDialogOffersTheViewFit() {
  QTemporaryDir dir;
  const QString path = dir.filePath(QStringLiteral("flat.tif"));
  {
    GDALAllRegister();
    std::unique_ptr<GDALDataset, decltype(&GDALClose)> dataset(
        GetGDALDriverManager()->GetDriverByName("GTiff")->Create(path.toUtf8().constData(), 64, 32, 1, GDT_Float32, nullptr),
        GDALClose);
    QVERIFY(dataset);
    double transform[] = {200000.0, 1.0, 0.0, 500000.0, 0.0, -1.0};
    OGRSpatialReference srs;
    QVERIFY(srs.importFromEPSG(5186) == OGRERR_NONE);
    dataset->SetSpatialRef(&srs);
    dataset->SetGeoTransform(transform);
    std::vector<float> z(64 * 32);
    for (int i = 0; i < 64 * 32; ++i) z[static_cast<size_t>(i)] = 20.0f + 0.1f * static_cast<float>(i % 64);
    QVERIFY(dataset->GetRasterBand(1)->RasterIO(GF_Write, 0, 0, 64, 32, z.data(), 64, 32, GDT_Float32, 0, 0) == CE_None);
  }
  auto* project = QgsProject::instance();
  const auto cleanup = qScopeGuard([project] { project->clear(); });
  project->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  auto* dem = new QgsRasterLayer(path, QStringLiteral("DEM"), QStringLiteral("gdal"));
  QVERIFY(dem->isValid());
  project->addMapLayer(dem);
  QgsMapCanvas canvas;
  canvas.setRenderFlag(false);
  canvas.setDestinationCrs(project->crs());
  canvas.resize(320, 200);
  canvas.setExtent(dem->extent());
  KaDemClassDialog dialog(dem, nullptr, &canvas);
  auto* status = dialog.findChild<QLabel*>(QStringLiteral("demStatus"));
  auto* fit = dialog.findChild<QPushButton*>(QStringLiteral("demFitView"));
  QVERIFY(status && fit && fit->isEnabled());
  QVERIFY2(status->text().contains(QStringLiteral("현재 화면에 색 맞추기")), qPrintable(status->text()));
  fit->click();
  QCOMPARE(dem->customProperty(QStringLiteral("ka_hgis/dem_preset")).toString(), QStringLiteral("viewport"));
}

void TestSurveyPointDialogs::gridOriginButtonMovesTheGrid() {
  QgsMapCanvas canvas;
  auto* grid = new KaCanvasGridOverlay(&canvas);
  KaGridOriginButton button(&canvas, [grid] { return grid; });
  button.setOrigin(0.3, 0.2);
  QCOMPARE(grid->config().originX, 0.3);
  QCOMPARE(grid->config().originY, 0.2);
  KaCanvasGridOverlay::Config config = grid->config();
  config.stepMeters = 0.5;
  grid->setConfig(config);
  const QgsPointXY snapped = grid->snapToGrid(QgsPointXY(1.1, 0.9));
  QVERIFY2(std::abs(snapped.x() - 1.3) < 1e-9 && std::abs(snapped.y() - 0.7) < 1e-9,
           qPrintable(QStringLiteral("%1 %2").arg(snapped.x()).arg(snapped.y())));
  QVERIFY(button.toolTip().contains(QStringLiteral("0.300")));
}

int main(int argc, char** argv) {
  CPLSetConfigOption("GDAL_PAM_ENABLED", "NO");
  QgsApplication app(argc, argv, true);
#ifdef Q_OS_WIN
  if (QGuiApplication::platformName() == QLatin1String("offscreen")) {
    const QDir windows(qEnvironmentVariable("WINDIR", QStringLiteral("C:/Windows")));
    for (const QString& file : {QStringLiteral("malgun.ttf"), QStringLiteral("malgunbd.ttf")})
      QFontDatabase::addApplicationFont(windows.filePath(QStringLiteral("Fonts/") + file));
  }
#endif
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  int code = 0;
  {
    TestSurveyPointDialogs test;
    code = QTest::qExec(&test, argc, argv);
  }
  QgsApplication::exitQgis();
  return code;
}

#include "test_survey_point_dialogs.moc"
