#include <algorithm>
#include <cmath>
#include <vector>

#include <QtTest>
#include <QColor>
#include <QGuiApplication>
#include <QFile>
#include <QImage>
#include <QSet>
#include <QTemporaryDir>

#include "core/Terrain3dService.h"

#include <gdal_priv.h>

class TestTerrain3d : public QObject {
  Q_OBJECT
private slots:
  void loadDem_readsRampExtentAndZ();
  void buildMesh_risesInZ();
  void composeExport_drawsScaleBarAndNorth();
  void renderPerspective_isNotFlatFill();
  void visibleWidth_shrinksWhenCloser();
  void northAzimuth_usesGeotransform();
  void pickLocalDem_prefersNamedAndSkipsAmbiguous();
  void pickTerrainDem_allowsToolbarCopernicus();
  void loadDemClip_readsWindowNotFullGrid();
  void renderPerspective_keepsTextureDetail();
  void googleSatUri_isHighResXyz();
  void composeSheet_hasLegendScaleCrs();
  void studioTab_isSeparateFromMapAndSection();
  void demWorldRect_coversRotatedCorners();
  void followup_tileNetworkAndMapStyleCards();
  void terrainSheet_usesOwnSheetNotUserSheet();
  void studio_buildsHighResFromCanvasView();
  void studio_hidesManualChrome();
  void terrain3dSheet_hasLegendNorthScale();
  void renderPerspective_paperWhiteAndBright();
  void renderPerspective_softShadeNoFacetBands();
  void terrain3dSheet_matches2dChrome();
  void orbit_pitchFollowsMouseDown();
  void layoutStudio_has2dChromeCards();
  void terrain3dSheet_legendOffUntilAsked();
  void studio_drapesVisibleMapOverlays();
  void layoutStudio_hasLayerTreeAndDeleteUndo();
  void terrain3dSheet_hidesDummyFill();
  void applyScale_usesPaperScaleNotBarOnly();
  void distanceForVisibleWidth_invertsVisibleWidth();
  void placeTerrain3d_detachesBeforeReplaceSheet();
};

namespace {
QString writeRampDem(const QString& path) {
  GDALAllRegister();
  GDALDriver* drv = GetGDALDriverManager()->GetDriverByName("GTiff");
  if (!drv) return QStringLiteral("GTiff missing");
  GDALDataset* ds = drv->Create(path.toUtf8().constData(), 8, 8, 1, GDT_Float32, nullptr);
  if (!ds) return QStringLiteral("create failed");
  double gt[6] = {200000.0, 10.0, 0, 450080.0, 0, -10.0};
  ds->SetGeoTransform(gt);
  std::vector<float> z(64);
  for (int y = 0; y < 8; ++y)
    for (int x = 0; x < 8; ++x)
      z[static_cast<size_t>(y) * 8 + x] = static_cast<float>(x * 4 + y);
  ds->GetRasterBand(1)->RasterIO(GF_Write, 0, 0, 8, 8, z.data(), 8, 8, GDT_Float32, 0, 0);
  GDALClose(ds);
  return {};
}
}  // namespace

void TestTerrain3d::loadDem_readsRampExtentAndZ() {
  QTemporaryDir tmp;
  QVERIFY(tmp.isValid());
  const QString dem = tmp.filePath(QStringLiteral("ramp.tif"));
  const QString werr = writeRampDem(dem);
  QVERIFY2(werr.isEmpty(), qPrintable(werr));
  Terrain3dService::DemScene sc;
  QString err;
  QVERIFY2(Terrain3dService::loadDem(dem, &sc, &err), qPrintable(err));
  QCOMPARE(sc.width, 8);
  QCOMPARE(sc.height, 8);
  QVERIFY2(sc.zMax > sc.zMin + 1.0f, "높낮이가 있어야 3D가 됨");
  QVERIFY2(sc.groundWidthM > 50.0 && sc.groundWidthM < 90.0, "8셀×10m");
}

void TestTerrain3d::buildMesh_risesInZ() {
  QTemporaryDir tmp;
  QVERIFY(tmp.isValid());
  const QString dem = tmp.filePath(QStringLiteral("ramp.tif"));
  QVERIFY(writeRampDem(dem).isEmpty());
  Terrain3dService::DemScene sc;
  QString err;
  QVERIFY(Terrain3dService::loadDem(dem, &sc, &err));
  const Terrain3dService::Mesh mesh = Terrain3dService::buildMesh(sc, 16, 2.0f);
  QVERIFY2(mesh.positions.size() >= 8 * 3, "정점이 있어야 함");
  QVERIFY2(mesh.indices.size() >= 6, "삼각형이 있어야 함");
  float zMin = mesh.positions[2];
  float zMax = mesh.positions[2];
  for (int i = 2; i < mesh.positions.size(); i += 3) {
    zMin = std::min(zMin, mesh.positions[i]);
    zMax = std::max(zMax, mesh.positions[i]);
  }
  QVERIFY2(zMax > zMin + 0.5f, "메시 Z가 솟아야 입체임");
}

