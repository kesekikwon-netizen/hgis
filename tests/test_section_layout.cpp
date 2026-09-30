// Task 1 (RED): SectionLayoutService ?? ?? ?? ?? ???
// Task 2 (GREEN): SectionLayoutService::axisTicks / niceDistanceInterval ?? ? ??
// Task 3 (RED->GREEN): QGIS ?? ?? ?? ???

#include <cmath>
#include <limits>
#include <vector>

#include <QtTest>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QVector>

#include <qgis.h>

#include "core/SectionLayoutService.h"

#include <qgsapplication.h>
#include <qgscoordinatereferencesystem.h>
#include <qgslayoutitemmap.h>
#include <qgslayoutitemlabel.h>
#include <qgslayoutitemscalebar.h>
#include <qgslayoutitempolyline.h>
#include <qgslinesymbollayer.h>
#include <qgslinesymbol.h>
#include <qgslayoutmanager.h>
#include <qgslayoutpagecollection.h>
#include <qgslayoutitempage.h>
#include <qgsprintlayout.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>
#include <qgsvectorlayer.h>
#include <qgsrasterrenderer.h>
#include <qgsrastertransparency.h>
#include <qgslayoutpoint.h>
#include <qgstextformat.h>

#include <QPointer>
#include <QTemporaryDir>

#include <gdal.h>
#include <cpl_conv.h>
#include <ogr_spatialref.h>

class TestSectionLayout : public QObject {
    Q_OBJECT
private slots:
    // ---- ?? ?? ----
    void initTestCase();
    void cleanupTestCase();

    // ---- Task 1: axisTicks ?? ----
    void elevationTicksNominal();      // 100.01~100.41, 0.10 -> 4?
    void relativeDistanceTicks();      // 0.0~2.0, 0.5 -> 5?
    void exactBoundaryIncluded();      // 0.0~1.0, 0.5 -> [0.0, 0.5, 1.0] ?? ??

    // ---- Task 1: ?? 1-2-5 ?? ?? ----
    void niceIntervalFor2m();          // span=2.0 -> 0.5
    void niceIntervalFor10m();         // span=10.0 -> 2.0
    void niceIntervalFor100m();        // span=100.0 -> 20.0

    // ---- Task 1: ?? ?? ----
    void invalidIntervalZero();        // interval=0 -> ??
    void invalidIntervalNegative();    // interval=-0.1 -> ??
    void invalidRangeNonFiniteMax();   // maxVal=inf -> ??
    void invalidRangeNonFiniteMin();   // minVal=nan -> ??
    void invalidRangeReversed();       // min>max -> ??
    void invalidRangeEqual();          // min==max -> ??
    void maxTicksExceeded();           // 0.0~1000.0, 0.1 -> 500? ?? ??

    // ---- Task 2: ?? ??? ??? ----
    void integerIndexPrecision();      // ????? ?? ?? ??? ?

    // ---- ?? ?? P2/P3 ?? ----
    void niceIntervalNonFiniteSpan();  // NaN/Inf span -> 1.0 (P2 API ??)
    void axisTicksFirstIdxOverflow();  // ??? long long ?? ?? -> ??? ?? (P3 UB)

    // ---- Task 3: QGIS ???? ?? ??? ----
    void buildLayoutA3();              // A3 ?? 420x297, map CRS=EPSG:5186
    void buildLayoutA4();              // A4 ?? 297x210
    void buildEmptyPaperHasGrid();     // empty layers -> paper + ticks
    void emptyPaperMapKeepsMmFrameAndDummyLayer();
    void sheetMapUsesZoomToExtentOnly();
    void buildLayoutUserCrsLabel();    // mapCrsAuthId overrides raster CRS label
    void buildLayoutUnrotatesWorldGeoref(); // rotated map GT -> 10x2 section plane
    void buildLayoutWorldPlacementSitsHorizontal(); // ??-like map XY, Y not northing
    void buildLayoutKeepsOrthometricElev(); // shear + ?? 100..102 must stay 2.00m
    void buildLayoutElevAxisExists();  // ka_section_elevation_0 ??? ??
    void elevationGridUsesInkNotMidGray();
    void buildLayoutDistAxisStartsZero(); // ka_section_distance_0 ?? "0.00m"
    void buildLayoutReferenceLineStyle(); // ??? #D7191C, ??, 0.20mm
    void buildLayoutAppliesCustomLineStyle(); // 옵션 굵기·색이 기준선에 적용
    void buildLayoutHasScaleBarSamples(); // one Double Box under the map; no samples
    void buildLayoutChromeExists();    // ???.??.??? ?? ??
    void exportSectionPdfNotEmpty();   // PDF ?? ?? & ?? > 100 ???
    // ---- Task 3 ???: raster ? ??? ? map item ??? < 0.5mm ----
    void buildLayoutRasterCornersAligned();
    // ---- Task 3 ???: scaleDenominator ?? ?? ----
    void buildLayoutScaleDenominator();
    void buildLayoutCropsWhiteMargin();
    // ---- F103: requested scale that does not fit is reported, not silent ----
    void scaleThatDoesNotFitWarnsAndSuggestsStandard();
    void largerPaperKeepsRequestedScale();
    // ---- F051: in-place options, skip rebuild, keep user moves ----
    void inputSignatureIgnoresLookOnlyOptions();
    void decorationsApplyInPlace();
    void userMovedChromeSurvivesRebuild();
    void displayLayersStayOutOfProjectAndAreReused();

private:
    QString m_tiffPath; // Task 3 ???? ?? GeoTIFF ??
};

// ---- ?? ----
static void checkTicks(const AxisTickResult &r,
                       const QVector<double> &expected,
                       double tol = 1e-9)
{
    QVERIFY2(r.error.isEmpty(),
             qPrintable(QStringLiteral("?? ??: ") + r.error));
    QCOMPARE(r.ticks.size(), expected.size());
    for (int i = 0; i < expected.size(); ++i) {
        const double diff = std::abs(r.ticks[i] - expected[i]);
        QVERIFY2(diff < tol,
                 qPrintable(QStringLiteral("ticks[%1]: got %2, expected %3, diff %4")
                                .arg(i)
                                .arg(r.ticks[i], 0, 'g', 15)
                                .arg(expected[i], 0, 'g', 15)
                                .arg(diff, 0, 'e', 3)));
    }
}

// ---- ?? ??: 100.01~100.41, ?? 0.10 ----
void TestSectionLayout::elevationTicksNominal()
{
    const auto r = SectionLayoutService::axisTicks(100.01, 100.41, 0.10);
    checkTicks(r, {100.10, 100.20, 100.30, 100.40});
}

// ---- ?? ?? ??: 0.0~2.0m, ?? 0.5 ----
void TestSectionLayout::relativeDistanceTicks()
{
    const auto r = SectionLayoutService::axisTicks(0.0, 2.0, 0.5);
    checkTicks(r, {0.0, 0.5, 1.0, 1.5, 2.0});
}

// ---- ??? ?? ----
void TestSectionLayout::exactBoundaryIncluded()
{
    const auto r = SectionLayoutService::axisTicks(0.0, 1.0, 0.5);
    checkTicks(r, {0.0, 0.5, 1.0});
}

// ---- ?? 1-2-5 ?? ?? ----
void TestSectionLayout::niceIntervalFor2m()
{
    const double v = SectionLayoutService::niceDistanceInterval(2.0);
    QVERIFY2(std::abs(v - 0.5) < 1e-12,
             qPrintable(QStringLiteral("niceDistanceInterval(2.0)=%1, expected 0.5").arg(v)));
}

void TestSectionLayout::niceIntervalFor10m()
{
    const double v = SectionLayoutService::niceDistanceInterval(10.0);
    QVERIFY2(std::abs(v - 2.0) < 1e-12,
             qPrintable(QStringLiteral("niceDistanceInterval(10.0)=%1, expected 2.0").arg(v)));
}

void TestSectionLayout::niceIntervalFor100m()
{
    const double v = SectionLayoutService::niceDistanceInterval(100.0);
    QVERIFY2(std::abs(v - 20.0) < 1e-12,
             qPrintable(QStringLiteral("niceDistanceInterval(100.0)=%1, expected 20.0").arg(v)));
}

// ---- ?? ?? ----
void TestSectionLayout::invalidIntervalZero()
{
    const auto r = SectionLayoutService::axisTicks(0.0, 1.0, 0.0);
    QVERIFY(!r.error.isEmpty());
    QVERIFY(r.ticks.isEmpty());
}

void TestSectionLayout::invalidIntervalNegative()
{
    const auto r = SectionLayoutService::axisTicks(0.0, 1.0, -0.1);
    QVERIFY(!r.error.isEmpty());
    QVERIFY(r.ticks.isEmpty());
}

