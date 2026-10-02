#include "SectionLayoutService.h"
#include "LayoutService.h"
#include "PdfExportSettings.h"
#include "SectionSheetDecor.h"
#include "StandardScales.h"

#include <QColor>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QList>
#include <QLocale>
#include <QMutex>
#include <QMutexLocker>
#include <QPolygonF>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include <qgis.h>
#include <qgscoordinatereferencesystem.h>
#include <qgslayout.h>
#include <qgslayoutexporter.h>
#include <qgslayoutitemlabel.h>
#include <qgslayoutitemmap.h>
#include <qgslayoutitempage.h>
#include <qgslayoutitempolyline.h>
#include <qgsfillsymbol.h>
#include <qgslayoutitemscalebar.h>
#include <qgslayoutmanager.h>
#include <qgslayoutmeasurement.h>
#include <qgslayoutpagecollection.h>
#include <qgslayoutsize.h>
#include <qgslinesymbol.h>
#include <qgsmaplayer.h>
#include <qgsmasterlayoutinterface.h>
#include <qgsprintlayout.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#include <qgsrasterdataprovider.h>
#include <qgsrasterlayer.h>
#include <qgsrasterrenderer.h>
#include <qgsrastertransparency.h>
#include <qgsrectangle.h>
#include <qgstextformat.h>

#include <cpl_string.h>
#include <cpl_vsi.h>
#include <gdal_priv.h>

// ---- axisTicks ----

AxisTickResult SectionLayoutService::axisTicks(double minVal, double maxVal, double interval)
{
    AxisTickResult result;

    // 범위 유효성 검사
    if (!std::isfinite(minVal) || !std::isfinite(maxVal)) {
        result.error = QStringLiteral("범위가 유한하지 않습니다 (무한대 또는 NaN).");
        return result;
    }
    if (minVal >= maxVal) {
        result.error = QStringLiteral("최솟값이 최댓값 이상입니다 (min >= max).");
        return result;
    }

    // 간격 유효성 검사
    if (!std::isfinite(interval) || interval <= 0.0) {
        result.error = QStringLiteral("눈금 간격은 0보다 커야 합니다.");
        return result;
    }

    // 첫 눈금 인덱스: minVal 이상인 최소 정수 배수
    // 정수 인덱스 곱셈 방식으로 부동소수점 누적 오차를 방지한다.
    //
    // P3 UB 방지: double → long long 캐스트 전에 rawIdx가 유한하고
    // long long 범위 안에 있는지 확인한다. 안전 상한 9.0e18은
    // LLONG_MAX(≈9.22e18)보다 작아 캐스트와 루프 내 ++i 오버플로를 방지한다.
    const double rawIdx = std::ceil(minVal / interval);
    constexpr double kIdxMax =  9.0e18; // LLONG_MAX ≈ 9.22e18 에서 안전 마진
    constexpr double kIdxMin = -9.0e18;
    if (!std::isfinite(rawIdx) || rawIdx > kIdxMax || rawIdx < kIdxMin) {
        result.error = QStringLiteral("눈금 인덱스가 표현 범위를 초과합니다. 범위나 간격을 조정해 주세요.");
        return result;
    }
    const long long firstIdx = static_cast<long long>(rawIdx);

    // 작은 엡실론: 구간 끝 경계 포함 여부 판정에 사용 (1e-9 * interval)
    const double eps = 1e-9 * interval;

    QVector<double> ticks;
    for (long long i = firstIdx; ; ++i) {
        const double v = static_cast<double>(i) * interval;
        if (v > maxVal + eps) {
            break;
        }
        ticks.append(v);
        if (ticks.size() > 500) {
            result.error = QStringLiteral("눈금이 500개를 초과합니다. 간격을 늘려주세요.");
            return result;
        }
    }

    result.ticks = std::move(ticks);
    return result;
}

// ---- niceDistanceInterval ----

double SectionLayoutService::niceDistanceInterval(double span, int targetTickCount)
{
    // span이 NaN/Inf이면 log10/pow에서 비정상값이 전파된다 (P2 API 계약).
    if (!std::isfinite(span) || span <= 0.0 || targetTickCount < 1) {
        return 1.0;
    }

    const double rough     = span / static_cast<double>(targetTickCount);
    const double magnitude = std::pow(10.0, std::floor(std::log10(rough)));

    // 1-2-5-10 계열에서 조건을 만족하는 가장 작은 간격을 선택한다.
    // span/v <= targetTickCount + 2 이면 수용 가능한 개수로 판단한다.
    const double candidates[] = {1.0, 2.0, 5.0, 10.0};
    const double limit = static_cast<double>(targetTickCount) + 2.0;
    for (double c : candidates) {
        const double v = c * magnitude;
        if (span / v <= limit) {
            return v;
        }
    }

    // 모두 실패하면 가장 큰 후보(10 * magnitude)를 반환한다.
    return 10.0 * magnitude;
}

