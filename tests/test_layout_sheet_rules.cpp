// Sheet rules shared by the drawing studio, printing and the submission package:
// one PDF recipe (F009), one standard-scale table (F104), heritage number badges
// that never cover each other and stay on the visible map paper (F024), and the
// logical heritage dataset tag (F162).
#include <QtTest>
#include <QFile>
#include <QFontDatabase>
#include <QImage>
#include <QPdfDocument>
#include <QPdfSelection>
#include <QTemporaryDir>
#include <algorithm>
#include <optional>

#include <qgsapplication.h>
#include <qgsfeature.h>
#include <qgsfillsymbol.h>
#include <qgsgeometry.h>
#include <qgslayertree.h>
#include <qgslayoutitemlabel.h>
#include <qgslayoutitemlegend.h>
#include <qgslayoutitemmap.h>
#include <qgslayoutmanager.h>
#include <qgsprintlayout.h>
#include <qgsproject.h>
#include <qgssinglesymbolrenderer.h>
#include <qgstextformat.h>
#include <qgsvectorlayer.h>

#include "core/HeritageLayoutNumbers.h"
#include "core/HeritageStyle.h"
#include "core/LayerOps.h"
#include "core/LayoutService.h"
#include "core/PdfExportSettings.h"
#include "core/StandardScales.h"
#include "core/TilePrint.h"

namespace {
QgsLayoutItemMap* addMap(QgsPrintLayout& layout, const QList<QgsMapLayer*>& layers, const QgsRectangle& extent) {
  auto* map = new QgsLayoutItemMap(&layout);
  layout.addLayoutItem(map);
  map->attemptSetSceneRect(QRectF(10., 10., 160., 160.));
  map->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  map->setKeepLayerSet(true);
  map->setLayers(layers);
  map->setExtent(extent);
  return map;
}

QgsVectorLayer* heritagePoints(QgsProject& project, const QList<QPair<QString, QgsPointXY>>& sites, const QString& group) {
  auto* layer = new QgsVectorLayer(QStringLiteral("Point?crs=EPSG:5186&field=nm:string(80)"),
                                   QStringLiteral("받은 자료"), QStringLiteral("memory"));
  layer->startEditing();
  for (const auto& site : sites) {
    QgsFeature feature(layer->fields());
    feature.setGeometry(QgsGeometry::fromPointXY(site.second));
    feature.setAttribute(QStringLiteral("nm"), site.first);
    layer->addFeature(feature);
  }
  layer->commitChanges();
  HeritageStyle::apply(layer, HeritageDataset::HeritageDistributionMap, QStringLiteral("nm"));
  LayerOps::markReferenceLayer(layer);
  project.addMapLayer(layer, false);
  project.layerTreeRoot()->addGroup(QStringLiteral("참조 지도"))->addGroup(group)->addLayer(layer);
  return layer;
}
}  // namespace

class LayoutSheetRulesTest : public QObject {
  Q_OBJECT
private slots:
  void sheetPdfSettingsAreOneRecipe() {
    const auto settings = KaPdfExport::sheetSettings();
    QCOMPARE(settings.dpi, 300.);
    QVERIFY(settings.forceVectorOutput);
    QVERIFY(!settings.rasterizeWholeImage);
    QCOMPARE(settings.textRenderFormat, Qgis::TextRenderFormat::PreferText);
    QCOMPARE(KaPdfExport::sheetSettings(150.).dpi, 150.);
    QCOMPARE(KaPdfExport::describe(settings), KaPdfExport::describe(KaPdfExport::sheetSettings(KaPdfExport::kSheetDpi)));
  }