void TestSectionLayout::invalidRangeNonFiniteMax()
{
    const auto r = SectionLayoutService::axisTicks(0.0,
                                                   std::numeric_limits<double>::infinity(),
                                                   0.1);
    QVERIFY(!r.error.isEmpty());
    QVERIFY(r.ticks.isEmpty());
}

void TestSectionLayout::invalidRangeNonFiniteMin()
{
    const auto r = SectionLayoutService::axisTicks(std::numeric_limits<double>::quiet_NaN(),
                                                   1.0,
                                                   0.1);
    QVERIFY(!r.error.isEmpty());
    QVERIFY(r.ticks.isEmpty());
}

void TestSectionLayout::invalidRangeReversed()
{
    const auto r = SectionLayoutService::axisTicks(5.0, 3.0, 0.1);
    QVERIFY(!r.error.isEmpty());
    QVERIFY(r.ticks.isEmpty());
}

void TestSectionLayout::invalidRangeEqual()
{
    const auto r = SectionLayoutService::axisTicks(2.0, 2.0, 0.1);
    QVERIFY(!r.error.isEmpty());
    QVERIFY(r.ticks.isEmpty());
}

void TestSectionLayout::maxTicksExceeded()
{
    // 0.0~1000.0, ?? 0.1 -> 10001? ??, 500 ?? ??
    const auto r = SectionLayoutService::axisTicks(0.0, 1000.0, 0.1);
    QVERIFY(!r.error.isEmpty());
    QVERIFY(r.ticks.isEmpty());
}

// ---- ?? ??? ??? (????? ?? ??) ----
void TestSectionLayout::integerIndexPrecision()
{
    // 0.0~0.50, ?? 0.10 -> 6?: 0.00, 0.10, 0.20, 0.30, 0.40, 0.50
    const auto r = SectionLayoutService::axisTicks(0.0, 0.50, 0.10);
    checkTicks(r, {0.00, 0.10, 0.20, 0.30, 0.40, 0.50});
}

// ---- ?? ?? P2: niceDistanceInterval NaN/Inf span -> 1.0 ----
void TestSectionLayout::niceIntervalNonFiniteSpan()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    QCOMPARE(SectionLayoutService::niceDistanceInterval(nan), 1.0);
    QCOMPARE(SectionLayoutService::niceDistanceInterval(inf), 1.0);
    QCOMPARE(SectionLayoutService::niceDistanceInterval(-inf), 1.0);
}

// ---- ?? ?? P3: axisTicks firstIdx long long ???? -> ??? ?? ----
void TestSectionLayout::axisTicksFirstIdxOverflow()
{
    // 1.0e19 > LLONG_MAX ~= 9.22e18 ??? rawIdx = ceil(1e19/1.0) = 1e19 > kIdxMax
    const auto r = SectionLayoutService::axisTicks(1.0e19, 2.0e19, 1.0);
    QVERIFY(!r.error.isEmpty());
    QVERIFY(r.ticks.isEmpty());
    // "???" in UTF-8: EC 9D B8 EB 8D B1 EC 8A A4
    QVERIFY2(r.error.contains(QString::fromUtf8("\xec\x9d\xb8\xeb\x8d\xb1\xec\x8a\xa4")),
             qPrintable(QStringLiteral("error missing index keyword (P3 guard missing): ") + r.error));
}

// =========================================================
// Task 3: QGIS ?? ?? ???
// =========================================================

// ---- ???: ?? GeoTIFF ?? ----
// X=0..10m (??), Y=100..102m (??), EPSG:5186, 10x20 ??, Float32
void TestSectionLayout::initTestCase()
{
    GDALAllRegister();
    m_tiffPath = QDir::temp().filePath(QStringLiteral("ka_test_section_task3.tif"));

    GDALDriverH drv = GDALGetDriverByName("GTiff");
    if (!drv) { qWarning("GTiff ???? ?? - Task 3 ??? ???"); return; }

    GDALDatasetH ds = GDALCreate(drv, qUtf8Printable(m_tiffPath), 10, 20, 1, GDT_Float32, nullptr);
    if (!ds) { m_tiffPath.clear(); qWarning("GeoTIFF ?? ??"); return; }

    // geotransform: ??(0, 102), ?? 1m?(-0.1m) -> X=0..10, Y=100..102
    double gt[6] = {0.0, 1.0, 0.0, 102.0, 0.0, -0.1};
    GDALSetGeoTransform(ds, gt);

    // CRS: EPSG:5186
    OGRSpatialReferenceH srs = OSRNewSpatialReference(nullptr);
    OSRImportFromEPSG(srs, 5186);
    char* wkt = nullptr;
    OSRExportToWkt(srs, &wkt);
    GDALSetProjection(ds, wkt);
    CPLFree(wkt);
    OSRDestroySpatialReference(srs);

    // ???: ?? ??? 101.0?? ??
    std::vector<float> data(10 * 20, 101.0f);
    GDALRasterBandH band = GDALGetRasterBand(ds, 1);
    GDALRasterIO(band, GF_Write, 0, 0, 10, 20,
                 data.data(), 10, 20, GDT_Float32, 0, 0);
    GDALClose(ds);

    if (!QFile::exists(m_tiffPath))
        m_tiffPath.clear();
}

void TestSectionLayout::cleanupTestCase()
{
    if (!m_tiffPath.isEmpty())
        QFile::remove(m_tiffPath);
}

// ---- ??: ???? ?? + ???? ??? ??? ?? ----
// QgsProject? raster layer? ????? ???? layer? delete ?? ???.
static SectionLayoutResult buildTestLayout(
    QgsProject* proj,
    const QString& tiffPath,
    QgsRasterLayer** outLayer,
    SectionLayoutOptions::Paper paper = SectionLayoutOptions::Paper::A3)
{
    proj->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    auto* layer = new QgsRasterLayer(tiffPath,
                                     QStringLiteral("test_section"),
                                     QStringLiteral("gdal"));
    if (!layer->isValid()) {
        delete layer; // ???? ??? ???? ?? ? ?? ??
        if (outLayer) *outLayer = nullptr;
        return SectionLayoutResult{QString{}, 0.0, QgsRectangle{},
                                   QStringLiteral("??? ?? ? GeoTIFF ??")};
    }
    // ????? ???? ??? proj ?? ? layer? ?? ????.
    proj->addMapLayer(layer, /*addToLegend=*/false);
    if (outLayer) *outLayer = layer;

    SectionLayoutOptions opts;
    opts.paper = paper;
    return SectionLayoutService::buildSectionLayout(proj, {layer}, opts);
}

// ---- A3 ??: ??? 420x297, map CRS = EPSG:5186 ----
void TestSectionLayout::buildLayoutA3()
{
    if (m_tiffPath.isEmpty()) QSKIP("GeoTIFF ??");
    QgsProject proj;
    QgsRasterLayer* layer = nullptr;
    const auto res = buildTestLayout(&proj, m_tiffPath, &layer,
                                     SectionLayoutOptions::Paper::A3);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));

    auto* ly = dynamic_cast<QgsPrintLayout*>(
        proj.layoutManager()->layoutByName(QStringLiteral("section_sheet")));
    QVERIFY2(ly, "section_sheet ?? ??");

    // ??? ??: A3 ?? 420x297
    QVERIFY(ly->pageCollection() && ly->pageCollection()->pageCount() > 0);
    const QgsLayoutSize ps = ly->pageCollection()->page(0)->pageSize();
    QVERIFY2(std::abs(ps.width()  - 420.0) < 1.0 &&
             std::abs(ps.height() - 297.0) < 1.0,
             qPrintable(QStringLiteral("Page %1x%2 != 420x297").arg(ps.width()).arg(ps.height())));

    // ?? ?? ??
    auto* mapItem = qobject_cast<QgsLayoutItemMap*>(
        ly->itemById(QStringLiteral("ka_section_map")));
    QVERIFY2(mapItem, "ka_section_map ??");

    // ?? CRS = GeoTIFF EPSG:5186 (???? EPSG:5187 ??)
    QCOMPARE(mapItem->crs().authid(), QStringLiteral("EPSG:5186"));

    // ?? ??? ?? ?? ??
    const QRectF mapRect = mapItem->sceneBoundingRect();
    QVERIFY2(mapRect.left()   >= -0.5 && mapRect.right()  <= 421.0 &&
             mapRect.top()    >= -0.5 && mapRect.bottom() <= 298.0,
             qPrintable(QStringLiteral("map rect ?? ??: %1,%2,%3,%4")
                            .arg(mapRect.left()).arg(mapRect.top())
                            .arg(mapRect.right()).arg(mapRect.bottom())));
}