void TestTerrain3d::composeExport_drawsScaleBarAndNorth() {
  QImage view(320, 200, QImage::Format_RGB32);
  view.fill(QColor(80, 120, 90));
  const QImage out = Terrain3dService::composeExport(view, 80.0, 25.0);
  QVERIFY(out.width() >= view.width());
  QVERIFY(out.height() > view.height());
  QVERIFY2(out.width() * out.height() > view.width() * view.height(),
           "축척자·방위 띠가 이미지에 붙어야 함");
  int dark = 0;
  for (int y = out.height() - 28; y < out.height(); ++y) {
    for (int x = 8; x < 160; ++x) {
      const QRgb p = out.pixel(x, y);
      if (qGray(p) < 80)
        ++dark;
    }
  }
  QVERIFY2(dark > 20, "아래쪽에 축척자 먹선이 있어야 함");
}

void TestTerrain3d::renderPerspective_isNotFlatFill() {
  QTemporaryDir tmp;
  QVERIFY(tmp.isValid());
  const QString dem = tmp.filePath(QStringLiteral("ramp.tif"));
  QVERIFY(writeRampDem(dem).isEmpty());
  Terrain3dService::DemScene sc;
  QVERIFY(Terrain3dService::loadDem(dem, &sc, nullptr));
  const Terrain3dService::Mesh mesh = Terrain3dService::buildMesh(sc, 16, 2.0f);
  const QImage tex = Terrain3dService::hillshadeTexture(sc);
  const QImage img = Terrain3dService::renderPerspective(mesh, tex, 220, 160, 35.0f, 42.0f, 90.0f);
  QSet<QRgb> cols;
  int notBg = 0;
  for (int y = 8; y < img.height() - 8; y += 3) {
    for (int x = 8; x < img.width() - 8; x += 3) {
      const QRgb p = img.pixel(x, y);
      cols.insert(p);
      if (qGray(p) > 40)
        ++notBg;
    }
  }
  QVERIFY2(cols.size() > 8, "높이 메시가 단색 평면이면 안 됨");
  QVERIFY2(notBg > 20, "3D 지형이 화면에 보여야 함");
}

void TestTerrain3d::visibleWidth_shrinksWhenCloser() {
  const double farW = Terrain3dService::visibleWidthAtTarget(200.0f, 1280, 800);
  const double nearW = Terrain3dService::visibleWidthAtTarget(80.0f, 1280, 800);
  QVERIFY2(farW > nearW * 1.5, "줌인하면 축척자가 더 짧은 거리를 나타내야 함");
  QVERIFY2(nearW > 10.0 && farW < 2000.0, "투시 가시 폭이 미터여야 함");
  QVERIFY2(Terrain3dService::scaleBarSegmentM(farW) > Terrain3dService::scaleBarSegmentM(nearW),
           "먼 화면의 눈금(m)이 더 커야 함");
}

void TestTerrain3d::northAzimuth_usesGeotransform() {
  const double northUp[6] = {0, 10, 0, 0, 0, -10};
  QVERIFY2(std::abs(Terrain3dService::northAzimuthDeg(northUp)) < 1.0, "정북 DEM은 N=0");
  const double eastIsUp[6] = {0, 0, -10, 0, 10, 0};
  const double az = Terrain3dService::northAzimuthDeg(eastIsUp);
  QVERIFY2(std::abs(az - 90.0) < 2.0 || std::abs(az + 270.0) < 2.0, "격자 북이 +X면 약 90도");
}

void TestTerrain3d::pickLocalDem_prefersNamedAndSkipsAmbiguous() {
  using C = Terrain3dService::DemCandidate;
  QCOMPARE(Terrain3dService::pickLocalDemSource(
               {C{QStringLiteral("맞춘사진"), QStringLiteral("a.tif")},
                C{QStringLiteral("국토지리원 DEM"), QStringLiteral("dem.img")}}),
           QStringLiteral("dem.img"));
  QVERIFY2(Terrain3dService::pickLocalDemSource(
               {C{QStringLiteral("스캔"), QStringLiteral("scan.tif")},
                C{QStringLiteral("단면"), QStringLiteral("sec.tif")}})
               .isEmpty(),
           "DEM이 아닌 래스터가 여럿이면 고르지 말 것");
  QCOMPARE(Terrain3dService::pickLocalDemSource({C{QStringLiteral("표고"), QStringLiteral("z.tif")}}),
           QStringLiteral("z.tif"));
  QVERIFY2(Terrain3dService::pickLocalDemSource(
               {C{QStringLiteral("DEM"), QStringLiteral("/vsicurl/http://x/dem.tif")}})
               .isEmpty(),
           "원격 DEM은 지도 DEM 쓰기에 쓰지 않음");
}