namespace {

constexpr const char* kPropSectionDisplay = "ka_hgis/section_display";
constexpr const char* kPropSectionDisplayPath = "ka_hgis/section_display_path";

bool yLooksLikeElevation(const QgsRectangle& ext)
{
    return ext.isFinite()
        && ext.yMinimum() > -500.0 && ext.yMaximum() < 10000.0
        && ext.height() > 0.0 && ext.height() <= 500.0;
}

/// 해발(m). 지도 북ing(~1.4e6)은 제외.
bool looksLikeOrthometricElev(double a, double b)
{
    return std::isfinite(a) && std::isfinite(b)
        && std::abs(a) < 10000.0 && std::abs(b) < 10000.0
        && std::abs(a - b) > 0.0 && std::abs(a - b) <= 500.0;
}

QString xmlEscape(const QString& s)
{
    QString o = QDir::fromNativeSeparators(s);
    o.replace(QLatin1Char('&'), QLatin1String("&amp;"));
    o.replace(QLatin1Char('<'), QLatin1String("&lt;"));
    o.replace(QLatin1Char('>'), QLatin1String("&gt;"));
    o.replace(QLatin1Char('"'), QLatin1String("&quot;"));
    return o;
}

struct SectionPlane {
    QgsRasterLayer* src = nullptr;
    QgsRectangle extent;
    bool flatten = false;
    bool worldPlacement = false; ///< 4x4가 지도 XY. 픽셀 축이 거리×해발.
    double lengthM = 0.0;
    double heightM = 0.0;
    double elevBottom = 0.0;
    int cropX0 = 0;
    int cropY0 = 0;
    int cropW = 0;  // 0이면 원본 전체
    int cropH = 0;
};

// Descartes 단면 TIFF는 벽 사진 둘레에 흰 용지 여백이 있다.
// RGB 바이트만 본다. 단밴드·Float 테스트 래스터는 손대지 않는다.
struct PhotoBox {
    int x0 = 0;
    int y0 = 0;
    int w = 0;
    int h = 0;
    bool cropped = false;
};

bool findPhotoPixelBox(GDALDataset* ds, PhotoBox* out)
{
    if (!ds || !out) return false;
    const int cols = ds->GetRasterXSize();
    const int rows = ds->GetRasterYSize();
    const int nBands = ds->GetRasterCount();
    if (cols < 4 || rows < 4 || nBands < 3) return false;
    for (int b = 1; b <= 3; ++b) {
        GDALRasterBand* band = ds->GetRasterBand(b);
        if (!band || band->GetRasterDataType() != GDT_Byte) return false;
    }

    constexpr unsigned char kPaper = 248;  // 용지 흰색(압축 잡음 포함)
    int minX = cols;
    int minY = rows;
    int maxX = -1;
    int maxY = -1;
    std::vector<unsigned char> row(static_cast<size_t>(cols) * 3);
    const int bands[3] = {1, 2, 3};
    for (int y = 0; y < rows; ++y) {
        const CPLErr err = ds->RasterIO(GF_Read, 0, y, cols, 1, row.data(), cols, 1,
                                        GDT_Byte, 3, bands, 3, 0, 1);
        if (err != CE_None) return false;
        for (int x = 0; x < cols; ++x) {
            const unsigned char r = row[static_cast<size_t>(x) * 3];
            const unsigned char g = row[static_cast<size_t>(x) * 3 + 1];
            const unsigned char b = row[static_cast<size_t>(x) * 3 + 2];
            if (r >= kPaper && g >= kPaper && b >= kPaper) continue;
            if (x < minX) minX = x;
            if (x > maxX) maxX = x;
            if (y < minY) minY = y;
            if (y > maxY) maxY = y;
        }
    }
    if (maxX < minX || maxY < minY) return false;
    const int w = maxX - minX + 1;
    const int h = maxY - minY + 1;
    if (w < 4 || h < 4) return false;
    out->x0 = minX;
    out->y0 = minY;
    out->w = w;
    out->h = h;
    const double frac = static_cast<double>(w) * static_cast<double>(h)
        / (static_cast<double>(cols) * static_cast<double>(rows));
    out->cropped = frac < 0.97;
    return true;
}

/// findPhotoPixelBox reads every row of the TIFF. Remember the result per file
/// version so option changes and re-opened studios do not scan it again.
bool cachedPhotoPixelBox(const QString& path, GDALDataset* ds, PhotoBox* out)
{
    struct Scan {
        bool ok = false;
        PhotoBox box;
    };
    static QMutex mutex;
    static QHash<QString, Scan> cache;
    const QFileInfo info(path);
    const QString key = info.exists()
        ? QStringLiteral("%1|%2|%3").arg(info.absoluteFilePath()).arg(info.size())
              .arg(info.lastModified().toMSecsSinceEpoch())
        : QString();
    if (!key.isEmpty()) {
        QMutexLocker lock(&mutex);
        const auto it = cache.constFind(key);
        if (it != cache.cend()) {
            if (it->ok && out) *out = it->box;
            return it->ok;
        }
    }
    Scan scan;
    scan.ok = findPhotoPixelBox(ds, &scan.box);
    if (!key.isEmpty()) {
        QMutexLocker lock(&mutex);
        if (cache.size() > 64) cache.clear();
        cache.insert(key, scan);
    }
    if (scan.ok && out) *out = scan.box;
    return scan.ok;
}

void knockOutSectionPaper(QgsRasterLayer* rl)
{
    if (!rl || !rl->isValid() || rl->bandCount() < 3) return;
    QgsRasterRenderer* rend = rl->renderer();
    if (!rend) return;
    auto* trans = new QgsRasterTransparency();
    QVector<QgsRasterTransparency::TransparentThreeValuePixel> whites;
    whites.append(QgsRasterTransparency::TransparentThreeValuePixel(255, 255, 255, 0.0, 12, 12, 12));
    trans->setTransparentThreeValuePixelList(whites);
    rend->setRasterTransparency(trans);
}

void inspectRaster(QgsRasterLayer* rl, SectionPlane* out)
{
    out->src = rl;
    out->extent = rl->extent();
    out->flatten = false;
    out->worldPlacement = false;
    out->cropX0 = 0;
    out->cropY0 = 0;
    out->cropW = 0;
    out->cropH = 0;
    GDALAllRegister();
    GDALDataset* ds = static_cast<GDALDataset*>(
        GDALOpen(rl->source().toUtf8().constData(), GA_ReadOnly));
    if (!ds) return;
    double gt[6] = {};
    ds->GetGeoTransform(gt);
    const int cols = ds->GetRasterXSize();
    const int rows = ds->GetRasterYSize();
    PhotoBox photo;
    const bool hasPhoto = cachedPhotoPixelBox(rl->source(), ds, &photo);
    GDALClose(ds);
    if (cols <= 0 || rows <= 0) return;

    // 사진실측: 픽셀 가로=거리, 픽셀 세로=해발 축. 세계 XYZ가 도면 축이 아님.
    const double colStep = std::hypot(gt[1], gt[4]);
    const double rowStep = std::hypot(gt[2], gt[5]);
    if (!(colStep > 0.0) || !(rowStep > 0.0)) return;

    const bool rotated = std::abs(gt[2]) > 1e-8 || std::abs(gt[4]) > 1e-8;
    const double yTopLeft = gt[3];
    const double yBotLeft = gt[3] + static_cast<double>(rows) * gt[5];
    const bool elevFromFile = looksLikeOrthometricElev(yTopLeft, yBotLeft);

    int useX0 = 0;
    int useY0 = 0;
    int useW = cols;
    int useH = rows;
    if (hasPhoto && photo.cropped) {
        useX0 = photo.x0;
        useY0 = photo.y0;
        useW = photo.w;
        useH = photo.h;
        out->cropX0 = useX0;
        out->cropY0 = useY0;
        out->cropW = useW;
        out->cropH = useH;
    }

    out->lengthM = static_cast<double>(useW) * colStep;
    out->heightM = static_cast<double>(useH) * rowStep;

    if (!rotated && elevFromFile && yLooksLikeElevation(out->extent) && out->cropW == 0) {
        out->flatten = false;
        out->lengthM = out->extent.width();
        out->heightM = out->extent.height();
        out->elevBottom = out->extent.yMinimum();
        return;
    }

    if (elevFromFile && std::abs(gt[5]) > 0.0) {
        // 이미 단면 평면(Y=해발). 하단을 자른 만큼 해발 원점을 올린다.
        const double yStep = std::abs(gt[5]);
        const int padBottom = rows - useY0 - useH;
        out->heightM = static_cast<double>(useH) * yStep;
        out->elevBottom = std::min(yTopLeft, yBotLeft) + static_cast<double>(padBottom) * yStep;
    } else {
        // 지도에 눕힌 4×4. 픽셀 세로 길이가 벽 높이. 원점은 표고 보정.
        out->worldPlacement = true;
        out->elevBottom = 0.0;
        out->heightM = static_cast<double>(useH) * rowStep;
    }

    out->flatten = true;
    out->extent = QgsRectangle(0.0, out->elevBottom,
                               out->lengthM, out->elevBottom + out->heightM);
}

/// Sheets built before display layers were layout-owned registered them in the
/// project (and so in the saved .qgs with a temp path). Drop those; return files.
QStringList takeLegacyDisplayLayers(QgsProject* project)
{
    QStringList ids;
    QStringList paths;
    for (QgsMapLayer* l : project->mapLayers()) {
        if (!l || !l->customProperty(QLatin1String(kPropSectionDisplay)).toBool())
            continue;
        ids.append(l->id());
        const QString p = l->customProperty(QLatin1String(kPropSectionDisplayPath)).toString();
        if (!p.isEmpty())
            paths.append(p);
    }
    if (!ids.isEmpty())
        project->removeMapLayers(ids);
    return paths;
}

/// Derived display files of a section sheet (its layout owns the layers).
QStringList ownedDisplayPaths(QgsLayout* layout)
{
    QStringList paths;
    if (!layout) return paths;
    for (QgsRasterLayer* rl : layout->findChildren<QgsRasterLayer*>(QString(), Qt::FindDirectChildrenOnly)) {
        if (!rl->customProperty(QLatin1String(kPropSectionDisplay)).toBool()) continue;
        const QString p = rl->customProperty(QLatin1String(kPropSectionDisplayPath)).toString();
        if (!p.isEmpty()) paths.append(p);
    }
    return paths;
}

void removeStaleDisplayFiles(const QStringList& stale, const QSet<QString>& keep)
{
    for (const QString& p : stale) {
        if (keep.contains(p)) continue;
        if (p.startsWith(QLatin1String("/vsimem/")))
            VSIUnlink(p.toUtf8().constData());
        else
            QFile::remove(p);
    }
}

void sharpenSectionRaster(QgsRasterLayer* rl)
{
    if (!rl) return;
    // 지오레퍼런스 래스터에 setDpi 하지 않는다. DPI를 붙이면 QGIS가
    // 개요(피라미드)로 낮춰 읽어 단면 사진이 흐려진다.
    // 사진실측은 1:1 픽셀을 유지한다. Cubic은 기울어진 소스 GT를 섞는다.
    rl->setResamplingStage(Qgis::RasterResamplingStage::Provider);
    if (QgsRasterDataProvider* dp = rl->dataProvider()) {
        dp->enableProviderResampling(true);
        dp->setZoomedInResamplingMethod(Qgis::RasterResamplingMethod::Nearest);
        dp->setZoomedOutResamplingMethod(Qgis::RasterResamplingMethod::Nearest);
    }
}

constexpr const char* kPropSheetPlanes = "ka_section/planes";

/// Plane parameters per source raster, in map order, as compact JSON (a plain
/// string survives any project-file property writer). Enough to derive the
/// display raster again without scanning the photo.
QString planesToJson(const QVector<SectionPlane>& planes)
{
    QVariantList out;
    for (const SectionPlane& plane : planes) {
        QVariantMap m;
        m.insert(QStringLiteral("src"), plane.src ? plane.src->id() : QString());
        m.insert(QStringLiteral("flatten"), plane.flatten);
        m.insert(QStringLiteral("lengthM"), plane.lengthM);
        m.insert(QStringLiteral("heightM"), plane.heightM);
        m.insert(QStringLiteral("elevBottom"), plane.elevBottom);
        m.insert(QStringLiteral("cropX0"), plane.cropX0);
        m.insert(QStringLiteral("cropY0"), plane.cropY0);
        m.insert(QStringLiteral("cropW"), plane.cropW);
        m.insert(QStringLiteral("cropH"), plane.cropH);
        out.append(m);
    }
    return QString::fromUtf8(QJsonDocument(QJsonArray::fromVariantList(out)).toJson(QJsonDocument::Compact));
}

QVariantList planesFromProperty(const QVariant& property)
{
    return QJsonDocument::fromJson(property.toString().toUtf8()).array().toVariantList();
}

QgsVectorLayer* addBlankLayer(QgsLayout* layout, const QgsCoordinateReferenceSystem& crs)
{
    // An empty layer set would draw every project layer (WMS) in the frame.
    const QString auth = crs.isValid() ? crs.authid() : QStringLiteral("EPSG:5187");
    auto* blank = new QgsVectorLayer(QStringLiteral("Polygon?crs=%1").arg(auth),
                                     QStringLiteral("ka_section_blank"), QStringLiteral("memory"));
    blank->setParent(layout);
    return blank;
}

/// Temp file stem for one source file version + plane. The same inputs reuse
/// the earlier derived file instead of copying the whole TIFF again.
QString derivedStem(const QgsRasterLayer* src, const SectionPlane& plane, const QString& wkt)
{
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    const QFileInfo info(src->source());
    stream << info.absoluteFilePath() << info.size() << info.lastModified().toMSecsSinceEpoch()
           << plane.cropX0 << plane.cropY0 << plane.cropW << plane.cropH
           << plane.lengthM << plane.heightM << plane.elevBottom << wkt;
    const QByteArray digest = QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex().left(16);
    return QDir::temp().filePath(QStringLiteral("ka_section_%1").arg(QString::fromLatin1(digest)));
}

/// Display layers belong to the section sheet layout, not to the project, so
/// a saved survey never references a temp file. They die with the layout.
QgsRasterLayer* adoptDisplayLayer(QgsRasterLayer* layer, const QString& path,
                                  const QgsCoordinateReferenceSystem& planeCrs, QObject* owner)
{
    layer->setCustomProperty(QLatin1String(kPropSectionDisplay), true);
    layer->setCustomProperty(QLatin1String(kPropSectionDisplayPath), path);
    if (planeCrs.isValid())
        layer->setCrs(planeCrs);
    sharpenSectionRaster(layer);
    knockOutSectionPaper(layer);
    layer->setParent(owner);
    return layer;
}

QgsRasterLayer* makeNorthUpDisplay(QgsRasterLayer* src,
                                   const QgsCoordinateReferenceSystem& planeCrs,
                                   const SectionPlane& plane, QObject* owner)
{
    const QString stem = derivedStem(src, plane, planeCrs.isValid() ? planeCrs.toWkt() : QString());
    for (const QString& reuse : {stem + QStringLiteral(".tif"), stem + QStringLiteral(".vrt")}) {
        if (!QFile::exists(reuse)) continue;
        auto* layer = new QgsRasterLayer(reuse, src->name(), QStringLiteral("gdal"));
        if (layer->isValid())
            return adoptDisplayLayer(layer, reuse, planeCrs, owner);
        delete layer;
        QFile::remove(reuse);
    }
    GDALAllRegister();
    GDALDataset* ds = static_cast<GDALDataset*>(
        GDALOpen(src->source().toUtf8().constData(), GA_ReadOnly));
    if (!ds) return nullptr;
    const int cols = ds->GetRasterXSize();
    const int rows = ds->GetRasterYSize();
    const int nBands = ds->GetRasterCount();
    if (cols <= 0 || rows <= 0 || nBands <= 0) {
        GDALClose(ds);
        return nullptr;
    }
    const int srcX = plane.cropW > 0 ? plane.cropX0 : 0;
    const int srcY = plane.cropH > 0 ? plane.cropY0 : 0;
    const int srcW = plane.cropW > 0 ? plane.cropW : cols;
    const int srcH = plane.cropH > 0 ? plane.cropH : rows;
    if (srcW <= 0 || srcH <= 0 || srcX < 0 || srcY < 0
        || srcX + srcW > cols || srcY + srcH > rows) {
        GDALClose(ds);
        return nullptr;
    }
    const double px = plane.lengthM / static_cast<double>(srcW);
    const double py = plane.heightM / static_cast<double>(srcH);
    const double elevTop = plane.elevBottom + plane.heightM;
    double ngt[6] = {0.0, px, 0.0, elevTop, 0.0, -py};
    const bool cropped = plane.cropW > 0;

    QString outPath = stem + QStringLiteral(".tif");
    // Write under a part name and rename, so an interrupted copy is never reused.
    const QString partPath = stem + QStringLiteral(".part.tif");
    QFile::remove(partPath);

    QString wkt = planeCrs.isValid() ? planeCrs.toWkt() : QString();
    GDALDriver* drv = GetGDALDriverManager()->GetDriverByName("GTiff");
    GDALDataset* copy = nullptr;
    if (drv && !cropped) {
        char** copts = CSLSetNameValue(nullptr, "TILED", "YES");
        copy = drv->CreateCopy(QDir::fromNativeSeparators(partPath).toUtf8().constData(),
                               ds, FALSE, copts, nullptr, nullptr);
        CSLDestroy(copts);
    }
    if (copy) {
        copy->SetGeoTransform(ngt);
        if (!wkt.isEmpty())
            copy->SetProjection(wkt.toUtf8().constData());
        GDALClose(copy);
        GDALClose(ds);
        QFile::remove(outPath);
        if (!QFile::rename(partPath, outPath))
            outPath = partPath;
    } else {
        wkt.replace(QLatin1Char('&'), QLatin1String("&amp;"));
        wkt.replace(QLatin1Char('<'), QLatin1String("&lt;"));
        QString xml;
        QTextStream ts(&xml);
        ts << QStringLiteral("<VRTDataset rasterXSize=\"%1\" rasterYSize=\"%2\">\n")
              .arg(srcW).arg(srcH);
        if (!wkt.isEmpty())
            ts << QStringLiteral("  <SRS>") << wkt << QStringLiteral("</SRS>\n");
        ts << QStringLiteral("  <GeoTransform>0, %1, 0, %2, 0, %3</GeoTransform>\n")
              .arg(px, 0, 'g', 17).arg(elevTop, 0, 'g', 17).arg(-py, 0, 'g', 17);
        for (int b = 1; b <= nBands; ++b) {
            GDALRasterBand* band = ds->GetRasterBand(b);
            const char* dt = GDALGetDataTypeName(band->GetRasterDataType());
            ts << QStringLiteral("  <VRTRasterBand dataType=\"%1\" band=\"%2\">\n")
                  .arg(QLatin1String(dt)).arg(b);
            const GDALColorInterp ci = band->GetColorInterpretation();
            if (ci != GCI_Undefined && ci != GCI_PaletteIndex) {
                ts << QStringLiteral("    <ColorInterp>%1</ColorInterp>\n")
                      .arg(QLatin1String(GDALGetColorInterpretationName(ci)));
            }
            int blockX = 0;
            int blockY = 0;
            band->GetBlockSize(&blockX, &blockY);
            ts << QStringLiteral("    <SimpleSource>\n")
               << QStringLiteral("      <SourceFilename relativeToVRT=\"0\">")
               << xmlEscape(src->source())
               << QStringLiteral("</SourceFilename>\n")
               << QStringLiteral("      <SourceBand>%1</SourceBand>\n").arg(b)
               << QStringLiteral("      <SourceProperties RasterXSize=\"%1\" RasterYSize=\"%2\" "
                                 "DataType=\"%3\" BlockXSize=\"%4\" BlockYSize=\"%5\"/>\n")
                    .arg(cols).arg(rows).arg(QLatin1String(dt))
                    .arg(blockX > 0 ? blockX : cols).arg(blockY > 0 ? blockY : 1)
               << QStringLiteral("      <SrcRect xOff=\"%1\" yOff=\"%2\" xSize=\"%3\" ySize=\"%4\"/>\n")
                    .arg(srcX).arg(srcY).arg(srcW).arg(srcH)
               << QStringLiteral("      <DstRect xOff=\"0\" yOff=\"0\" xSize=\"%1\" ySize=\"%2\"/>\n")
                    .arg(srcW).arg(srcH)
               << QStringLiteral("    </SimpleSource>\n")
               << QStringLiteral("  </VRTRasterBand>\n");
        }
        ts << QStringLiteral("</VRTDataset>\n");
        GDALClose(ds);
        QFile::remove(partPath);
        const QString vrtPath = stem + QStringLiteral(".vrt");
        QFile f(vrtPath);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return nullptr;
        f.write(xml.toUtf8());
        f.close();
        outPath = vrtPath;
    }

    auto* layer = new QgsRasterLayer(outPath, src->name(), QStringLiteral("gdal"));
    if (!layer->isValid()) {
        delete layer;
        QFile::remove(outPath);
        return nullptr;
    }
    return adoptDisplayLayer(layer, outPath, planeCrs, owner);
}

} // namespace