// ---- A4 ??: ??? 297x210 ----
void TestSectionLayout::buildLayoutA4()
{
    if (m_tiffPath.isEmpty()) QSKIP("GeoTIFF ??");
    QgsProject proj;
    QgsRasterLayer* layer = nullptr;
    const auto res = buildTestLayout(&proj, m_tiffPath, &layer,
                                     SectionLayoutOptions::Paper::A4);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));

    auto* ly = dynamic_cast<QgsPrintLayout*>(
        proj.layoutManager()->layoutByName(QStringLiteral("section_sheet")));
    QVERIFY2(ly, "section_sheet ?? ??");

    const QgsLayoutSize ps = ly->pageCollection()->page(0)->pageSize();
    QVERIFY2(std::abs(ps.width()  - 297.0) < 1.0 &&
             std::abs(ps.height() - 210.0) < 1.0,
             qPrintable(QStringLiteral("Page %1x%2 != 297x210").arg(ps.width()).arg(ps.height())));
}

void TestSectionLayout::buildEmptyPaperHasGrid()
{
    QgsProject proj;
    SectionLayoutOptions opts;
    opts.mapCrsAuthId = QStringLiteral("EPSG:5187");
    const auto res = SectionLayoutService::buildSectionLayout(&proj, {}, opts);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));
    QVERIFY2(res.layoutName == QStringLiteral("section_sheet"),
             "empty paper should create section_sheet");

    auto* ly = dynamic_cast<QgsPrintLayout*>(
        proj.layoutManager()->layoutByName(QStringLiteral("section_sheet")));
    QVERIFY2(ly, "section_sheet missing");
    QVERIFY2(ly->itemById(QStringLiteral("ka_section_elevation_0")),
             "empty paper needs elevation ticks");
    QVERIFY2(ly->itemById(QStringLiteral("ka_section_distance_0")),
             "empty paper needs distance ticks");
    auto* crsLbl = qobject_cast<QgsLayoutItemLabel*>(
        ly->itemById(QStringLiteral("ka_section_crs")));
    QVERIFY(crsLbl);
    QVERIFY2(crsLbl->text().contains(QStringLiteral("5187")),
             qPrintable(QStringLiteral("CRS label: ") + crsLbl->text()));
}

void TestSectionLayout::emptyPaperMapKeepsMmFrameAndDummyLayer()
{
    QgsProject proj;
    auto* decoy = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                     QStringLiteral("satellite_fake"),
                                     QStringLiteral("memory"));
    QVERIFY(decoy->isValid());
    proj.addMapLayer(decoy);

    SectionLayoutOptions opts;
    opts.mapCrsAuthId = QStringLiteral("EPSG:5187");
    const auto res = SectionLayoutService::buildSectionLayout(&proj, {}, opts);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));

    auto* ly = dynamic_cast<QgsPrintLayout*>(
        proj.layoutManager()->layoutByName(QStringLiteral("section_sheet")));
    QVERIFY(ly);
    auto* map = qobject_cast<QgsLayoutItemMap*>(
        ly->itemById(QStringLiteral("ka_section_map")));
    QVERIFY2(map, "ka_section_map missing");
    QVERIFY2(!map->layers().isEmpty(),
             "empty setLayers would pull project WMS into the section frame");
    for (QgsMapLayer* ml : map->layers()) {
        QVERIFY2(ml && ml->name() != QLatin1String("satellite_fake"),
                 "section map must not render project layers on an empty GeoTIFF sheet");
    }
    // Empty paper extent is 10 x 2 m; A3 auto scale fills width → 390 x 78 mm.
    QVERIFY2(qAbs(map->rect().width() - 390.0) < 1.0,
             qPrintable(QStringLiteral("frame W %1 mm, expected ~390").arg(map->rect().width())));
    QVERIFY2(qAbs(map->rect().height() - 78.0) < 1.0,
             qPrintable(QStringLiteral("frame H %1 mm, expected ~78 (setExtent must not resize)")
                            .arg(map->rect().height())));
}

void TestSectionLayout::sheetMapUsesZoomToExtentOnly()
{
    QFile f(QStringLiteral("src/core/SectionLayoutService.cpp"));
    QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text),
             "run from source tree (ctest WORKING_DIRECTORY)");
    const QString src = QString::fromUtf8(f.readAll());
    QVERIFY2(src.contains(QLatin1String("map->zoomToExtent(combinedExtent)")),
             "sheet map must zoomToExtent");
    QVERIFY2(!src.contains(QLatin1String("map->setExtent(combinedExtent)")),
             "setExtent resizes the mm frame; ticks then slip");
}

void TestSectionLayout::buildLayoutUserCrsLabel()
{
    if (m_tiffPath.isEmpty()) QSKIP("GeoTIFF missing");
    QgsProject proj;
    QgsRasterLayer* layer = nullptr;
    auto* added = new QgsRasterLayer(m_tiffPath, QStringLiteral("test_section"),
                                     QStringLiteral("gdal"));
    QVERIFY2(added->isValid(), "test GeoTIFF invalid");
    proj.addMapLayer(added, false);
    layer = added;
    Q_UNUSED(layer);

    SectionLayoutOptions opts;
    opts.mapCrsAuthId = QStringLiteral("EPSG:5187");
    const auto res = SectionLayoutService::buildSectionLayout(&proj, {added}, opts);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));

    auto* ly = dynamic_cast<QgsPrintLayout*>(
        proj.layoutManager()->layoutByName(QStringLiteral("section_sheet")));
    QVERIFY(ly);
    auto* crsLbl = qobject_cast<QgsLayoutItemLabel*>(
        ly->itemById(QStringLiteral("ka_section_crs")));
    QVERIFY(crsLbl);
    QVERIFY2(crsLbl->text().contains(QStringLiteral("5187")),
             qPrintable(QStringLiteral("user CRS should label 5187, got ") + crsLbl->text()));
}

void TestSectionLayout::buildLayoutUnrotatesWorldGeoref()
{
    const QString path = QDir::temp().filePath(QStringLiteral("ka_test_section_rotated.tif"));
    GDALDriverH drv = GDALGetDriverByName("GTiff");
    QVERIFY2(drv, "GTiff driver missing");
    GDALDatasetH ds = GDALCreate(drv, qUtf8Printable(path), 10, 20, 1, GDT_Float32, nullptr);
    QVERIFY2(ds, "rotated GeoTIFF create failed");

    const double c = 0.7071067811865476;
    double gt[6] = {
        200000.0,
        1.0 * c,
        0.1 * c,
        450000.0,
        1.0 * c,
        -0.1 * c
    };
    GDALSetGeoTransform(ds, gt);
    OGRSpatialReferenceH srs = OSRNewSpatialReference(nullptr);
    OSRImportFromEPSG(srs, 5186);
    char* wkt = nullptr;
    OSRExportToWkt(srs, &wkt);
    GDALSetProjection(ds, wkt);
    CPLFree(wkt);
    OSRDestroySpatialReference(srs);
    std::vector<float> data(10 * 20, 101.0f);
    GDALRasterBandH band = GDALGetRasterBand(ds, 1);
    GDALRasterIO(band, GF_Write, 0, 0, 10, 20, data.data(), 10, 20, GDT_Float32, 0, 0);
    GDALClose(ds);

    QgsProject proj;
    auto* layer = new QgsRasterLayer(path, QStringLiteral("rotated"), QStringLiteral("gdal"));
    QVERIFY2(layer->isValid(), "rotated layer invalid");
    QVERIFY2(layer->extent().height() > 4.0, "world AABB should be taller than 2m");
    proj.addMapLayer(layer, false);

    SectionLayoutOptions opts;
    opts.mapCrsAuthId = QStringLiteral("EPSG:5187");
    const auto res = SectionLayoutService::buildSectionLayout(&proj, {layer}, opts);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));
    QVERIFY2(std::abs(res.appliedExtent.height() - 2.0) < 0.20,
             qPrintable(QStringLiteral("height %1 expected ~2m")
                            .arg(res.appliedExtent.height(), 0, 'f', 3)));
    QVERIFY2(std::abs(res.appliedExtent.width() - 10.0) < 0.20,
             qPrintable(QStringLiteral("width %1 expected ~10m")
                            .arg(res.appliedExtent.width(), 0, 'f', 3)));
    QFile::remove(path);
}