void TestTerrain3d::pickTerrainDem_allowsToolbarCopernicus() {
  using C = Terrain3dService::DemCandidate;
  QCOMPARE(Terrain3dService::pickTerrainDemSource(
               {C{QStringLiteral("DEM"),
                  QStringLiteral("/vsicurl/https://copernicus-dem-30m.s3.amazonaws.com/x.tif")}}),
           QStringLiteral("/vsicurl/https://copernicus-dem-30m.s3.amazonaws.com/x.tif"));
  QCOMPARE(Terrain3dService::pickTerrainDemSource(
               {C{QStringLiteral("국토지리원 DEM"), QStringLiteral("33606.img")},
                C{QStringLiteral("DEM"), QStringLiteral("/vsicurl/http://x/dem.tif")}}),
           QStringLiteral("33606.img"));
}

void TestTerrain3d::loadDemClip_readsWindowNotFullGrid() {
  QTemporaryDir tmp;
  QVERIFY(tmp.isValid());
  const QString path = tmp.filePath(QStringLiteral("clip.tif"));
  const QString werr = writeRampDem(path);
  QVERIFY2(werr.isEmpty(), qPrintable(werr));
  Terrain3dService::DemScene sc;
  QString err;
  QVERIFY2(Terrain3dService::loadDemClip(path, 200000.0, 450040.0, 200040.0, 450080.0, 32, &sc, &err),
           qPrintable(err));
  QVERIFY2(sc.width < 8 || sc.height < 8, "창만 읽어야 함");
  QVERIFY2(sc.width >= 2 && sc.height >= 2, "창이 너무 작으면 안 됨");
  QVERIFY2(sc.groundWidthM < 80.0, "클립 폭이 전체 80m보다 작아야 함");
}

void TestTerrain3d::renderPerspective_keepsTextureDetail() {
  Terrain3dService::Mesh m;
  m.positions = {-12.f, -12.f, 0.f, 12.f, -12.f, 0.f, 12.f, 12.f, 0.f, -12.f, 12.f, 0.f};
  m.uvs = {0.f, 0.f, 1.f, 0.f, 1.f, 1.f, 0.f, 1.f};
  m.indices = {0, 1, 2, 0, 2, 3};
  m.zMin = 0;
  m.zMax = 1;
  m.groundWidthM = 24;
  m.groundHeightM = 24;
  QImage tex(16, 16, QImage::Format_RGB32);
  for (int y = 0; y < 16; ++y)
    for (int x = 0; x < 16; ++x)
      tex.setPixel(x, y, ((x / 4 + y / 4) % 2) ? qRgb(220, 40, 40) : qRgb(40, 40, 220));
  const QImage out = Terrain3dService::renderPerspective(m, tex, 96, 96, 20.f, 50.f, 40.f);
  QSet<QRgb> cols;
  for (int y = 0; y < out.height(); y += 2)
    for (int x = 0; x < out.width(); x += 2)
      cols.insert(out.pixel(x, y) & 0x00ffffff);
  QVERIFY2(cols.size() > 8, "삼각형 한 색이 아니라 텍스처 무늬가 보여야 함");
}

void TestTerrain3d::googleSatUri_isHighResXyz() {
  const QString uri = Terrain3dService::googleSatelliteXyzUri();
  QVERIFY2(uri.contains(QLatin1String("mt1.google.com")), "Google 고해상 타일");
  QVERIFY2(uri.contains(QLatin1String("lyrs%3Ds")) || uri.contains(QLatin1String("lyrs=s")),
           "위성(s) 레이어");
  QVERIFY2(uri.contains(QLatin1String("3857")), "XYZ는 3857");
  QVERIFY2(!uri.contains(QLatin1String("vworld"), Qt::CaseInsensitive), "VWorld 키 경로 아님");
}

void TestTerrain3d::composeSheet_hasLegendScaleCrs() {
  QImage view(400, 240, QImage::Format_RGB32);
  view.fill(QColor(70, 110, 80));
  const QImage sheet = Terrain3dService::composeSheet(
      view, 120.0, 20.0, QStringLiteral("EPSG:5186"), QStringLiteral("33606.img"), 12.0f, 88.0f);
  QVERIFY2(sheet.width() > view.width(), "범례 열이 있어야 함");
  QVERIFY2(sheet.height() > view.height(), "축척·방위·CRS 띠가 있어야 함");
  int dark = 0;
  for (int y = sheet.height() - 40; y < sheet.height(); ++y) {
    for (int x = 10; x < sheet.width() - 10; x += 2) {
      if (qGray(sheet.pixel(x, y)) < 80)
        ++dark;
    }
  }
  QVERIFY2(dark > 15, "도면에 축척자·방위 먹선");
}