  void studioPdfEqualsSubmissionPdf() {
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    auto* area = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"), QStringLiteral("구역"), QStringLiteral("memory"));
    area->startEditing();
    QgsFeature feature(area->fields());
    feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(190000., 550000., 190100., 550080.)));
    area->addFeature(feature);
    area->commitChanges();
    // Semi-transparent fill: the element a raster fallback used to change.
    area->setRenderer(new QgsSingleSymbolRenderer(QgsFillSymbol::createSimple(
        {{QStringLiteral("color"), QStringLiteral("255,0,0,160")}, {QStringLiteral("outline_color"), QStringLiteral("0,0,0,255")}}).release()));
    project.addMapLayer(area);
    auto* layout = new QgsPrintLayout(&project);
    layout->initializeDefaults();
    layout->setName(QStringLiteral("user_sheet"));
    auto* map = addMap(*layout, {area}, QgsRectangle(189950., 549950., 190150., 550130.));
    map->setId(QStringLiteral("ka_map"));
    auto* label = new QgsLayoutItemLabel(layout);
    label->setText(QStringLiteral("SHEET PARITY 42"));
    QgsTextFormat format;
    format.setFont(QFont(QStringLiteral("Arial")));
    format.setSize(14.);
    format.setSizeUnit(Qgis::RenderUnit::Points);
    label->setTextFormat(format);
    layout->addLayoutItem(label);
    label->attemptSetSceneRect(QRectF(10., 175., 150., 12.));
    QVERIFY(project.layoutManager()->addLayout(layout));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString studio = dir.filePath(QStringLiteral("studio.pdf"));
    const QString submit = dir.filePath(QStringLiteral("submit.pdf"));
    QString error;
    HeritageLayoutNumbers numbers;  // the drawing studio 「PDF 저장」 call
    QVERIFY(numbers.update(map, true));
    QVERIFY2(numbers.exportPdf(map, nullptr, studio, KaPdfExport::kSheetDpi, &error), qPrintable(error));
    QVERIFY2(!LayoutService::exportLayoutPdf(&project, QStringLiteral("user_sheet"), submit, &error).isEmpty(), qPrintable(error));

    QPdfDocument a;
    QPdfDocument b;
    QCOMPARE(a.load(studio), QPdfDocument::Error::None);
    QCOMPARE(b.load(submit), QPdfDocument::Error::None);
    QCOMPARE(a.pageCount(), b.pageCount());
    QCOMPARE(a.pagePointSize(0), b.pagePointSize(0));
    const QString text = a.getAllText(0).text();
    QCOMPARE(text, b.getAllText(0).text());
    QVERIFY2(text.contains(QStringLiteral("PARITY")), qPrintable(QStringLiteral("plain labels stay text: ") + text));
    const QSize px = (a.pagePointSize(0) * (100. / 72.)).toSize();
    const QImage ia = a.render(0, px).convertToFormat(QImage::Format_ARGB32);
    const QImage ib = b.render(0, px).convertToFormat(QImage::Format_ARGB32);
    QCOMPARE(ia.size(), ib.size());
    int differing = 0;
    int red = 0;
    for (int y = 0; y < ia.height(); ++y) {
      for (int x = 0; x < ia.width(); ++x) {
        const QColor ca = ia.pixelColor(x, y);
        const QColor cb = ib.pixelColor(x, y);
        if (std::max({qAbs(ca.red() - cb.red()), qAbs(ca.green() - cb.green()), qAbs(ca.blue() - cb.blue())}) > 24) ++differing;
        if (ca.red() > 200 && ca.green() < 140 && ca.blue() < 140) ++red;
      }
    }
    QVERIFY2(red > 50, "the translucent fill must be drawn");
    QVERIFY2(differing <= ia.width() * ia.height() / 500, qPrintable(QStringLiteral("%1 pixels differ").arg(differing)));
  }

  void standardScalesAreOneTable() {
    const auto chips = StandardScales::denominators(StandardScales::StudioChip);
    for (int d : {20, 30, 50, 100, 200, 250, 300, 400, 500, 1000, 2000, 5000, 10000, 25000})
      QVERIFY2(chips.contains(d), qPrintable(QString::number(d)));
    const auto section = StandardScales::denominators(StandardScales::Section);
    for (int d : {10, 20, 25, 30, 40, 50, 60, 100, 200, 250}) QVERIFY(section.contains(d));
    QVERIFY(std::is_sorted(section.cbegin(), section.cend()));
    QCOMPARE(StandardScales::snapUp(23.), 30);
    QCOMPARE(StandardScales::snapUp(55.), 60);
    QCOMPARE(StandardScales::snapUp(520.), 600);
    QCOMPARE(StandardScales::snapUp(1100.), 1200);
    QCOMPARE(StandardScales::snapUp(2300.), 2500);
    QCOMPARE(StandardScales::snapUp(0.), 0);
    for (double raw : {12., 23., 37., 487., 1847., 21000.}) {
      const int nice = LayoutService::niceScaleDenominator(raw);
      QCOMPARE(nice, StandardScales::snapUp(raw));
      QVERIFY2(nice >= raw && nice % 10 == 0, qPrintable(QString::number(nice)));
    }
    QVERIFY(StandardScales::isStandard(1200.));
    QVERIFY(!StandardScales::isStandard(27.));
    QVERIFY(!StandardScales::isStandard(25., StandardScales::Snap));
    QCOMPARE(StandardScales::label(2500.), QStringLiteral("1:2,500"));
    QCOMPARE(TilePrint::scaleLabel(2500.), StandardScales::label(2500.));
    for (double d : TilePrint::enlargedScales(50000., 1000., 50))
      QVERIFY(StandardScales::isStandard(d, StandardScales::PrintEnlarge));
    for (const auto& recipe : LayoutService::allRecipes())
      for (double d : recipe.scaleChoices) QVERIFY2(StandardScales::isStandard(d), qPrintable(QString::number(d)));
  }

  void excavationScalesGetFineTicks() {
    const double segment = LayoutService::niceScaleBarSegmentMeters(160., 20., 4);
    QVERIFY2(segment >= 0.1 && segment < 0.5, qPrintable(QString::number(segment)));
    QCOMPARE(LayoutService::niceScaleBarSegmentMeters(200., 500., 4), 10.);
    QVERIFY(LayoutService::niceGridIntervalMeters(5., 180.) < 0.5);
    QCOMPARE(LayoutService::niceGridIntervalMeters(100., 180.), 5.);
  }

  void numberBadgesStayOnPaperAndOffLegend() {
    QgsProject project;
    QList<QPair<QString, QgsPointXY>> sites;
    // 150 sites on one spot need more than the old six rings (127 slots).
    for (int i = 0; i < 150; ++i) sites.append({QStringLiteral("밀집 %1").arg(i, 3, 10, QLatin1Char('0')), QgsPointXY(190040., 550040.)});
    sites.append({QStringLiteral("가장자리"), QgsPointXY(186090., 550040.)});  // 1 mm inside the left frame
    sites.append({QStringLiteral("범례 밑"), QgsPointXY(192400., 547600.)});   // under the legend card, placed last
    // Sorts first, so it is placed with the base rings only: 27.5 mm from every
    // card edge is beyond six 4.6 mm rings, and it must still get clear.
    sites.append({QStringLiteral("가 범례 밑 첫째"), QgsPointXY(192415., 547665.)});
    auto* layer = heritagePoints(project, sites, HeritageStyle::layerName(HeritageDataset::HeritageDistributionMap));
    QgsPrintLayout layout(&project);
    layout.initializeDefaults();
    auto* map = addMap(layout, {layer}, QgsRectangle(186040., 546040., 194040., 554040.));  // 1:50,000
    auto* legend = new QgsLayoutItemLegend(&layout);
    layout.addLayoutItem(legend);
    legend->setResizeToContents(false);
    const QRectF legendRect(110., 110., 55., 55.);
    legend->attemptSetSceneRect(legendRect);
    HeritageLayoutNumbers numbers;
    QVERIFY(numbers.update(map, true));
    QVERIFY2(numbers.error().isEmpty(), qPrintable(numbers.error()));
    QCOMPARE(numbers.entries().size(), 153);
    QCOMPARE(numbers.legendKeys().size(), 153);
    const QTransform toScene = map->layoutToMapCoordsTransform().inverted();
    const double minSep = (4.6 / 1000.) * map->scale() * 0.96 - 1.;
    QVector<QgsPointXY> pins;
    QgsFeature pin;
    auto rows = numbers.numberLayer()->getFeatures();
    while (rows.nextFeature(pin)) {
      const QgsPointXY p = pin.geometry().asPoint();
      const double r = pin.attribute(QStringLiteral("size")).toDouble() / 2.;
      const QPointF paper = toScene.map(QPointF(p.x(), p.y()));
      QVERIFY2(QRectF(10., 10., 160., 160.).adjusted(r, r, -r, -r).contains(paper),
               qPrintable(QStringLiteral("badge %1 leaves the frame at %2,%3").arg(pin.attribute(QStringLiteral("num")).toInt()).arg(paper.x()).arg(paper.y())));
      QVERIFY2(!legendRect.adjusted(-r, -r, r, r).contains(paper), "badge sits on the legend card");
      pins.append(p);
    }
    QCOMPARE(pins.size(), 153);
    for (int i = 0; i < pins.size(); ++i)
      for (int j = i + 1; j < pins.size(); ++j)
        QVERIFY2(pins.at(i).distance(pins.at(j)) >= minSep, "number circles must not cover each other");
  }

  void datasetTagSurvivesRenamedGroup() {
    QgsProject project;
    auto* layer = heritagePoints(project, {{QStringLiteral("가"), QgsPointXY(190040., 550040.)},
                                           {QStringLiteral("나"), QgsPointXY(190400., 550400.)}},
                                 QStringLiteral("내가 바꾼 이름"));
    layer->removeCustomProperty(HeritageLayoutNumbers::datasetPropertyKey());
    QgsPrintLayout layout(&project);
    layout.initializeDefaults();
    auto* map = addMap(layout, {layer}, QgsRectangle(186040., 546040., 194040., 554040.));
    {
      HeritageLayoutNumbers untagged;
      QVERIFY(untagged.update(map, true));
      QVERIFY2(untagged.entries().isEmpty(), "a renamed group alone cannot tell the dataset");
    }
    HeritageLayoutNumbers::tagDataset(layer, HeritageDataset::SurfaceSurveyArea);
    QVERIFY(HeritageLayoutNumbers::taggedDataset(layer) == HeritageDataset::SurfaceSurveyArea);
    HeritageLayoutNumbers numbers;
    QVERIFY(numbers.update(map, true));
    QCOMPARE(numbers.entries().size(), 2);
    QCOMPARE(numbers.entries().first().dataset, HeritageStyle::layerName(HeritageDataset::SurfaceSurveyArea));
    QCOMPARE(numbers.entries().first().number, 1);
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  const QString fonts = qEnvironmentVariable("WINDIR", QStringLiteral("C:/Windows")) + QStringLiteral("/Fonts/");
  for (const QString& file : {QStringLiteral("malgun.ttf"), QStringLiteral("malgunbd.ttf"), QStringLiteral("arial.ttf")})
    QFontDatabase::addApplicationFont(fonts + file);
  const QString prefix = qEnvironmentVariable(
      "QGIS_PREFIX_PATH", QFile::exists(QStringLiteral("A:/OSGeo4W/apps/qgis-dev")) ? QStringLiteral("A:/OSGeo4W/apps/qgis-dev")
                                                                                    : QStringLiteral("C:/OSGeo4W/apps/qgis-dev"));
  QgsApplication::setPrefixPath(prefix, true);
  QgsApplication::initQgis();
  LayoutSheetRulesTest test;
  const int result = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis();
  return result;
}
#include "test_layout_sheet_rules.moc"