void TestSectionLayout::buildLayoutWorldPlacementSitsHorizontal()
{
    const QString path = QDir::temp().filePath(QStringLiteral("ka_test_section_gangneung_like.tif"));
    GDALDriverH drv = GDALGetDriverByName("GTiff");
    QVERIFY2(drv, "GTiff driver missing");
    GDALDatasetH ds = GDALCreate(drv, qUtf8Printable(path), 10, 20, 1, GDT_Byte, nullptr);
    QVERIFY2(ds, "world-placement GeoTIFF create failed");

    // ??.tif? ?? ??: ?? ~1m, ??, ??? ?? XY. Z ??.
    double gt[6] = {
        194884.7739783798,
        0.8684753187746654,
        0.4957324083476341,
        574016.921990569,
        0.4957324083499024,
        -0.868475318770692
    };
    GDALSetGeoTransform(ds, gt);
    std::vector<unsigned char> data(10 * 20, 180);
    GDALRasterBandH band = GDALGetRasterBand(ds, 1);
    GDALRasterIO(band, GF_Write, 0, 0, 10, 20, data.data(), 10, 20, GDT_Byte, 0, 0);
    GDALClose(ds);

    QgsProject proj;
    auto* layer = new QgsRasterLayer(path, QStringLiteral("gangneung_like"), QStringLiteral("gdal"));
    QVERIFY2(layer->isValid(), "world-placement layer invalid");
    QVERIFY2(layer->extent().yMinimum() > 10000.0, "source Y should be map northing");
    proj.addMapLayer(layer, false);

    SectionLayoutOptions opts;
    opts.mapCrsAuthId = QStringLiteral("EPSG:5186");
    const auto res = SectionLayoutService::buildSectionLayout(&proj, {layer}, opts);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));
    QVERIFY2(std::abs(res.appliedExtent.xMinimum()) < 0.05,
             qPrintable(QStringLiteral("xMin %1 should be ~0 (distance)")
                            .arg(res.appliedExtent.xMinimum(), 0, 'f', 3)));
    QVERIFY2(std::abs(res.appliedExtent.yMinimum()) < 0.05,
             qPrintable(QStringLiteral("yMin %1 should be ~0, not northing")
                            .arg(res.appliedExtent.yMinimum(), 0, 'f', 3)));
    QVERIFY2(std::abs(res.appliedExtent.width() - 10.0) < 0.20,
             qPrintable(QStringLiteral("width %1 expected ~10m")
                            .arg(res.appliedExtent.width(), 0, 'f', 3)));
    QVERIFY2(std::abs(res.appliedExtent.height() - 20.0) < 0.20,
             qPrintable(QStringLiteral("height %1 expected ~20m (photo Y)")
                            .arg(res.appliedExtent.height(), 0, 'f', 3)));

    auto* ly = dynamic_cast<QgsPrintLayout*>(
        proj.layoutManager()->layoutByName(QStringLiteral("section_sheet")));
    QVERIFY(ly);
    auto* mapItem = qobject_cast<QgsLayoutItemMap*>(
        ly->itemById(QStringLiteral("ka_section_map")));
    QVERIFY(mapItem);
    QVERIFY2(std::abs(mapItem->mapRotation()) < 1e-9, "map must stay unrotated");
    auto* crsLbl = qobject_cast<QgsLayoutItemLabel*>(
        ly->itemById(QStringLiteral("ka_section_crs")));
    QVERIFY(crsLbl);
    QVERIFY2(crsLbl->text().contains(QStringLiteral("5186")),
             qPrintable(QStringLiteral("title CRS: ") + crsLbl->text()));
    QFile::remove(path);
}

void TestSectionLayout::buildLayoutKeepsOrthometricElev()
{
    const QString path = QDir::temp().filePath(QStringLiteral("ka_test_section_elev.tif"));
    GDALDriverH drv = GDALGetDriverByName("GTiff");
    QVERIFY2(drv, "GTiff driver missing");
    GDALDatasetH ds = GDALCreate(drv, qUtf8Printable(path), 10, 20, 1, GDT_Float32, nullptr);
    QVERIFY2(ds, "elev GeoTIFF create failed");

    // ?? ?? + ?? 102?100 (gt[5]=-0.1). hypot(gt[2],gt[5])? ??? 2m? ??.
    double gt[6] = {0.0, 1.0, 0.05, 102.0, 0.0, -0.1};
    GDALSetGeoTransform(ds, gt);
    OGRSpatialReferenceH srs = OSRNewSpatialReference(nullptr);
    OSRImportFromEPSG(srs, 5186);
    char* wkt = nullptr;
    OSRExportToWkt(srs, &wkt);
    GDALSetProjection(ds, wkt);
    CPLFree(wkt);
    OSRDestroySpatialReference(srs);
    std::vector<float> data(10 * 20, 101.0f);
    GDALRasterBandH band = GDALGetRasterBand(ds, 1);
    GDALRasterIO(band, GF_Write, 0, 0, 10, 20, data.data(), 10, 20, GDT_Float32, 0, 0);
    GDALClose(ds);

    QgsProject proj;
    auto* layer = new QgsRasterLayer(path, QStringLiteral("elev"), QStringLiteral("gdal"));
    QVERIFY2(layer->isValid(), "elev layer invalid");
    proj.addMapLayer(layer, false);

    const auto res = SectionLayoutService::buildSectionLayout(&proj, {layer}, SectionLayoutOptions{});
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));
    QVERIFY2(std::abs(res.appliedExtent.height() - 2.0) < 0.05,
             qPrintable(QStringLiteral("?? ?? %1m, ?? 2.00m")
                            .arg(res.appliedExtent.height(), 0, 'f', 3)));
    QVERIFY2(std::abs(res.appliedExtent.yMinimum() - 100.0) < 0.05,
             qPrintable(QStringLiteral("?? ?? %1, ?? 100")
                            .arg(res.appliedExtent.yMinimum(), 0, 'f', 3)));

    auto* ly = dynamic_cast<QgsPrintLayout*>(
        proj.layoutManager()->layoutByName(QStringLiteral("section_sheet")));
    QVERIFY(ly);
    auto* lbl0 = qobject_cast<QgsLayoutItemLabel*>(
        ly->itemById(QStringLiteral("ka_section_elevation_0")));
    QVERIFY(lbl0);
    QCOMPARE(lbl0->text(), QStringLiteral("100.00"));
    QFile::remove(path);
}

// ---- ?? ? ??? ?? ?? ----
// GeoTIFF Y=100..102, interval=0.10 -> ka_section_elevation_0 = "100.00"
void TestSectionLayout::buildLayoutElevAxisExists()
{
    if (m_tiffPath.isEmpty()) QSKIP("GeoTIFF ??");
    QgsProject proj;
    QgsRasterLayer* layer = nullptr;
    const auto res = buildTestLayout(&proj, m_tiffPath, &layer);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));

    auto* ly = dynamic_cast<QgsPrintLayout*>(
        proj.layoutManager()->layoutByName(QStringLiteral("section_sheet")));
    QVERIFY(ly);

    // ? ?? ??? ?? ? ??? ??
    auto* lbl0 = qobject_cast<QgsLayoutItemLabel*>(
        ly->itemById(QStringLiteral("ka_section_elevation_0")));
    QVERIFY2(lbl0, "ka_section_elevation_0 ??");
    QCOMPARE(lbl0->text(), QStringLiteral("100.00"));

    // ?? tick ?? ??
    QVERIFY2(ly->itemById(QStringLiteral("ka_section_elevation_tick_0")),
             "ka_section_elevation_tick_0 ??");

    // ?? ?? ?? ??
    QVERIFY2(ly->itemById(QStringLiteral("ka_section_elevation_axis")),
             "ka_section_elevation_axis ??");

    auto* grid0 = qobject_cast<QgsLayoutItemPolyline*>(
        ly->itemById(QStringLiteral("ka_section_elevation_grid_0")));
    QVERIFY2(grid0, "ka_section_elevation_grid_0 missing (0.10m 해발 가로선)");
    const QPolygonF gridPts = grid0->nodes();
    QVERIFY2(gridPts.size() >= 2, "elevation grid must span the map");
    QVERIFY2(std::abs(gridPts.first().x() - gridPts.last().x()) > 50.0,
             "elevation grid must be a full-width horizontal line");
}

void TestSectionLayout::elevationGridUsesInkNotMidGray()
{
    if (m_tiffPath.isEmpty()) QSKIP("GeoTIFF missing");
    QgsProject proj;
    QgsRasterLayer* layer = nullptr;
    const auto res = buildTestLayout(&proj, m_tiffPath, &layer);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));
    auto* ly = dynamic_cast<QgsPrintLayout*>(
        proj.layoutManager()->layoutByName(QStringLiteral("section_sheet")));
    QVERIFY(ly);
    auto* grid0 = qobject_cast<QgsLayoutItemPolyline*>(
        ly->itemById(QStringLiteral("ka_section_elevation_grid_0")));
    QVERIFY2(grid0, "ka_section_elevation_grid_0 missing");
    QgsLineSymbol* sym = grid0->symbol();
    QVERIFY(sym);
    const QColor c = sym->color();
    QCOMPARE(QColor(c.red(), c.green(), c.blue()), QColor(0x37, 0x41, 0x51));
    QVERIFY2(c != QColor(0x88, 0x88, 0x88), "mid-gray #888888 vanishes on soil photos");
}