void TestTerrain3d::studioTab_isSeparateFromMapAndSection() {
  QFile mw(QStringLiteral("src/app/MainWindow.cpp"));
  QVERIFY2(mw.open(QIODevice::ReadOnly | QIODevice::Text), "MainWindow.cpp");
  const QString src = QString::fromUtf8(mw.readAll());
  QVERIFY2(src.contains(QLatin1String("openTerrain3dStudio")), "입체지형 탭 슬롯");
  QVERIFY2(src.contains(QString::fromUtf8("입체지형")), "탭 이름");
  QVERIFY2(src.contains(QLatin1String("m_terrain3dStudio")), "단면도·지도와 다른 위젯");
  QFile st(QStringLiteral("src/app/KaTerrain3dStudio.cpp"));
  QVERIFY2(st.open(QIODevice::ReadOnly | QIODevice::Text), "KaTerrain3dStudio.cpp");
  const QString body = QString::fromUtf8(st.readAll());
  QVERIFY2(body.contains(QLatin1String("Terrain3dService")), "DEM 메시 서비스");
  QVERIFY2(body.contains(QString::fromUtf8("이미지 저장")) || body.contains(QLatin1String("exportImage")),
           "축척자·방위가 들어간 이미지 출력");
  QVERIFY2(body.contains(QLatin1String("visibleWidthAtTarget")), "줌에 맞는 축척자");
  QVERIFY2(body.contains(QLatin1String("northAzimuthDeg")), "DEM 격자 북");
  QVERIFY2(body.contains(QLatin1String("pickTerrainDemSource")),
           "화면 높이: 로컬 DEM 또는 Copernicus");
  QVERIFY2(!body.contains(QLatin1String("terrainLayersTree")), "입체지형에 레이어창 없음");
  QVERIFY2(!body.contains(QLatin1String("terrainFilesList")), "입체지형에 파일함 없음");
  QVERIFY2(body.contains(QLatin1String("googleSatelliteXyzUri")), "Google 위성 자동 입히기");
  QVERIFY2(body.contains(QLatin1String("exportSheet")) || body.contains(QString::fromUtf8("입체지형 도면출력")),
           "입체지형 전용 도면출력");
  QVERIFY2(src.contains(QLatin1String("terrain3d_sheet")) ||
               src.contains(QLatin1String("Terrain3dLayoutService")),
           "입체지형 도면출력은 전용 terrain3d_sheet");
  QVERIFY2(!src.contains(QLatin1String("placeTerrain3dPicture")),
           "2D user_sheet에 입체지형을 올리지 않음");
}

void TestTerrain3d::demWorldRect_coversRotatedCorners() {
  Terrain3dService::DemScene sc;
  sc.width = 10;
  sc.height = 8;
  // origin + pixel size + shear (gt[2], gt[4]) — axis-aligned bbox is too small
  sc.geotransform[0] = 1000.0;
  sc.geotransform[1] = 10.0;
  sc.geotransform[2] = 4.0;
  sc.geotransform[3] = 2000.0;
  sc.geotransform[4] = -3.0;
  sc.geotransform[5] = -10.0;
  double xMin = 0, yMin = 0, xMax = 0, yMax = 0;
  QVERIFY2(Terrain3dService::demWorldRect(sc, &xMin, &yMin, &xMax, &yMax), "범위 계산");
  const double c00x = 1000.0;
  const double c00y = 2000.0;
  const double c10x = 1000.0 + 10.0 * 10.0;
  const double c10y = 2000.0 + -3.0 * 10.0;
  const double c01x = 1000.0 + 4.0 * 8.0;
  const double c01y = 2000.0 + -10.0 * 8.0;
  const double c11x = 1000.0 + 10.0 * 10.0 + 4.0 * 8.0;
  const double c11y = 2000.0 + -3.0 * 10.0 + -10.0 * 8.0;
  QVERIFY2(xMin <= std::min({c00x, c10x, c01x, c11x}) + 1e-9, "서쪽 꼭짓점");
  QVERIFY2(xMax >= std::max({c00x, c10x, c01x, c11x}) - 1e-9, "동쪽 꼭짓점");
  QVERIFY2(yMin <= std::min({c00y, c10y, c01y, c11y}) + 1e-9, "남쪽 꼭짓점");
  QVERIFY2(yMax >= std::max({c00y, c10y, c01y, c11y}) - 1e-9, "북쪽 꼭짓점");
  const double naiveX1 = 1000.0 + 10.0 * 10.0;
  QVERIFY2(xMax > naiveX1 + 1.0, "회전분이 naive width보다 넓음");
}

