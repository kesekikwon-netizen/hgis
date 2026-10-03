#include "app/KaCanvasGridOverlay.h"
#include "app/KaDemClassDialog.h"
#include "app/KaGridOriginButton.h"
#include "app/KaSurveyAreaDialog.h"
#include "app/KaTheme.h"
#include "core/LayerOps.h"
#include "app/KaSurveyContourDialog.h"

#include <QApplication>
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
#include <qgsmapcanvas.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>
#include <qgsvectorfilewriter.h>
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
  void surveyAreaDialogIsCompact();
  void surveyAreaStartIsTheThemedMainButton();
  void surveyAreaToContinueIsTheSurveysOwn();
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
  KaSurveyAreaDialog dialog(nullptr);
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

// The window for the first survey area stays small: name, one row of colours, one row of widths and the two
// buttons (user 2026-10-03 「창이 너무 크게 나타나는것 같지않나?」, docs/intent/2026-10-03-draw-dialog-and-inspector-tabs.md).
void TestSurveyPointDialogs::surveyAreaDialogIsCompact() {
  KaSurveyAreaDialog dialog(nullptr);
  QCOMPARE(dialog.layerName(), QStringLiteral("조사구역"));  // the first area, named without a number
  dialog.setStyleSheet(KaTheme::applicationStyleSheet());  // as in the app: every button is taller there
  const QSize size = dialog.sizeHint();
  QVERIFY2(size.height() < 320 && size.width() < 640, qPrintable(QStringLiteral("%1x%2").arg(size.width()).arg(size.height())));
  dialog.setAttribute(Qt::WA_DontShowOnScreen);
  dialog.show();
  QApplication::processEvents();
  const auto swatches = dialog.findChildren<QPushButton*>(QStringLiteral("surveyAreaColor"));
  QCOMPARE(swatches.size(), 8);
  for (QPushButton* swatch : swatches) QCOMPARE(swatch->y(), swatches.first()->y());  // one row
  const QString out = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
  if (!out.isEmpty()) QVERIFY(dialog.grab().save(QDir(out).filePath(QStringLiteral("survey-area-dialog.png"))));
}

// The start button is the window's main button like every other window's: no colours of its own (the
// theme's main-button rule paints it, so it follows 새 모양) and it stands before 「취소」.
void TestSurveyPointDialogs::surveyAreaStartIsTheThemedMainButton() {
  KaSurveyAreaDialog dialog(nullptr);
  auto* start = dialog.findChild<QPushButton*>(QStringLiteral("surveyAreaStart"));
  QVERIFY(start && start->isDefault());
  QVERIFY2(start->styleSheet().isEmpty(), qPrintable(start->styleSheet()));
  QPushButton* cancel = nullptr;
  for (QPushButton* button : dialog.findChildren<QPushButton*>())
    if (button->text() == QStringLiteral("취소")) cancel = button;
  QVERIFY(cancel);
  for (QPushButton* button : dialog.findChildren<QPushButton*>())  // picking a colour or a width never takes Enter or the main paint
    QVERIFY2(button == start || button == cancel || !button->autoDefault(), qPrintable(button->text()));
  dialog.setAttribute(Qt::WA_DontShowOnScreen);
  dialog.show();
  QApplication::processEvents();
  QVERIFY2(start->x() < cancel->x(), qPrintable(QStringLiteral("%1 %2").arg(start->x()).arg(cancel->x())));
}

// Without a window only the survey's own area is continued: never a user's file named 조사구역 opened
// for tracing (it would be written on 저장), and the area picked in the layer list first (code review).
void TestSurveyPointDialogs::surveyAreaToContinueIsTheSurveysOwn() {
  QTemporaryDir dir;
  const QString survey = dir.filePath(QStringLiteral("조사.gpkg"));
  const QString outside = dir.filePath(QStringLiteral("조사구역.gpkg"));
  QgsVectorLayer shape(QStringLiteral("Polygon?crs=EPSG:5187"), QStringLiteral("area"), QStringLiteral("memory"));
  const auto write = [&shape](const QString& path, const QString& table, bool addTable) {
    QgsVectorFileWriter::SaveVectorOptions options;
    options.driverName = QStringLiteral("GPKG");
    options.layerName = table;
    options.actionOnExistingFile = addTable ? QgsVectorFileWriter::CreateOrOverwriteLayer : QgsVectorFileWriter::CreateOrOverwriteFile;
    return QgsVectorFileWriter::writeAsVectorFormatV3(&shape, path, QgsCoordinateTransformContext(), options) ==
           QgsVectorFileWriter::NoError;
  };
  QVERIFY(write(survey, QStringLiteral("survey_area"), false) && write(survey, QStringLiteral("survey_area_2"), true));
  QVERIFY(write(outside, QStringLiteral("조사구역"), false));
  QgsProject project;
  auto* own = new QgsVectorLayer(survey + QStringLiteral("|layername=survey_area"), QStringLiteral("조사구역"), QStringLiteral("ogr"));
  auto* second = new QgsVectorLayer(survey + QStringLiteral("|layername=survey_area_2"), QStringLiteral("조사구역 2"), QStringLiteral("ogr"));
  auto* traced = new QgsVectorLayer(outside, QStringLiteral("조사구역"), QStringLiteral("ogr"));
  LayerOps::markSurveyLayer(own, QStringLiteral("survey_area"));
  LayerOps::markSurveyLayer(second, QStringLiteral("survey_area"));
  QVERIFY(own->isValid() && second->isValid() && traced->isValid());
  project.addMapLayers({traced, own, second});
  QgsVectorLayer* chosen = KaSurveyAreaDialog::layerToContinue(&project, survey, traced);
  QVERIFY2(chosen == own || chosen == second, "사용자가 연 「조사구역」 파일에 이어 그리면 안 됩니다.");
  QCOMPARE(KaSurveyAreaDialog::layerToContinue(&project, survey, second), second);  // the one picked in the list
  QCOMPARE(KaSurveyAreaDialog::layerToContinue(&project, dir.filePath(QStringLiteral("다른 조사.gpkg")), nullptr),
           static_cast<QgsVectorLayer*>(nullptr));  // none of this survey's own: ask with the window
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