// ---- ?? ? ? ??? = "0.00m" ----
void TestSectionLayout::buildLayoutDistAxisStartsZero()
{
    if (m_tiffPath.isEmpty()) QSKIP("GeoTIFF ??");
    QgsProject proj;
    QgsRasterLayer* layer = nullptr;
    const auto res = buildTestLayout(&proj, m_tiffPath, &layer);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));

    auto* ly = dynamic_cast<QgsPrintLayout*>(
        proj.layoutManager()->layoutByName(QStringLiteral("section_sheet")));
    QVERIFY(ly);

    auto* lbl0 = qobject_cast<QgsLayoutItemLabel*>(
        ly->itemById(QStringLiteral("ka_section_distance_0")));
    QVERIFY2(lbl0, "ka_section_distance_0 ??");
    QCOMPARE(lbl0->text(), QStringLiteral("0.00m"));

    // ?? ?? ?? ??
    QVERIFY2(ly->itemById(QStringLiteral("ka_section_distance_axis")),
             "ka_section_distance_axis ??");
}

// ---- ???: #D7191C, ??, ??? 0.20mm ----
void TestSectionLayout::buildLayoutReferenceLineStyle()
{
    if (m_tiffPath.isEmpty()) QSKIP("GeoTIFF ??");
    QgsProject proj;
    QgsRasterLayer* layer = nullptr;
    const auto res = buildTestLayout(&proj, m_tiffPath, &layer);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));

    auto* ly = dynamic_cast<QgsPrintLayout*>(
        proj.layoutManager()->layoutByName(QStringLiteral("section_sheet")));
    QVERIFY(ly);

    auto* refItem = qobject_cast<QgsLayoutItemPolyline*>(
        ly->itemById(QStringLiteral("ka_section_reference_line")));
    QVERIFY2(refItem, "ka_section_reference_line ??");

    QgsLineSymbol* sym = refItem->symbol();
    QVERIFY2(sym && sym->symbolLayerCount() > 0, "??? ?? ??");

    auto* sl = dynamic_cast<QgsSimpleLineSymbolLayer*>(sym->symbolLayer(0));
    QVERIFY2(sl, "?? ???? QgsSimpleLineSymbolLayer ??");

    // ?? #D7191C = RGB(215, 25, 28)
    const QColor expected(0xD7, 0x19, 0x1C);
    QCOMPARE(sl->color().rgb(), expected.rgb());

    // ?? 0.20mm
    QVERIFY2(std::abs(sl->width() - 0.20) < 0.01,
             qPrintable(QStringLiteral("??? ?? %1mm, ?? 0.20mm").arg(sl->width())));

    // ?? ??
    QCOMPARE(sl->penStyle(), Qt::DashLine);
}

void TestSectionLayout::buildLayoutAppliesCustomLineStyle()
{
    if (m_tiffPath.isEmpty()) QSKIP("GeoTIFF missing");
    QgsProject proj;
    auto* layer = new QgsRasterLayer(m_tiffPath, QStringLiteral("test_section"),
                                     QStringLiteral("gdal"));
    QVERIFY2(layer->isValid(), "test GeoTIFF invalid");
    proj.addMapLayer(layer, false);

    SectionLayoutOptions opts;
    opts.referenceLineWidthMm = 0.45;
    opts.referenceLineColor = QStringLiteral("#0B3D91");
    const auto res = SectionLayoutService::buildSectionLayout(&proj, {layer}, opts);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));

    auto* ly = dynamic_cast<QgsPrintLayout*>(
        proj.layoutManager()->layoutByName(QStringLiteral("section_sheet")));
    QVERIFY(ly);
    auto* refItem = qobject_cast<QgsLayoutItemPolyline*>(
        ly->itemById(QStringLiteral("ka_section_reference_line")));
    QVERIFY2(refItem, "ka_section_reference_line missing");
    auto* sl = dynamic_cast<QgsSimpleLineSymbolLayer*>(refItem->symbol()->symbolLayer(0));
    QVERIFY(sl);
    QCOMPARE(sl->color().rgb(), QColor(QStringLiteral("#0B3D91")).rgb());
    QVERIFY2(std::abs(sl->width() - 0.45) < 0.01,
             qPrintable(QStringLiteral("width %1mm, expected 0.45mm").arg(sl->width())));
}

void TestSectionLayout::buildLayoutHasScaleBarSamples()
{
    // Empty paper matches the field screenshot: no GeoTIFF, A3 ticks only.
    // One Double Box under the map; sample scale bars are a field defect.
    QgsProject proj;
    SectionLayoutOptions opts;
    opts.mapCrsAuthId = QStringLiteral("EPSG:5187");
    const auto res = SectionLayoutService::buildSectionLayout(&proj, {}, opts);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));

    auto* ly = dynamic_cast<QgsPrintLayout*>(
        proj.layoutManager()->layoutByName(QStringLiteral("section_sheet")));
    QVERIFY(ly);

    QVERIFY2(!ly->itemById(QStringLiteral("ka_section_scale_bar_single")),
             "sample Single Box scale bar must not appear on the sheet");
    QVERIFY2(!ly->itemById(QStringLiteral("ka_section_scale_bar_ticks")),
             "sample Line Ticks scale bar must not appear on the sheet");
    QVERIFY2(!ly->itemById(QStringLiteral("ka_section_scale_bar_numeric")),
             "sample Numeric scale bar must not appear on the sheet");

    QList<QgsLayoutItemScaleBar*> bars;
    ly->layoutItems(bars);
    QCOMPARE(bars.size(), 1);

    auto* map = qobject_cast<QgsLayoutItemMap*>(
        ly->itemById(QStringLiteral("ka_section_map")));
    QVERIFY2(map, "ka_section_map missing");

    auto* sb = qobject_cast<QgsLayoutItemScaleBar*>(
        ly->itemById(QStringLiteral("ka_section_scale_bar")));
    QVERIFY2(sb, "ka_section_scale_bar missing");
    QCOMPARE(sb->style(), QStringLiteral("Double Box"));
    QVERIFY2(sb->linkedMap() == map, "scale bar must link to ka_section_map");
    QVERIFY2(sb->unitsPerSegment() > 0.0, "scale bar must not stay 0 m");
    QCOMPARE(sb->segmentSizeMode(), Qgis::ScaleBarSegmentSizeMode::Fixed);
    QVERIFY2(sb->rect().width() >= 40.0,
             qPrintable(QStringLiteral("scale bar paper width %1 mm, need >= 40")
                            .arg(sb->rect().width())));
    QVERIFY2(sb->rect().height() >= 11.0,
             "scale bar slot must fit Double Box labels");
    const QRectF mapScene = map->sceneBoundingRect();
    const QRectF sbScene = sb->sceneBoundingRect();
    QVERIFY2(sbScene.top() + 0.5 >= mapScene.bottom(),
             "scale bar must sit below the map, not on the sheet top");
    QVERIFY2(mapScene.bottom() <= sbScene.top() + 16.0,
             "scale bar sits immediately under the distance ticks, not in empty paper");
    QVERIFY2(qAbs(sbScene.left() - mapScene.left()) < 1.0,
             "scale bar left-aligned with map (0 m origin), not bottom-right");
    QVERIFY2(sbScene.top() > 40.0,
             "scale bar must not sit on the top of the sheet (that is the sample strip)");
}

// ---- ???.??.??? ?? ?? ----
void TestSectionLayout::buildLayoutChromeExists()
{
    if (m_tiffPath.isEmpty()) QSKIP("GeoTIFF ??");
    QgsProject proj;
    QgsRasterLayer* layer = nullptr;
    const auto res = buildTestLayout(&proj, m_tiffPath, &layer);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));

    auto* ly = dynamic_cast<QgsPrintLayout*>(
        proj.layoutManager()->layoutByName(QStringLiteral("section_sheet")));
    QVERIFY(ly);

    QVERIFY2(ly->itemById(QStringLiteral("ka_section_scale_bar")),
             "ka_section_scale_bar ??");
    QVERIFY2(ly->itemById(QStringLiteral("ka_section_title_block")),
             "ka_section_title_block ??");
    QVERIFY2(ly->itemById(QStringLiteral("ka_section_crs")),
             "ka_section_crs ??");
    QVERIFY2(ly->itemById(QStringLiteral("ka_section_scale")),
             "ka_section_scale ??");

    // ??? ?? ?? ?? ??
    auto* tb = qobject_cast<QgsLayoutItemLabel*>(
        ly->itemById(QStringLiteral("ka_section_title_block")));
    QVERIFY(tb);
    QVERIFY2(tb->text().contains(QStringLiteral("5186")) ||
             tb->text().contains(QStringLiteral("m")) ,
             qPrintable(QStringLiteral("?? ??? ??: ") + tb->text()));

    // ??? ???? "EPSG:5186" ?? ??
    auto* crsLbl = qobject_cast<QgsLayoutItemLabel*>(
        ly->itemById(QStringLiteral("ka_section_crs")));
    QVERIFY(crsLbl);
    QVERIFY2(crsLbl->text().contains(QStringLiteral("5186")),
             qPrintable(QStringLiteral("CRS ???? '5186' ??: ") + crsLbl->text()));
}