void TestTerrain3d::followup_tileNetworkAndMapStyleCards() {
  QFile ops(QStringLiteral("src/core/LayerOps.h"));
  QVERIFY2(ops.open(QIODevice::ReadOnly | QIODevice::Text), "LayerOps.h");
  const QString hdr = QString::fromUtf8(ops.readAll());
  QVERIFY2(hdr.contains(QLatin1String("ensureTileNetworkIdentity")),
           "XYZ 타일 User-Agent를 입체지형도 쓸 수 있게 공개");

  QFile st(QStringLiteral("src/app/KaTerrain3dStudio.cpp"));
  QVERIFY2(st.open(QIODevice::ReadOnly | QIODevice::Text), "KaTerrain3dStudio.cpp");
  const QString body = QString::fromUtf8(st.readAll());
  QVERIFY2(body.contains(QLatin1String("LayerOps::ensureTileNetworkIdentity")),
           "Google drape 전에 지도와 같은 타일 네트워크");
  QVERIFY2(body.contains(QLatin1String("demWorldRect")), "회전 GT DEM 범위");
}

void TestTerrain3d::terrainSheet_usesOwnSheetNotUserSheet() {
  QString src;
  for (const QString& path : {QStringLiteral("src/app/MainWindow.cpp"),
                              QStringLiteral("src/app/MainWindowExport.cpp")}) {
    QFile mw(path);
    QVERIFY2(mw.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(path));
    src += QString::fromUtf8(mw.readAll());
  }
  QVERIFY2(!src.contains(QLatin1String("m_terrain3dStudio->runExportSheet()")),
           "툴바 도면출력은 PNG 저장이 아님");
  QVERIFY2(src.contains(QLatin1String("placeTerrain3dOnSheet")), "입체지형 → 조판");
  QVERIFY2(src.contains(QLatin1String("terrain3d_sheet")) ||
               src.contains(QLatin1String("openTerrain3dLayout")),
           "2D 레이아웃 탭이 아니라 입체지형 조판");
  QVERIFY2(!src.contains(QLatin1String("m_drawingStudio->placeTerrain3dPicture")),
           "user_sheet에 입체지형을 겹치지 않음");

  QFile st(QStringLiteral("src/app/KaTerrain3dStudio.cpp"));
  QVERIFY2(st.open(QIODevice::ReadOnly | QIODevice::Text), "KaTerrain3dStudio.cpp");
  const QString body = QString::fromUtf8(st.readAll());
  QVERIFY2(body.contains(QLatin1String("requestDrawingStudio")) ||
               body.contains(QLatin1String("openTerrain3dLayout")),
           "입체지형 도면출력이 전용 조판을 연다");

  QVERIFY2(src.contains(QString::fromUtf8("입체지형_도면.png")),
           "도면 그림은 조사 폴더에 둠");
}

void TestTerrain3d::studio_hidesManualChrome() {
  QFile st(QStringLiteral("src/app/KaTerrain3dStudio.cpp"));
  QVERIFY2(st.open(QIODevice::ReadOnly | QIODevice::Text), "KaTerrain3dStudio.cpp");
  const QString body = QString::fromUtf8(st.readAll());
  QVERIFY2(!body.contains(QString::fromUtf8("DEM 열기")), "수동 DEM 열기 없음");
  QVERIFY2(!body.contains(QString::fromUtf8("지도 DEM 쓰기")), "수동 지도 DEM 없음");
  QVERIFY2(!body.contains(QString::fromUtf8("위성 입히기")), "수동 위성 입히기 없음");
  QVERIFY2(!body.contains(QString::fromUtf8("사진 파일")), "수동 사진 파일 없음");
  QVERIFY2(!body.contains(QString::fromUtf8("파일함")), "파일함 칸 없음");
  QVERIFY2(!body.contains(QLatin1String("terrainLayersCard")), "레이어 칸 없음");
  QVERIFY2(body.contains(QString::fromUtf8("화면을 입체로")), "자동 입체는 유지");
}

void TestTerrain3d::terrain3dSheet_hasLegendNorthScale() {
  QFile hdr(QStringLiteral("src/core/Terrain3dLayoutService.h"));
  QVERIFY2(hdr.open(QIODevice::ReadOnly | QIODevice::Text), "Terrain3dLayoutService.h");
  QFile svc(QStringLiteral("src/core/Terrain3dLayoutService.cpp"));
  QVERIFY2(svc.open(QIODevice::ReadOnly | QIODevice::Text), "Terrain3dLayoutService.cpp");
  const QString body = QString::fromUtf8(hdr.readAll()) + QString::fromUtf8(svc.readAll());
  QVERIFY2(body.contains(QLatin1String("terrain3d_sheet")), "전용 시트 이름");
  QVERIFY2(body.contains(QLatin1String("t3d_legend")) || body.contains(QString::fromUtf8("범례")),
           "3D 조판 범례");
  QVERIFY2(body.contains(QLatin1String("t3d_north")) || body.contains(QString::fromUtf8("방위")),
           "3D 조판 방위");
  QVERIFY2(body.contains(QLatin1String("t3d_scale")) || body.contains(QString::fromUtf8("축척")),
           "3D 조판 축척");
  QVERIFY2(!body.contains(QLatin1String("\"user_sheet\"")),
           "2D user_sheet 이름을 쓰지 않음");
}