// ---- Task 3: buildSectionLayout ----

SectionLayoutResult SectionLayoutService::buildSectionLayout(
    QgsProject* project,
    const QList<QgsMapLayer*>& layers,
    const SectionLayoutOptions& options)
{
    SectionLayoutResult result;

    if (!project) {
        result.errorKo = QStringLiteral("프로젝트가 없습니다.");
        return result;
    }

    // 1. 단면 평면(픽셀 가로=거리, 세로=표고)으로 범위 계산.
    //    지도 CRS geotransform의 회전/AABB는 쓰지 않는다.
    QgsRasterLayer* firstRaster = nullptr;
    QgsCoordinateReferenceSystem mapCrs;
    QgsRectangle combinedExtent;
    QVector<SectionPlane> planes;
    bool anyFlatten = false;

    for (QgsMapLayer* ml : layers) {
        auto* rl = qobject_cast<QgsRasterLayer*>(ml);
        if (!rl || !rl->isValid()) continue;
        if (rl->extent().isEmpty()) continue;

        SectionPlane plane;
        inspectRaster(rl, &plane);
        if (plane.extent.isEmpty() || plane.extent.width() <= 0.0
            || plane.extent.height() <= 0.0)
            continue;
        if (std::abs(options.elevationOffsetM) > 1e-12) {
            plane.elevBottom += options.elevationOffsetM;
            plane.flatten = true;
            if (plane.worldPlacement || plane.extent.xMinimum() <= 0.0) {
                plane.extent = QgsRectangle(0.0, plane.elevBottom,
                                            plane.lengthM, plane.elevBottom + plane.heightM);
            } else {
                plane.extent = QgsRectangle(plane.extent.xMinimum(), plane.elevBottom,
                                            plane.extent.xMaximum(),
                                            plane.elevBottom + plane.heightM);
            }
        }
        planes.append(plane);
        anyFlatten = anyFlatten || plane.flatten;
        if (!firstRaster) {
            firstRaster = rl;
            combinedExtent = plane.extent;
        } else {
            combinedExtent.combineExtentWith(plane.extent);
        }
    }

    const QString userCrsAuth = options.mapCrsAuthId.isEmpty()
        ? QStringLiteral("EPSG:5187") : options.mapCrsAuthId;
    QString titleCrsId = options.mapCrsAuthId;
    if (titleCrsId.isEmpty()) {
        if (firstRaster && firstRaster->crs().isValid()
            && !firstRaster->crs().authid().isEmpty())
            titleCrsId = firstRaster->crs().authid();
        else
            titleCrsId = QStringLiteral("EPSG:5187");
    }
    if (!firstRaster) {
        mapCrs = QgsCoordinateReferenceSystem(userCrsAuth);
        if (!mapCrs.isValid()) {
            result.errorKo = QStringLiteral("좌표계를 인식하지 못했습니다.");
            return result;
        }
        combinedExtent = QgsRectangle(0.0, 0.0, 10.0, 2.0);
    } else if (!options.mapCrsAuthId.isEmpty() || anyFlatten) {
        // 표시 래스터는 이미 거리×해발(회전 없음). 5186/5187은 표제·map CRS 라벨용.
        mapCrs = QgsCoordinateReferenceSystem(userCrsAuth);
        if (!mapCrs.isValid())
            mapCrs = firstRaster->crs();
    } else {
        mapCrs = firstRaster->crs();
    }
    if (combinedExtent.isEmpty()
        || !std::isfinite(combinedExtent.width())
        || !std::isfinite(combinedExtent.height())
        || combinedExtent.width() <= 0.0
        || combinedExtent.height() <= 0.0) {
        result.errorKo = QStringLiteral("레이어 범위가 유효하지 않습니다.");
        return result;
    }

    // 2. 용지 치수 (mm, 가로)
    const QSizeF paperMm = paperSizeMm(options.paper);
    const double W = paperMm.width();
    const double H = paperMm.height();

    // 3. 레이아웃 구역 (mm)
    const double leftAxisW   = 22.0;  // 표고 축 영역
    const double topM        = 10.0;  // 상단 여백
    const double rightM      = 8.0;
    const double bottomAxisH = 14.0;  // 거리 축 영역
    const double infoH       = 20.0;  // 표제 + 교호식 축척자 한 줄 (A4 하단 여유)

    const double mapW_avail = W - leftAxisW - rightM;
    const double mapH_avail = H - topM - bottomAxisH - infoH;

    // 4. 축척 및 지도 프레임 크기 결정
    //    combinedExtent는 mapCrs 기준 미터 단위
    const double extW = combinedExtent.width();
    const double extH = combinedExtent.height();

    // 용지 가용 영역에 꽉 채우는 최소 분모 (map units m → mm: * 1000)
    const double scaleFromW = extW * 1000.0 / mapW_avail;
    const double scaleFromH = extH * 1000.0 / mapH_avail;
    const double autoScale  = std::max(scaleFromW, scaleFromH);

    // 사용자 지정 분모 또는 자동; 용지에 안 맞으면 자동 최솟값으로 올린다.
    // 조용히 바꾸지 않는다: 안내와 용지에 들어가는 표준 축척을 함께 돌려준다.
    double scaleDenom = (options.scaleDenominator > 0.0)
        ? options.scaleDenominator : autoScale;
    const bool requestedTooLarge = options.scaleDenominator > 0.0
        && options.scaleDenominator < autoScale * (1.0 - 1e-9);
    if (scaleDenom < autoScale) scaleDenom = autoScale;
    if (firstRaster) {
        const int standardFit = StandardScales::snapUp(
            autoScale, StandardScales::Section | StandardScales::Snap);
        if (!StandardScales::isStandard(std::round(scaleDenom * 1000.0) / 1000.0))
            result.suggestedScaleDenominator = standardFit;
        if (requestedTooLarge) {
            const QString paperName = options.paper == SectionLayoutOptions::Paper::A4 ? QStringLiteral("A4")
                : options.paper == SectionLayoutOptions::Paper::A2 ? QStringLiteral("A2")
                : options.paper == SectionLayoutOptions::Paper::A1 ? QStringLiteral("A1")
                                                                    : QStringLiteral("A3");
            result.warningKo = QStringLiteral(
                "지정한 축척(%1)은 %2 용지에 들어가지 않아 용지에 맞춘 축척(%3)으로 그렸습니다. "
                "표준 축척(%4)을 고르거나 더 큰 용지를 쓰세요.")
                .arg(StandardScales::label(options.scaleDenominator), paperName,
                     StandardScales::label(scaleDenom), StandardScales::label(standardFit));
        }
    }

    // 지도 프레임 크기: extent와 같은 종횡비 → zoomToExtent가 정확히 combinedExtent를 렌더링
    const double frameW = extW * 1000.0 / scaleDenom;
    const double frameH = extH * 1000.0 / scaleDenom;

    // 가용 영역 내 중앙 배치
    const double mapX = leftAxisW + (mapW_avail - frameW) * 0.5;
    const double mapY = topM      + (mapH_avail - frameH) * 0.5;

    // 5. 눈금 사전 계산 (레이아웃 생성 전 검증 — 실패 시 기존 조판 유지)
    const double xMin = combinedExtent.xMinimum();
    const double xMax = combinedExtent.xMaximum();
    const double yMin = combinedExtent.yMinimum();
    const double yMax = combinedExtent.yMaximum();

    const double elevInterval = options.elevationIntervalM > 0.0
        ? options.elevationIntervalM : 0.10;
    const double elevMin = yMin;
    const double elevMax = yMax;

    const auto elevResult = axisTicks(elevMin, elevMax, elevInterval);
    if (!elevResult.error.isEmpty()) {
        result.errorKo = elevResult.error;
        return result;
    }

    const double distSpan = xMax - xMin;
    const double distInterval = (options.manualDistanceIntervalM > 0.0)
        ? options.manualDistanceIntervalM
        : niceDistanceInterval(distSpan);

    const auto distResult = axisTicks(0.0, distSpan, distInterval);
    if (!distResult.error.isEmpty()) {
        result.errorKo = distResult.error;
        return result;
    }

    const double elevRange = (elevMax > elevMin) ? (elevMax - elevMin) : 1.0;

    // 6. 기존 조판 제거 후 새 조판 생성 (검증 통과 이후에만 삭제)
    static const QString kName = QStringLiteral("section_sheet");
    QStringList staleDisplayFiles;
    if (auto* old = project->layoutManager()->layoutByName(kName)) {
        staleDisplayFiles += ownedDisplayPaths(dynamic_cast<QgsLayout*>(old));
        project->layoutManager()->removeLayout(old);
    }

    auto* layout = new QgsPrintLayout(project);
    layout->setName(kName);
    layout->initializeDefaults();

    if (layout->pageCollection() && layout->pageCollection()->pageCount() > 0) {
        layout->pageCollection()->page(0)
            ->setPageSize(QgsLayoutSize(W, H, Qgis::LayoutUnit::Millimeters));
    }
    project->layoutManager()->addLayout(layout);
    KaPdfExport::prepareLayout(layout);

    staleDisplayFiles += takeLegacyDisplayLayers(project);
    QSet<QString> liveDisplayFiles;
    QList<QgsMapLayer*> mapLayers;
    for (const SectionPlane& plane : planes) {
        if (plane.flatten) {
            QgsRasterLayer* display = makeNorthUpDisplay(
                plane.src, mapCrs, plane, layout);
            if (!display) {
                result.errorKo =
                    QStringLiteral("단면 GeoTIFF를 거리×표고 평면으로 펼치지 못했습니다.");
                return result;
            }
            liveDisplayFiles.insert(display->customProperty(QLatin1String(kPropSectionDisplayPath)).toString());
            mapLayers.append(display);
        } else {
            sharpenSectionRaster(plane.src);
            knockOutSectionPaper(plane.src);
            mapLayers.append(plane.src);
        }
    }
    if (mapLayers.isEmpty())
        mapLayers.append(addBlankLayer(layout, mapCrs));
    removeStaleDisplayFiles(staleDisplayFiles, liveDisplayFiles);
    // Display rasters are not saved with the survey; keep what is needed to
    // derive them again after the survey is reopened (restoreDisplayLayers).
    layout->setCustomProperty(QLatin1String(kPropSheetPlanes), planesToJson(planes));

    // 7. 지도 항목 (ka_section_map): extent 종횡비와 동일한 프레임 크기
    auto* map = new QgsLayoutItemMap(layout);
    map->setId(QStringLiteral("ka_section_map"));
    map->attemptSetSceneRect(QRectF(mapX, mapY, frameW, frameH));
    map->setFrameEnabled(true);
    map->setFrameStrokeWidth(QgsLayoutMeasurement(0.3, Qgis::LayoutUnit::Millimeters));
    map->setMapRotation(0.0);
    layout->addLayoutItem(map);
    map->setCrs(mapCrs);
    map->setLayers(mapLayers);
    map->setKeepLayerSet(true);
    map->zoomToExtent(combinedExtent);

    result.appliedScaleDenominator = map->scale();
    result.appliedExtent = map->extent();

    // 선 심볼 생성 헬퍼 (회색 얇은 선)
    auto makeGraySym = [](double widthMm) -> std::unique_ptr<QgsLineSymbol> {
        return QgsLineSymbol::createSimple({
            {QStringLiteral("line_color"),      QStringLiteral("#1F2937")},
            {QStringLiteral("line_width"),      QString::number(widthMm)},
            {QStringLiteral("line_width_unit"), QStringLiteral("MM")}
        });
    };

    // 8. 표고 축선 (지도 좌측 경계, 수직)
    {
        QPolygonF ln;
        ln << QPointF(mapX, mapY) << QPointF(mapX, mapY + frameH);
        auto* item = new QgsLayoutItemPolyline(ln, layout);
        item->setId(QStringLiteral("ka_section_elevation_axis"));
        item->setStartMarker(QgsLayoutItemPolyline::NoMarker);
        item->setEndMarker(QgsLayoutItemPolyline::NoMarker);
        if (auto sym = makeGraySym(0.15)) item->setSymbol(sym.get());
        layout->addLayoutItem(item);
    }

    // 8a. 표고 눈금선 + 레이블
    for (int i = 0; i < elevResult.ticks.size(); ++i) {
        const double tickVal = elevResult.ticks[i];
        const double fracY   = (tickVal - elevMin) / elevRange;
        const double layoutY = mapY + frameH * (1.0 - fracY); // 레이아웃 Y: 아래로 증가

        QPolygonF tick;
        tick << QPointF(mapX - 3.0, layoutY) << QPointF(mapX, layoutY);
        auto* tickItem = new QgsLayoutItemPolyline(tick, layout);
        tickItem->setId(QStringLiteral("ka_section_elevation_tick_%1").arg(i));
        tickItem->setStartMarker(QgsLayoutItemPolyline::NoMarker);
        tickItem->setEndMarker(QgsLayoutItemPolyline::NoMarker);
        if (auto sym = makeGraySym(0.10)) tickItem->setSymbol(sym.get());
        layout->addLayoutItem(tickItem);

        QPolygonF grid;
        grid << QPointF(mapX, layoutY) << QPointF(mapX + frameW, layoutY);
        auto* gridItem = new QgsLayoutItemPolyline(grid, layout);
        gridItem->setId(QStringLiteral("ka_section_elevation_grid_%1").arg(i));
        gridItem->setStartMarker(QgsLayoutItemPolyline::NoMarker);
        gridItem->setEndMarker(QgsLayoutItemPolyline::NoMarker);
        if (auto sym = QgsLineSymbol::createSimple({
                {QStringLiteral("line_color"),      QStringLiteral("#374151")},
                {QStringLiteral("line_width"),      QStringLiteral("0.12")},
                {QStringLiteral("line_width_unit"), QStringLiteral("MM")}}))
            gridItem->setSymbol(sym.get());
        layout->addLayoutItem(gridItem);

        auto* lbl = new QgsLayoutItemLabel(layout);
        lbl->setId(QStringLiteral("ka_section_elevation_%1").arg(i));
        lbl->setText(SectionSheetDecor::elevationText(tickVal, options));
        lbl->setCustomProperty(SectionSheetDecor::tickKindKey(), QStringLiteral("elevation"));
        lbl->setCustomProperty(SectionSheetDecor::tickValueKey(), tickVal);
        lbl->setHAlign(Qt::AlignRight);
        lbl->setVAlign(Qt::AlignVCenter);
        lbl->attemptSetSceneRect(QRectF(0.5, layoutY - 2.5, mapX - 4.0, 5.0));
        SectionSheetDecor::setLabelFont(lbl, SectionSheetDecor::tickFont(options));
        layout->addLayoutItem(lbl);
    }

    // 9. 거리 축선 (지도 하단 경계, 수평)
    {
        QPolygonF ln;
        ln << QPointF(mapX, mapY + frameH) << QPointF(mapX + frameW, mapY + frameH);
        auto* item = new QgsLayoutItemPolyline(ln, layout);
        item->setId(QStringLiteral("ka_section_distance_axis"));
        item->setStartMarker(QgsLayoutItemPolyline::NoMarker);
        item->setEndMarker(QgsLayoutItemPolyline::NoMarker);
        if (auto sym = makeGraySym(0.15)) item->setSymbol(sym.get());
        layout->addLayoutItem(item);
    }

    // 9a. 거리 눈금선 + 레이블 (왼쪽 0.00m부터 상대 누적거리)
    for (int i = 0; i < distResult.ticks.size(); ++i) {
        const double relDist = distResult.ticks[i];
        const double fracX   = (distSpan > 0.0) ? (relDist / distSpan) : 0.0;
        const double layoutX = mapX + frameW * fracX;
        const double axisY   = mapY + frameH;

        QPolygonF tick;
        tick << QPointF(layoutX, axisY) << QPointF(layoutX, axisY + 3.0);
        auto* tickItem = new QgsLayoutItemPolyline(tick, layout);
        tickItem->setId(QStringLiteral("ka_section_distance_tick_%1").arg(i));
        tickItem->setStartMarker(QgsLayoutItemPolyline::NoMarker);
        tickItem->setEndMarker(QgsLayoutItemPolyline::NoMarker);
        if (auto sym = makeGraySym(0.10)) tickItem->setSymbol(sym.get());
        layout->addLayoutItem(tickItem);

        auto* lbl = new QgsLayoutItemLabel(layout);
        lbl->setId(QStringLiteral("ka_section_distance_%1").arg(i));
        lbl->setText(SectionSheetDecor::distanceText(relDist));
        lbl->setCustomProperty(SectionSheetDecor::tickKindKey(), QStringLiteral("distance"));
        lbl->setCustomProperty(SectionSheetDecor::tickValueKey(), relDist);
        lbl->setHAlign(Qt::AlignHCenter);
        lbl->setVAlign(Qt::AlignTop);
        lbl->attemptSetSceneRect(QRectF(layoutX - 10.0, axisY + 3.0, 20.0, 5.0));
        SectionSheetDecor::setLabelFont(lbl, SectionSheetDecor::tickFont(options));
        layout->addLayoutItem(lbl);
    }

    // 10. 기준선: #D7191C 점선 0.20mm (지도 하단 = 기준 표고)
    const QRectF mapRect(mapX, mapY, frameW, frameH);
    if (options.showReferenceLine)
        SectionSheetDecor::addReferenceLine(layout, mapRect, options);
    // 선택 주기(예: A–A′ 단면 · 북벽). 비어 있으면 넣지 않는다.
    if (!options.noteText.trimmed().isEmpty())
        SectionSheetDecor::addNote(layout, mapRect, options.noteText.trimmed());

    // 11. 크롬: 거리 눈금 바로 아래 왼쪽 축척자 하나. 샘플 스타일은 용지에 두지 않는다.
    const double chromeY = mapY + frameH + bottomAxisH;

    const int segs = 4;
    const double denom = (map->scale() > 0.0) ? map->scale() : scaleDenom;
    const double segM = LayoutService::niceScaleBarSegmentMeters(frameW, denom, segs);
    double barW = LayoutService::scaleBarWidthMm(segM, segs, denom);
    barW = std::clamp(barW, 40.0, std::min(160.0, std::max(40.0, frameW)));
    const double barY = chromeY + 0.4;
    const double barH = 11.5;
    const QString barStyle = options.scaleBarStyle.isEmpty()
        ? QStringLiteral("Double Box") : options.scaleBarStyle;

    {
        auto* sb = new QgsLayoutItemScaleBar(layout);
        sb->setId(QStringLiteral("ka_section_scale_bar"));
        layout->addLayoutItem(sb);
        sb->setLinkedMap(map);
        sb->setStyle(barStyle);
        sb->setUnits(Qgis::DistanceUnit::Meters);
        sb->setUnitLabel(QStringLiteral("m"));
        sb->setNumberOfSegments(segs);
        sb->setNumberOfSegmentsLeft(0);
        sb->setHeight(2.4);
        sb->setLabelBarSpace(1.0);
        sb->setBoxContentSpace(0.6);
        sb->setLabelVerticalPlacement(
            Qgis::ScaleBarDistanceLabelVerticalPlacement::AboveSegment);
        sb->setSegmentSizeMode(Qgis::ScaleBarSegmentSizeMode::Fixed);
        if (segM > 0.0)
            sb->setUnitsPerSegment(segM);
        QgsTextFormat sbFmt;
        sbFmt.setFont(QFont(QStringLiteral("Malgun Gothic")));
        sbFmt.setSize(7.0);
        sbFmt.setSizeUnit(Qgis::RenderUnit::Points);
        sb->setTextFormat(sbFmt);
        LayoutService::applySheetScaleBarInk(sb);
        sb->attemptSetSceneRect(QRectF(mapX, barY, barW, barH));
        SectionSheetDecor::markAutoPosition(sb);
    }

    {
        const long long sd = result.appliedScaleDenominator > 0.5
            ? static_cast<long long>(std::lround(result.appliedScaleDenominator)) : 0LL;
        auto* lbl = new QgsLayoutItemLabel(layout);
        lbl->setId(QStringLiteral("ka_section_scale"));
        lbl->setText(sd > 0
            ? QStringLiteral("S = 1:%1").arg(QLocale().toString(sd))
            : QStringLiteral("S = 1:N"));
        lbl->setHAlign(Qt::AlignLeft);
        lbl->setVAlign(Qt::AlignVCenter);
        lbl->attemptSetSceneRect(QRectF(mapX + barW + 4.0, barY + 1.5, 40.0, 9.0));
        QFont f(QStringLiteral("Malgun Gothic"));
        f.setPointSize(7);
        SectionSheetDecor::setLabelFont(lbl, f);
        layout->addLayoutItem(lbl);
        SectionSheetDecor::markAutoPosition(lbl);
    }

    {
        // 좌표계 번호에 한글 원점명을 붙인다(예: EPSG:5187 · 동부원점(GRS80)).
        auto* lbl = new QgsLayoutItemLabel(layout);
        lbl->setId(QStringLiteral("ka_section_crs"));
        lbl->setText(SectionSheetDecor::crsText(titleCrsId));
        lbl->setHAlign(Qt::AlignLeft);
        lbl->setVAlign(Qt::AlignVCenter);
        lbl->attemptSetSceneRect(QRectF(mapX + barW + 46.0, barY + 1.5, 70.0, 9.0));
        QFont f(QStringLiteral("Malgun Gothic"));
        f.setPointSize(7);
        SectionSheetDecor::setLabelFont(lbl, f);
        layout->addLayoutItem(lbl);
        SectionSheetDecor::markAutoPosition(lbl);
    }

    {
        auto* lbl = new QgsLayoutItemLabel(layout);
        lbl->setId(QStringLiteral("ka_section_title_block"));
        lbl->setText(SectionSheetDecor::titleText(options));
        lbl->setHAlign(Qt::AlignLeft);
        lbl->setVAlign(Qt::AlignVCenter);
        lbl->attemptSetSceneRect(QRectF(mapX, chromeY + 13.2, frameW, 6.0));
        QFont f(QStringLiteral("Malgun Gothic"));
        f.setPointSize(7);
        f.setBold(true);
        SectionSheetDecor::setLabelFont(lbl, f);
        layout->addLayoutItem(lbl);
        SectionSheetDecor::markAutoPosition(lbl);
    }

    result.layoutName = kName;
    return result;
}