// ---- PDF ????: ?? ?? & ?? > 100 ??? ----
void TestSectionLayout::exportSectionPdfNotEmpty()
{
    if (m_tiffPath.isEmpty()) QSKIP("GeoTIFF ??");
    QgsProject proj;
    QgsRasterLayer* layer = nullptr;
    const auto res = buildTestLayout(&proj, m_tiffPath, &layer);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));

    const QString pdfPath = QDir::temp().filePath(
        QStringLiteral("ka_test_section_layout_task3.pdf"));
    QFile::remove(pdfPath);

    QString errStr;
    const QString out = SectionLayoutService::exportSectionPdf(&proj, pdfPath, &errStr);
    QVERIFY2(!out.isEmpty(),
             qPrintable(QStringLiteral("exportSectionPdf ??: ") + errStr));
    QVERIFY2(QFile::exists(pdfPath), "PDF ?? ???? ??");
    QVERIFY2(QFileInfo(pdfPath).size() > 100,
             qPrintable(QStringLiteral("PDF ?? %1 ??? (?? ??)")
                            .arg(QFileInfo(pdfPath).size())));

    QFile::remove(pdfPath);
}

// ---- [RED] raster ? ??? ? map item ? ??? < 0.5mm ----
// GeoTIFF: X=0..10, Y=100..102 (EPSG:5186, 10x2m)
// ?? ??? 390?261mm ???? 2m ??? ?? ???? Y extent? ~6.69m?
// ???? raster ??? map item ??? ~92mm ???? ? FAIL ??.
// ---- [RED] raster ? ?? ? map item ?? extent < 0.5mm ----
// GeoTIFF: X=0..10, Y=100..102 (EPSG:5186, 10x2m, aspect 5:1)
// ?? ??: 390?261mm ??? ? zoomToExtent ? Y extent ~6.69m ??
// ? raster yMin/yMax ?? ~2.35m ? 0.5mm ??? ? FAIL ??.
void TestSectionLayout::buildLayoutRasterCornersAligned()
{
    if (m_tiffPath.isEmpty()) QSKIP("GeoTIFF ??");
    QgsProject proj;
    const auto res = buildTestLayout(&proj, m_tiffPath, nullptr);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));

    auto* ly = dynamic_cast<QgsPrintLayout*>(
        proj.layoutManager()->layoutByName(QStringLiteral("section_sheet")));
    QVERIFY2(ly, "section_sheet ???? ??");

    auto* mapItem = qobject_cast<QgsLayoutItemMap*>(
        ly->itemById(QStringLiteral("ka_section_map")));
    QVERIFY2(mapItem, "ka_section_map ??");

    // GeoTIFF extent in EPSG:5186 (meters)
    constexpr double xMin = 0.0, xMax = 10.0, yMin = 100.0, yMax = 102.0;

    // ?? ??: 0.5mm ? ?? ?? m = 0.5 ? scale / 1000
    const double scale = mapItem->scale();
    const double tolM  = 0.5 * scale / 1000.0;

    // map->extent()? raster extent? ???? ???? ??? ??? ??? ?
    const QgsRectangle mapExt = mapItem->extent();
    QVERIFY2(std::abs(mapExt.xMinimum() - xMin) < tolM,
             qPrintable(QStringLiteral("xMin: got %1, expected %2, tol %3m (scale=%4)")
                            .arg(mapExt.xMinimum(), 0, 'f', 6).arg(xMin)
                            .arg(tolM, 0, 'f', 6).arg(scale, 0, 'f', 2)));
    QVERIFY2(std::abs(mapExt.xMaximum() - xMax) < tolM,
             qPrintable(QStringLiteral("xMax: got %1, expected %2, tol %3m")
                            .arg(mapExt.xMaximum(), 0, 'f', 6).arg(xMax).arg(tolM, 0, 'f', 6)));
    QVERIFY2(std::abs(mapExt.yMinimum() - yMin) < tolM,
             qPrintable(QStringLiteral("yMin: got %1, expected %2, tol %3m")
                            .arg(mapExt.yMinimum(), 0, 'f', 6).arg(yMin).arg(tolM, 0, 'f', 6)));
    QVERIFY2(std::abs(mapExt.yMaximum() - yMax) < tolM,
             qPrintable(QStringLiteral("yMax: got %1, expected %2, tol %3m")
                            .arg(mapExt.yMaximum(), 0, 'f', 6).arg(yMax).arg(tolM, 0, 'f', 6)));
}

// ---- [RED] scaleDenominator ??: ??? ?? ?? ?? ----
// 10x2m raster, 1:50 ? frameW=200mm, frameH=40mm (A3 390?261mm ?? ?? ? ??)
// ?? ??? scaleDenominator ??? ???? ??? ?? ??? ??? ? ? FAIL ??.
// ---- [RED] scaleDenominator ??: ??? ?? ?? ?? ----
// 10x2m raster, 1:50 ? frameW=200mm, frameH=40mm (A3 390?261mm ?? ?? ? ??)
// ?? ???? scaleDenominator ??? ?? ??? ?? ? FAIL ??.
void TestSectionLayout::buildLayoutScaleDenominator()
{
    if (m_tiffPath.isEmpty()) QSKIP("GeoTIFF ??");
    QgsProject proj;
    proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    auto* layer = new QgsRasterLayer(m_tiffPath,
                                     QStringLiteral("test_section_scale"),
                                     QStringLiteral("gdal"));
    QVERIFY2(layer->isValid(), "??? ??");
    proj.addMapLayer(layer, false);

    SectionLayoutOptions opts;
    opts.paper = SectionLayoutOptions::Paper::A3;
    opts.scaleDenominator = 50.0; // 1:50 ? 10x2m ? 200?40mm, ??? ???

    const auto res = SectionLayoutService::buildSectionLayout(&proj, {layer}, opts);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));

    // ?? ?? ??? 50? ???? ?? (1.0 ??)
    QVERIFY2(std::abs(res.appliedScaleDenominator - 50.0) < 1.0,
             qPrintable(QStringLiteral("appliedScaleDenominator: %1, expected ~50")
                            .arg(res.appliedScaleDenominator, 0, 'f', 2)));

    // ?? ??? ??: 200?40mm ? 1mm
    auto* ly = dynamic_cast<QgsPrintLayout*>(
        proj.layoutManager()->layoutByName(QStringLiteral("section_sheet")));
    QVERIFY2(ly, "section_sheet ??");
    auto* mapItem = qobject_cast<QgsLayoutItemMap*>(
        ly->itemById(QStringLiteral("ka_section_map")));
    QVERIFY2(mapItem, "ka_section_map ??");
    QVERIFY2(std::abs(mapItem->rect().width()  - 200.0) < 1.0,
             qPrintable(QStringLiteral("map width: %1mm, expected ~200mm").arg(mapItem->rect().width(), 0, 'f', 2)));
    QVERIFY2(std::abs(mapItem->rect().height() -  40.0) < 1.0,
             qPrintable(QStringLiteral("map height: %1mm, expected ~40mm").arg(mapItem->rect().height(), 0, 'f', 2)));
}