void TestTerrain3d::renderPerspective_paperWhiteAndBright() {
  Terrain3dService::Mesh empty;
  const QImage sky = Terrain3dService::renderPerspective(empty, QImage(), 80, 60, 0.f, 40.f, 80.f);
  QVERIFY2(!sky.isNull(), "빈 장면도 용지를 그림");
  QVERIFY2(qGray(sky.pixel(4, 4)) > 240, "배경은 흰색");
  QVERIFY2(qGray(sky.pixel(sky.width() - 5, 5)) > 240, "모서리도 흰색");

  QTemporaryDir tmp;
  QVERIFY(tmp.isValid());
  const QString dem = tmp.filePath(QStringLiteral("ramp.tif"));
  QVERIFY(writeRampDem(dem).isEmpty());
  Terrain3dService::DemScene sc;
  QVERIFY(Terrain3dService::loadDem(dem, &sc, nullptr));
  const Terrain3dService::Mesh mesh = Terrain3dService::buildMesh(sc, 16, 2.0f);
  QImage tex(8, 8, QImage::Format_RGB32);
  tex.fill(QColor(180, 180, 180));
  const QImage out = Terrain3dService::renderPerspective(mesh, tex, 180, 130, 35.f, 42.f, 90.f);
  int paper = 0;
  int terrain = 0;
  qint64 sum = 0;
  for (int y = 2; y < out.height() - 2; y += 2) {
    for (int x = 2; x < out.width() - 2; x += 2) {
      const int g = qGray(out.pixel(x, y));
      if (g > 245)
        ++paper;
      else {
        ++terrain;
        sum += g;
      }
    }
  }
  QVERIFY2(paper > 8, "지형 밖은 흰 용지");
  QVERIFY2(terrain > 20, "회색 지형이 보여야 함");
  const double mean = double(sum) / double(terrain);
  QVERIFY2(mean > 135.0, "위성에 강한 그림자를 곱하지 않음");
}

void TestTerrain3d::renderPerspective_softShadeNoFacetBands() {
  QTemporaryDir tmp;
  QVERIFY(tmp.isValid());
  const QString dem = tmp.filePath(QStringLiteral("ramp.tif"));
  QVERIFY(writeRampDem(dem).isEmpty());
  Terrain3dService::DemScene sc;
  QVERIFY(Terrain3dService::loadDem(dem, &sc, nullptr));
  const Terrain3dService::Mesh mesh = Terrain3dService::buildMesh(sc, 16, 3.0f);
  QImage tex(8, 8, QImage::Format_RGB32);
  tex.fill(QColor(170, 170, 170));
  const QImage out = Terrain3dService::renderPerspective(mesh, tex, 200, 140, 40.f, 38.f, 85.f);
  int gmin = 255;
  int gmax = 0;
  int n = 0;
  for (int y = 4; y < out.height() - 4; y += 2) {
    for (int x = 4; x < out.width() - 4; x += 2) {
      const int g = qGray(out.pixel(x, y));
      if (g > 245)
        continue;
      gmin = std::min(gmin, g);
      gmax = std::max(gmax, g);
      ++n;
    }
  }
  QVERIFY2(n > 30, "지형 픽셀");
  QVERIFY2(gmax - gmin < 70, "면 단위 음영 띠(물결 잔상) 금지");
}

void TestTerrain3d::terrain3dSheet_matches2dChrome() {
  QFile svc(QStringLiteral("src/core/Terrain3dLayoutService.cpp"));
  QVERIFY2(svc.open(QIODevice::ReadOnly | QIODevice::Text), "Terrain3dLayoutService.cpp");
  const QString body = QString::fromUtf8(svc.readAll());
  QVERIFY2(body.contains(QLatin1String("standardSheetChrome")),
           "2D와 같은 용지 띠(축척·좌표계·방위)");
  QVERIFY2(body.contains(QLatin1String("applySheetScaleBarInk")), "축척자 먹선");
  QVERIFY2(body.contains(QString::fromUtf8("축척 1")), "축척 1 : N 라벨");
  QVERIFY2(!body.contains(QLatin1String("300.0, 230.0")), "왼쪽만 쓰는 3D 칸 아님");
  QVERIFY2(!body.contains(QLatin1String("326.0, 18.0")), "오른쪽 빈 범례 칸 아님");
  QVERIFY2(body.contains(QLatin1String("legend->setZValue(pic->zValue()")),
           "범례가 PNG 위에 있어야 함");
}