// ---- restoreDisplayLayers: re-derive layout-owned display rasters ----

bool SectionLayoutService::restoreDisplayLayers(QgsProject* project)
{
    if (!project) return false;
    auto* layout = dynamic_cast<QgsPrintLayout*>(
        project->layoutManager()->layoutByName(QStringLiteral("section_sheet")));
    auto* map = layout ? qobject_cast<QgsLayoutItemMap*>(
        layout->itemById(QStringLiteral("ka_section_map"))) : nullptr;
    if (!map || !layout->customProperty(QLatin1String(kPropSheetPlanes)).isValid())
        return false;
    const QVariantList stored = planesFromProperty(layout->customProperty(QLatin1String(kPropSheetPlanes)));
    const QList<QgsMapLayer*> current = map->layers();
    const bool complete = !current.isEmpty() && current.size() == std::max<qsizetype>(1, stored.size())
        && std::all_of(current.cbegin(), current.cend(),
                       [](const QgsMapLayer* l) { return l && l->isValid(); });
    if (complete) return false;
    QList<QgsMapLayer*> layers;
    for (const QVariant& value : stored) {
        const QVariantMap m = value.toMap();
        auto* src = qobject_cast<QgsRasterLayer*>(
            project->mapLayer(m.value(QStringLiteral("src")).toString()));
        if (!src || !src->isValid())
            return false;  // the source is gone: leave the sheet as it is
        if (!m.value(QStringLiteral("flatten")).toBool()) {
            sharpenSectionRaster(src);
            knockOutSectionPaper(src);
            layers.append(src);
            continue;
        }
        SectionPlane plane;
        plane.src = src;
        plane.flatten = true;
        plane.lengthM = m.value(QStringLiteral("lengthM")).toDouble();
        plane.heightM = m.value(QStringLiteral("heightM")).toDouble();
        plane.elevBottom = m.value(QStringLiteral("elevBottom")).toDouble();
        plane.cropX0 = m.value(QStringLiteral("cropX0")).toInt();
        plane.cropY0 = m.value(QStringLiteral("cropY0")).toInt();
        plane.cropW = m.value(QStringLiteral("cropW")).toInt();
        plane.cropH = m.value(QStringLiteral("cropH")).toInt();
        QgsRasterLayer* display = makeNorthUpDisplay(src, map->crs(), plane, layout);
        if (!display) return false;
        layers.append(display);
    }
    if (layers.isEmpty())
        layers.append(addBlankLayer(layout, map->crs()));
    map->setLayers(layers);
    map->invalidateCache();
    return true;
}