void TestSectionLayout::buildLayoutCropsWhiteMargin()
{
    const QString path = QDir::temp().filePath(QStringLiteral("ka_test_section_white_margin.tif"));
    GDALDriverH drv = GDALGetDriverByName("GTiff");
    QVERIFY2(drv, "GTiff driver missing");
    const int cols = 40;
    const int rows = 20;
    GDALDatasetH ds = GDALCreate(drv, qUtf8Printable(path), cols, rows, 3, GDT_Byte, nullptr);
    QVERIFY2(ds, "white-margin GeoTIFF create failed");
    double gt[6] = {0.0, 0.01, 0.0, 0.20, 0.0, -0.01};
    GDALSetGeoTransform(ds, gt);
    std::vector<unsigned char> r(cols * rows, 255);
    std::vector<unsigned char> g(cols * rows, 255);
    std::vector<unsigned char> b(cols * rows, 255);
    for (int y = 4; y < 16; ++y) {
        for (int x = 8; x < 32; ++x) {
            r[y * cols + x] = 120;
            g[y * cols + x] = 80;
            b[y * cols + x] = 50;
        }
    }
    GDALRasterIO(GDALGetRasterBand(ds, 1), GF_Write, 0, 0, cols, rows, r.data(), cols, rows, GDT_Byte, 0, 0);
    GDALRasterIO(GDALGetRasterBand(ds, 2), GF_Write, 0, 0, cols, rows, g.data(), cols, rows, GDT_Byte, 0, 0);
    GDALRasterIO(GDALGetRasterBand(ds, 3), GF_Write, 0, 0, cols, rows, b.data(), cols, rows, GDT_Byte, 0, 0);
    GDALClose(ds);

    QgsProject proj;
    auto* layer = new QgsRasterLayer(path, QStringLiteral("white_margin"), QStringLiteral("gdal"));
    QVERIFY2(layer->isValid(), "white-margin layer invalid");
    proj.addMapLayer(layer, false);

    const auto res = SectionLayoutService::buildSectionLayout(&proj, {layer}, SectionLayoutOptions{});
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));
    QVERIFY2(std::abs(res.appliedExtent.width() - 0.24) < 0.02,
             qPrintable(QStringLiteral("width %1 expected 0.24m after crop")
                            .arg(res.appliedExtent.width(), 0, 'f', 4)));
    QVERIFY2(std::abs(res.appliedExtent.height() - 0.12) < 0.02,
             qPrintable(QStringLiteral("height %1 expected 0.12m after crop")
                            .arg(res.appliedExtent.height(), 0, 'f', 4)));

    // Display rasters belong to the section sheet, not to the saved project.
    auto* sheet = dynamic_cast<QgsPrintLayout*>(
        proj.layoutManager()->layoutByName(QStringLiteral("section_sheet")));
    QVERIFY(sheet);
    auto* sheetMap = qobject_cast<QgsLayoutItemMap*>(sheet->itemById(QStringLiteral("ka_section_map")));
    QVERIFY(sheetMap);
    bool knocked = false;
    for (QgsMapLayer* ml : sheetMap->layers()) {
        auto* rl = qobject_cast<QgsRasterLayer*>(ml);
        if (!rl || !rl->renderer() || !rl->renderer()->rasterTransparency()) continue;
        const auto list = rl->renderer()->rasterTransparency()->transparentThreeValuePixelList();
        for (const auto& px : list) {
            if (std::abs(px.red - 255.0) < 1e-6 && px.opacity < 0.01)
                knocked = true;
        }
    }
    QVERIFY2(knocked, "white paper pixels should be transparent");
    QFile::remove(path);
}

static QgsPrintLayout* sectionSheet(QgsProject& proj)
{
    return dynamic_cast<QgsPrintLayout*>(
        proj.layoutManager()->layoutByName(QStringLiteral("section_sheet")));
}

void TestSectionLayout::scaleThatDoesNotFitWarnsAndSuggestsStandard()
{
    if (m_tiffPath.isEmpty()) QSKIP("GeoTIFF missing");
    QgsProject proj;
    auto* layer = new QgsRasterLayer(m_tiffPath, QStringLiteral("fit"), QStringLiteral("gdal"));
    QVERIFY(layer->isValid());
    proj.addMapLayer(layer, false);

    // 10 m at 1:20 is 500 mm; the A3 frame holds 390 mm.
    SectionLayoutOptions opts;
    opts.scaleDenominator = 20.0;
    auto res = SectionLayoutService::buildSectionLayout(&proj, {layer}, opts);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));
    QVERIFY2(!res.warningKo.isEmpty(), "a scale that does not fit must not change silently");
    QVERIFY2(res.warningKo.contains(QStringLiteral("1:20")) && res.warningKo.contains(QStringLiteral("1:30")),
             qPrintable(res.warningKo));
    QVERIFY(std::abs(res.appliedScaleDenominator - 10000.0 / 390.0) < 0.5);
    QCOMPARE(res.suggestedScaleDenominator, 30.0);

    // Fill mode: a non-standard 1:26 gets a suggestion but no warning.
    opts.scaleDenominator = 0.0;
    res = SectionLayoutService::buildSectionLayout(&proj, {layer}, opts);
    QVERIFY(res.warningKo.isEmpty());
    QCOMPARE(res.suggestedScaleDenominator, 30.0);

    // A standard scale that fits: nothing to say.
    opts.scaleDenominator = 30.0;
    res = SectionLayoutService::buildSectionLayout(&proj, {layer}, opts);
    QVERIFY(res.warningKo.isEmpty());
    QCOMPARE(res.suggestedScaleDenominator, 0.0);
    QVERIFY(std::abs(res.appliedScaleDenominator - 30.0) < 0.5);
}

void TestSectionLayout::largerPaperKeepsRequestedScale()
{
    QCOMPARE(SectionLayoutService::paperSizeMm(SectionLayoutOptions::Paper::A2), QSizeF(594.0, 420.0));
    QCOMPARE(SectionLayoutService::paperSizeMm(SectionLayoutOptions::Paper::A1), QSizeF(841.0, 594.0));
    if (m_tiffPath.isEmpty()) QSKIP("GeoTIFF missing");
    QgsProject proj;
    auto* layer = new QgsRasterLayer(m_tiffPath, QStringLiteral("a2"), QStringLiteral("gdal"));
    QVERIFY(layer->isValid());
    proj.addMapLayer(layer, false);
    SectionLayoutOptions opts;
    opts.paper = SectionLayoutOptions::Paper::A2;
    opts.scaleDenominator = 20.0;
    const auto res = SectionLayoutService::buildSectionLayout(&proj, {layer}, opts);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));
    QVERIFY2(res.warningKo.isEmpty(), qPrintable(res.warningKo));
    QVERIFY(std::abs(res.appliedScaleDenominator - 20.0) < 0.5);
    QCOMPARE(res.suggestedScaleDenominator, 0.0);
    auto* ly = sectionSheet(proj);
    QVERIFY(ly);
    QCOMPARE(ly->pageCollection()->page(0)->pageSize().width(), 594.0);
}

void TestSectionLayout::inputSignatureIgnoresLookOnlyOptions()
{
    const SectionLayoutOptions base;
    const QByteArray signature = SectionLayoutService::inputSignature({}, base);
    SectionLayoutOptions look = base;
    look.titleKo = QStringLiteral("북벽 단면");
    look.showReferenceLine = false;
    look.referenceLineWidthMm = 0.5;
    look.referenceLineColor = QStringLiteral("#000000");
    look.scaleBarStyle = QStringLiteral("Single Box");
    look.tickLabelPt = 8.0;
    look.elevationPrefix = true;
    look.noteText = QStringLiteral("A-A'");
    QCOMPARE(SectionLayoutService::inputSignature({}, look), signature);
    SectionLayoutOptions geometry = base;
    geometry.elevationIntervalM = 0.5;
    QVERIFY(SectionLayoutService::inputSignature({}, geometry) != signature);
    geometry = base;
    geometry.paper = SectionLayoutOptions::Paper::A2;
    QVERIFY(SectionLayoutService::inputSignature({}, geometry) != signature);
    geometry = base;
    geometry.scaleDenominator = 50.0;
    QVERIFY(SectionLayoutService::inputSignature({}, geometry) != signature);
}