void TestTerrain3d::orbit_pitchFollowsMouseDown() {
  QFile f(QStringLiteral("src/app/KaTerrain3dView.cpp"));
  QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text), "KaTerrain3dView.cpp");
  const QString body = QString::fromUtf8(f.readAll());
  QVERIFY2(body.contains(QLatin1String("m_pitch + d.y()")),
           "마우스 아래 = 시선 아래(뒤집지 않음)");
  QVERIFY2(!body.contains(QLatin1String("m_pitch - d.y()")), "Y축 반전 금지");
}

void TestTerrain3d::layoutStudio_has2dChromeCards() {
  QFile f(QStringLiteral("src/app/KaTerrain3dLayoutStudio.cpp"));
  QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text), "KaTerrain3dLayoutStudio.cpp");
  const QString body = QString::fromUtf8(f.readAll());
  QVERIFY2(body.contains(QString::fromUtf8("도면 항목")), "2D와 같은 도면 항목 카드");
  QVERIFY2(body.contains(QString::fromUtf8("방위")), "2D와 같은 방위 카드");
  QVERIFY2(body.contains(QString::fromUtf8("도면 정보")), "2D와 같은 도명 카드");
  QVERIFY2(body.contains(QString::fromUtf8("범례")), "범례는 버튼으로만");
}

void TestTerrain3d::terrain3dSheet_legendOffUntilAsked() {
  QFile svc(QStringLiteral("src/core/Terrain3dLayoutService.cpp"));
  QVERIFY2(svc.open(QIODevice::ReadOnly | QIODevice::Text), "Terrain3dLayoutService.cpp");
  const QString body = QString::fromUtf8(svc.readAll());
  QVERIFY2(body.contains(QLatin1String("ensureLegend")), "범례는 따로 붙임");
  const int buildAt = body.indexOf(QLatin1String("QString buildSheet"));
  const int ensureAt = body.indexOf(QLatin1String("ensureLegend"));
  QVERIFY2(buildAt >= 0 && ensureAt > buildAt, "buildSheet 다음에 ensureLegend");
  const QString buildFn = body.mid(buildAt, ensureAt - buildAt);
  QVERIFY2(!buildFn.contains(QLatin1String("kIdLegend")), "도면출력 때 범례 자동 생성 금지");
  QVERIFY2(!buildFn.contains(QLatin1String("t3d_legend")), "도면출력 때 범례 자동 생성 금지");
}

void TestTerrain3d::studio_drapesVisibleMapOverlays() {
  QFile st(QStringLiteral("src/app/KaTerrain3dStudio.cpp"));
  QVERIFY2(st.open(QIODevice::ReadOnly | QIODevice::Text), "KaTerrain3dStudio.cpp");
  const QString body = QString::fromUtf8(st.readAll());
  QVERIFY2(body.contains(QLatin1String("layers()")), "2D 캔버스 레이어를 입힘");
  QVERIFY2(body.contains(QString::fromUtf8("지질")), "지질도 입힘");
  QVERIFY2(body.contains(QString::fromUtf8("토양")), "토양도 입힘");
  QVERIFY2(body.contains(QLatin1String("survey_area")), "조사구역 폴리곤 입힘");
  QVERIFY2(body.contains(QLatin1String("prepend")), "조사구역을 입체 그림 맨 위에");
  QString mwSrc;
  for (const QString& path : {QStringLiteral("src/app/MainWindow.cpp"),
                              QStringLiteral("src/app/MainWindowExport.cpp"),
                              QStringLiteral("src/app/MainWindowEditing.cpp")}) {
    QFile mw(path);
    QVERIFY2(mw.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(path));
    mwSrc += QString::fromUtf8(mw.readAll());
  }
  QVERIFY2(mwSrc.contains(QLatin1String("refreshDrape")),
           "그린 뒤 입체 드레이프를 다시 입힘");
}

void TestTerrain3d::layoutStudio_hasLayerTreeAndDeleteUndo() {
  QFile f(QStringLiteral("src/app/KaTerrain3dLayoutStudio.cpp"));
  QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text), "KaTerrain3dLayoutStudio.cpp");
  const QString body = QString::fromUtf8(f.readAll());
  QVERIFY2(body.contains(QLatin1String("layersCard")) || body.contains(QLatin1String("QgsLayerTreeView")),
           "2D와 같은 레이어창");
  QVERIFY2(body.contains(QLatin1String("Key_Delete")), "Delete로 항목 삭제");
  QVERIFY2(body.contains(QLatin1String("Undo")) || body.contains(QLatin1String("undoLast")),
           "Ctrl+Z로 되돌리기");
}