// ---- Task 3: exportSectionPdf ----

QString SectionLayoutService::exportSectionPdf(
    QgsProject* project,
    const QString& pdfPath,
    QString* errorOut)
{
    if (!project) {
        if (errorOut) *errorOut = QStringLiteral("프로젝트가 없습니다.");
        return {};
    }

    static const QString kName = QStringLiteral("section_sheet");
    auto* master = project->layoutManager()->layoutByName(kName);
    auto* layout = dynamic_cast<QgsPrintLayout*>(master);
    if (!layout) {
        if (errorOut) *errorOut = QStringLiteral("section_sheet 도면이 없습니다.");
        return {};
    }

    // 다시 연 조사에서는 임시 표시 래스터가 없다. 저장된 평면값으로 다시 만든다.
    restoreDisplayLayers(project);
    // 래스터를 타일 없이 한 번에 그려 PDF에서 조각 없이 나오게 한다.
    // 설정은 도면 PDF(조판·제출)와 같은 KaPdfExport 한 곳에서 온다.
    KaPdfExport::prepareLayout(layout);

    QgsLayoutExporter exporter(layout);
    const QgsLayoutExporter::PdfExportSettings settings = KaPdfExport::sheetSettings();

    const double keepDpi = layout->renderContext().dpi();
    layout->renderContext().setDpi(settings.dpi);
    const auto r = exporter.exportToPdf(pdfPath, settings);
    layout->renderContext().setDpi(keepDpi);

    if (r != QgsLayoutExporter::Success) {
        if (errorOut)
            *errorOut = QStringLiteral("PDF 내보내기 실패 (코드 %1)").arg(int(r));
        return {};
    }
    if (!QFile::exists(pdfPath) || QFileInfo(pdfPath).size() < 100) {
        if (errorOut)
            *errorOut = QStringLiteral("PDF 파일이 비었거나 너무 작습니다.");
        return {};
    }
    return pdfPath;
}