void TestSectionLayout::decorationsApplyInPlace()
{
    if (m_tiffPath.isEmpty()) QSKIP("GeoTIFF missing");
    QgsProject proj;
    QgsRasterLayer* layer = nullptr;
    QVERIFY(buildTestLayout(&proj, m_tiffPath, &layer).errorKo.isEmpty());
    QPointer<QgsPrintLayout> ly = sectionSheet(proj);
    QVERIFY(ly);
    auto* title = qobject_cast<QgsLayoutItemLabel*>(ly->itemById(QStringLiteral("ka_section_title_block")));
    QVERIFY(title);
    title->attemptMove(QgsLayoutPoint(30.0, 20.0, Qgis::LayoutUnit::Millimeters));
    const QPointF moved = title->pos();

    SectionLayoutOptions opts;
    opts.titleKo = QStringLiteral("북벽 단면");
    opts.referenceLineWidthMm = 0.45;
    opts.tickLabelPt = 7.0;
    opts.elevationPrefix = true;
    opts.noteText = QStringLiteral("A-A' 단면");
    QVERIFY(SectionLayoutService::applyDecorationOptions(&proj, opts));
    QVERIFY2(ly && sectionSheet(proj) == ly.data(), "look-only options must not rebuild the sheet");
    QVERIFY(title->text().contains(QStringLiteral("북벽 단면")));
    QCOMPARE(title->pos(), moved);
    auto* elev0 = qobject_cast<QgsLayoutItemLabel*>(ly->itemById(QStringLiteral("ka_section_elevation_0")));
    QVERIFY(elev0);
    QCOMPARE(elev0->text(), QStringLiteral("EL. 100.00"));
    QCOMPARE(elev0->textFormat().size(), 7.0);
    auto* dist0 = qobject_cast<QgsLayoutItemLabel*>(ly->itemById(QStringLiteral("ka_section_distance_0")));
    QVERIFY(dist0);
    QCOMPARE(dist0->text(), QStringLiteral("0.00m"));
    auto* note = qobject_cast<QgsLayoutItemLabel*>(ly->itemById(QStringLiteral("ka_section_note")));
    QVERIFY2(note, "optional note line");
    QCOMPARE(note->text(), QStringLiteral("A-A' 단면"));
    auto* ref = qobject_cast<QgsLayoutItemPolyline*>(ly->itemById(QStringLiteral("ka_section_reference_line")));
    QVERIFY(ref);
    auto* sl = dynamic_cast<QgsSimpleLineSymbolLayer*>(ref->symbol()->symbolLayer(0));
    QVERIFY(sl && std::abs(sl->width() - 0.45) < 0.01);

    opts.showReferenceLine = false;
    opts.noteText.clear();
    QVERIFY(SectionLayoutService::applyDecorationOptions(&proj, opts));
    QVERIFY(!ly->itemById(QStringLiteral("ka_section_reference_line")));
    QVERIFY(!ly->itemById(QStringLiteral("ka_section_note")));

    // Switched back on: the line returns along the exact bottom edge of the map frame.
    opts.showReferenceLine = true;
    QVERIFY(SectionLayoutService::applyDecorationOptions(&proj, opts));
    auto* again = qobject_cast<QgsLayoutItemPolyline*>(ly->itemById(QStringLiteral("ka_section_reference_line")));
    QVERIFY(again);
    auto* map = qobject_cast<QgsLayoutItemMap*>(ly->itemById(QStringLiteral("ka_section_map")));
    QVERIFY(map);
    const QRectF frame = map->mapRectToScene(map->rect());
    const QRectF line = again->mapRectToScene(again->rect());
    QVERIFY2(std::abs(line.bottom() - frame.bottom()) < 0.05 && std::abs(line.left() - frame.left()) < 0.05
                 && std::abs(line.right() - frame.right()) < 0.05,
             qPrintable(QStringLiteral("line %1..%2 @%3 vs frame %4..%5 @%6")
                            .arg(line.left()).arg(line.right()).arg(line.bottom())
                            .arg(frame.left()).arg(frame.right()).arg(frame.bottom())));
}

void TestSectionLayout::userMovedChromeSurvivesRebuild()
{
    if (m_tiffPath.isEmpty()) QSKIP("GeoTIFF missing");
    QgsProject proj;
    QgsRasterLayer* layer = nullptr;
    QVERIFY(buildTestLayout(&proj, m_tiffPath, &layer).errorKo.isEmpty());
    QVERIFY(SectionLayoutService::userMovedItems(&proj).isEmpty());
    auto* scale = sectionSheet(proj)->itemById(QStringLiteral("ka_section_scale"));
    QVERIFY(scale);
    scale->attemptMove(QgsLayoutPoint(300.0, 250.0, Qgis::LayoutUnit::Millimeters));
    const auto moved = SectionLayoutService::userMovedItems(&proj);
    QCOMPARE(moved.size(), 1);
    QVERIFY(moved.contains(QStringLiteral("ka_section_scale")));

    SectionLayoutOptions opts;
    opts.elevationIntervalM = 0.5;
    QVERIFY(SectionLayoutService::buildSectionLayout(&proj, {layer}, opts).errorKo.isEmpty());
    SectionLayoutService::restoreUserMovedItems(&proj, moved);
    auto* rebuilt = sectionSheet(proj)->itemById(QStringLiteral("ka_section_scale"));
    QVERIFY(rebuilt);
    QCOMPARE(rebuilt->pos(), QPointF(300.0, 250.0));
    QVERIFY(!SectionLayoutService::userMovedItems(&proj).contains(QStringLiteral("ka_section_title_block")));
}

void TestSectionLayout::displayLayersStayOutOfProjectAndAreReused()
{
    const QString path = QDir::temp().filePath(QStringLiteral("ka_test_section_reuse.tif"));
    GDALDriverH drv = GDALGetDriverByName("GTiff");
    QVERIFY2(drv, "GTiff driver missing");
    GDALDatasetH ds = GDALCreate(drv, qUtf8Printable(path), 10, 20, 1, GDT_Float32, nullptr);
    QVERIFY2(ds, "reuse GeoTIFF create failed");
    const double c = 0.7071067811865476;
    double gt[6] = {200000.0, 1.0 * c, 0.1 * c, 450000.0, 1.0 * c, -0.1 * c};
    GDALSetGeoTransform(ds, gt);
    std::vector<float> data(10 * 20, 101.0f);
    GDALRasterIO(GDALGetRasterBand(ds, 1), GF_Write, 0, 0, 10, 20, data.data(), 10, 20, GDT_Float32, 0, 0);
    GDALClose(ds);

    QgsProject proj;
    auto* layer = new QgsRasterLayer(path, QStringLiteral("reuse"), QStringLiteral("gdal"));
    QVERIFY(layer->isValid());
    proj.addMapLayer(layer, false);
    SectionLayoutOptions opts;
    opts.mapCrsAuthId = QStringLiteral("EPSG:5187");
    auto displayOf = [&proj]() -> QgsMapLayer* {
        auto* map = qobject_cast<QgsLayoutItemMap*>(sectionSheet(proj)->itemById(QStringLiteral("ka_section_map")));
        for (QgsMapLayer* ml : map ? map->layers() : QList<QgsMapLayer*>()) {
            if (ml && ml->customProperty(QStringLiteral("ka_hgis/section_display")).toBool()) return ml;
        }
        return nullptr;
    };
    QVERIFY(SectionLayoutService::buildSectionLayout(&proj, {layer}, opts).errorKo.isEmpty());
    QCOMPARE(proj.mapLayers().size(), 1);  // the saved survey never lists a temp display file
    QgsMapLayer* first = displayOf();
    QVERIFY2(first, "rotated section must be shown through a display raster");
    QCOMPARE(first->parent(), static_cast<QObject*>(sectionSheet(proj)));
    const QString firstPath = first->customProperty(QStringLiteral("ka_hgis/section_display_path")).toString();
    QVERIFY(QFile::exists(firstPath));

    opts.paper = SectionLayoutOptions::Paper::A4;  // new sheet, same source and plane
    QVERIFY(SectionLayoutService::buildSectionLayout(&proj, {layer}, opts).errorKo.isEmpty());
    QCOMPARE(proj.mapLayers().size(), 1);
    QgsMapLayer* second = displayOf();
    QVERIFY(second);
    QCOMPARE(second->customProperty(QStringLiteral("ka_hgis/section_display_path")).toString(), firstPath);
    QVERIFY2(QFile::exists(firstPath), "the reused derived file must not be deleted");

    // Reopen: the saved project lists only the source; the sheet derives its
    // display raster again from the stored plane, without a full photo scan.
    QTemporaryDir saveDir;
    QVERIFY(saveDir.isValid());
    const QString qgs = saveDir.filePath(QStringLiteral("reopen.qgs"));
    QVERIFY(proj.write(qgs));
    QgsProject reopened;
    QVERIFY(reopened.read(qgs));
    QCOMPARE(reopened.mapLayers().size(), 1);
    QVERIFY(sectionSheet(reopened));
    QVERIFY(SectionLayoutService::restoreDisplayLayers(&reopened));
    auto* reopenedMap = qobject_cast<QgsLayoutItemMap*>(
        sectionSheet(reopened)->itemById(QStringLiteral("ka_section_map")));
    QVERIFY(reopenedMap);
    QCOMPARE(reopenedMap->layers().size(), 1);
    QVERIFY(reopenedMap->layers().first()->isValid());
    QVERIFY(reopenedMap->layers().first()->customProperty(QStringLiteral("ka_hgis/section_display")).toBool());
    QCOMPARE(reopened.mapLayers().size(), 1);
    QVERIFY2(!SectionLayoutService::restoreDisplayLayers(&reopened), "a complete sheet is left alone");

    reopened.removeAllMapLayers();
    proj.removeAllMapLayers();
    QFile::remove(path);
}

#include "test_section_layout.moc"

int main(int argc, char **argv)
{
    QgsApplication app(argc, argv, false);
    const QString prefix = qEnvironmentVariable(
        "QGIS_PREFIX_PATH",
        QFile::exists(QStringLiteral("A:/OSGeo4W/apps/qgis-dev"))
            ? QStringLiteral("A:/OSGeo4W/apps/qgis-dev")
            : QStringLiteral("C:/OSGeo4W/apps/qgis-dev"));
    QgsApplication::setPrefixPath(prefix, true);
    QgsApplication::setPluginPath(prefix + QStringLiteral("/plugins"));
    QgsApplication::initQgis();
    TestSectionLayout tc;
    const int rc = QTest::qExec(&tc, argc, argv);
    QgsApplication::exitQgis();
    return rc;
}