void TestTerrain3d::terrain3dSheet_hidesDummyFill() {
  QFile svc(QStringLiteral("src/core/Terrain3dLayoutService.cpp"));
  QVERIFY2(svc.open(QIODevice::ReadOnly | QIODevice::Text), "Terrain3dLayoutService.cpp");
  const QString body = QString::fromUtf8(svc.readAll());
  QVERIFY2(body.contains(QLatin1String("setOpacity")) || body.contains(QLatin1String("Qt::transparent")),
           "더미 폴리곤이 노란 막대로 비치지 않음");
  QVERIFY2(body.contains(QLatin1String("Stretch")) || body.contains(QLatin1String("ZoomResizeFrame")),
           "그림이 칸을 채워 옆이 안 보임");
}

void TestTerrain3d::applyScale_usesPaperScaleNotBarOnly() {
  QFile svc(QStringLiteral("src/core/Terrain3dLayoutService.cpp"));
  QVERIFY2(svc.open(QIODevice::ReadOnly | QIODevice::Text), "Terrain3dLayoutService.cpp");
  const QString body = QString::fromUtf8(svc.readAll());
  QVERIFY2(body.contains(QLatin1String("extentForPaperScale")),
           "축척 숫자에 맞춰 맵 범위를 바꿈");
  QFile st(QStringLiteral("src/app/KaTerrain3dLayoutStudio.cpp"));
  QVERIFY2(st.open(QIODevice::ReadOnly | QIODevice::Text), "KaTerrain3dLayoutStudio.cpp");
  QVERIFY2(QString::fromUtf8(st.readAll()).contains(QLatin1String("requestScale")),
           "입체지형 그림이 축척을 따라감");
}

void TestTerrain3d::distanceForVisibleWidth_invertsVisibleWidth() {
  const float d = 120.0f;
  const double w = Terrain3dService::visibleWidthAtTarget(d, 1600, 1000);
  const double back = Terrain3dService::distanceForVisibleWidth(w, 1600, 1000);
  QVERIFY2(std::abs(back - d) < 1.5, "축척용 카메라 거리를 되돌릴 수 있음");
}

void TestTerrain3d::placeTerrain3d_detachesBeforeReplaceSheet() {
  QString body;
  for (const QString& path : {QStringLiteral("src/app/MainWindow.cpp"),
                              QStringLiteral("src/app/MainWindowExport.cpp")}) {
    QFile mw(path);
    QVERIFY2(mw.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(path));
    body += QString::fromUtf8(mw.readAll());
  }
  const int fn = body.indexOf(QLatin1String("void MainWindow::placeTerrain3dOnSheet"));
  QVERIFY2(fn >= 0, "placeTerrain3dOnSheet");
  const int next = body.indexOf(QLatin1String("void MainWindow::"), fn + 8);
  const QString fnBody = body.mid(fn, (next > fn ? next : body.size()) - fn);
  const int det = fnBody.indexOf(QLatin1String("detachSheet"));
  const int build = fnBody.indexOf(QLatin1String("buildSheet"));
  QVERIFY2(det >= 0 && build > det, "뷰를 뗀 뒤에야 replaceSheet/removeLayout");
}

void TestTerrain3d::studio_buildsHighResFromCanvasView() {
  QFile st(QStringLiteral("src/app/KaTerrain3dStudio.cpp"));
  QVERIFY2(st.open(QIODevice::ReadOnly | QIODevice::Text), "KaTerrain3dStudio.cpp");
  const QString body = QString::fromUtf8(st.readAll());
  QVERIFY2(body.contains(QLatin1String("tryAutoFill")), "탭에서 현재 지도 화면을 입체로");
  QVERIFY2(body.contains(QLatin1String("showEvent")), "탭이 보일 때 자동");
  QVERIFY2(body.contains(QLatin1String("loadDemClip")), "화면 범위만 DEM을 자름");
  QVERIFY2(body.contains(QLatin1String("3072")) || body.contains(QLatin1String("4096")),
           "위성은 고해상으로 받음");
  QVERIFY2(body.contains(QString::fromUtf8("화면을 입체로")), "DEM 파일 고르기가 기본이 아님");
  QVERIFY2(body.contains(QLatin1String("return loadDemClipToCanvas")),
           "캔버스 dest CRS 화면이 있으면 전체 도엽 loadDem 금지");
  QVERIFY2(body.contains(QLatin1String("refreshDrape()")),
           "같은 화면으로 돌아와도 조사구역 드레이프를 다시 입힘");

  QFile hdr(QStringLiteral("src/core/LayerOps.h"));
  QVERIFY2(hdr.open(QIODevice::ReadOnly | QIODevice::Text), "LayerOps.h");
  QVERIFY2(QString::fromUtf8(hdr.readAll()).contains(QLatin1String("copernicusCogUriForWgs84")),
           "화면에 DEM이 없으면 Copernicus 높이");
}

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  TestTerrain3d tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "test_terrain_3d.moc"
