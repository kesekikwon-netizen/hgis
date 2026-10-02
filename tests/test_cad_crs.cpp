// 좌표계 없는 CAD 도면 숫자로 한국 좌표계 후보를 찾는 CadCrsGuess.
// 기준 숫자는 사용자 가수리 DWG(5174 로 읽으면 영천 고경면)와 제주 DXF(5186 제주시)에서 잰 값이다(2026-10-02).
#include <QStandardPaths>
#include <QtTest>

#include "core/CadCrsGuess.h"
#include "core/CadDrawingLayers.h"
#include "core/LayerOps.h"

#include <qgsapplication.h>
#include <qgscoordinatetransform.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

namespace {

// QGIS 가 준비된 뒤에 만든다(정적 전역 CRS 는 initQgis 전에 생겨 좌표계 자료를 못 찾는다).
QgsCoordinateReferenceSystem crs5186() { return QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")); }
QgsCoordinateReferenceSystem crs5187() { return QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")); }

QgsPointXY toCrs(const QgsPointXY& p, const QString& from, const QString& to) {
  const QgsCoordinateTransform transform(QgsCoordinateReferenceSystem(from), QgsCoordinateReferenceSystem(to),
                                         QgsProject::instance()->transformContext());
  return transform.transform(p);
}

QStringList ids(const CadCrsResult& r) {
  QStringList out;
  for (const CadCrsCandidate& c : r.candidates) out << c.authId;
  return out;
}

const CadCrsCandidate* find(const CadCrsResult& r, const QString& authId) {
  for (const CadCrsCandidate& c : r.candidates)
    if (c.authId == authId) return &c;
  return nullptr;
}

const QgsRectangle kGasuri(387199.70, 279739.90, 388199.70, 280739.90);  // 가수리 도면 숫자(5174)
const QgsPointXY kYeongcheon(207440.79, 378546.00);                     // 그 중심을 5174 → 5187
const QgsPointXY kBusan(207440.79, 278546.00);
const QgsRectangle kKoreaWide(0, 200000, 400000, 600000);  // 한반도 전체를 보는 화면

QgsVectorLayer* polygonLayer(const QgsRectangle& rect) {
  auto* layer = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187"), QStringLiteral("면"), QStringLiteral("memory"));
  QgsFeature feature(layer->fields());
  feature.setGeometry(QgsGeometry::fromRect(rect));
  layer->dataProvider()->addFeature(feature);
  layer->updateExtents();
  return layer;
}

QgsVectorLayer* drawingLayer(const QgsRectangle& rect) {
  QgsVectorLayer* layer = polygonLayer(rect);
  layer->setCustomProperty(QString::fromLatin1(CadDrawingLayers::kPropDrawing), QStringLiteral("d1"));
  return layer;
}

QgsCoordinateTransformContext context() {
  return QgsProject::instance()->transformContext();
}

}  // namespace

class TestCadCrs : public QObject {
  Q_OBJECT
 private slots:
  void candidateAuthIds_followTheSpecOrder() {
    QCOMPARE(CadCrsGuess::candidateAuthIds(),
             QStringList({"EPSG:5186", "EPSG:5187", "EPSG:5185", "EPSG:5188", "EPSG:5174", "EPSG:5176", "EPSG:5173",
                          "EPSG:5177", "EPSG:5175", "EPSG:5181", "EPSG:5183", "EPSG:5180", "EPSG:5184", "EPSG:5182",
                          "EPSG:2097", "EPSG:2096", "EPSG:2098", "EPSG:5179", "EPSG:32652", "EPSG:32651",
                          "EPSG:4326"}));
  }

  void guess_gasuriNumbersNearYeongcheonAreCertain5174() {
    const CadCrsResult r = CadCrsGuess::guess(kGasuri, crs5187(), {.site = kYeongcheon}, context());
    QCOMPARE(r.verdict, CadCrsVerdict::Certain);
    QCOMPARE(r.reason, QStringLiteral("조사 위치"));
    QVERIFY2(r.candidates.size() >= 3, qUtf8Printable(ids(r).join(QLatin1Char(','))));
    QCOMPARE(r.candidates[0].authId, QStringLiteral("EPSG:5174"));
    QCOMPARE(r.candidates[0].region, QStringLiteral("경상북도"));
    QVERIFY(r.candidates[0].distanceToSiteM >= 0 && r.candidates[0].distanceToSiteM < 1000);
    QCOMPARE(r.candidates[1].authId, QStringLiteral("EPSG:5181"));
    QCOMPARE(r.candidates[2].authId, QStringLiteral("EPSG:2097"));
  }

  // 단서가 하나도 없어도 묻지 않는다: 기본 순서로 가장 그럴듯한 자리를 앞에 두고 나머지는 바꿀 자리로 남긴다.
  void guess_withoutCluesIsLikelyInTheDefaultOrder() {
    const CadCrsResult r = CadCrsGuess::guess(kGasuri, crs5187(), {}, context());
    QCOMPARE(r.verdict, CadCrsVerdict::Likely);
    QCOMPARE(r.reason, QStringLiteral("기본 순서"));
    QCOMPARE(r.candidates[0].authId, QStringLiteral("EPSG:5186"));
    QCOMPARE(r.candidates[0].region, QStringLiteral("부산광역시"));
    const CadCrsCandidate* bessel = find(r, QStringLiteral("EPSG:5174"));
    QVERIFY2(bessel, qUtf8Printable(ids(r).join(QLatin1Char(','))));
    QCOMPARE(bessel->region, QStringLiteral("경상북도"));
    for (const CadCrsCandidate& c : r.candidates) QCOMPARE(c.distanceToSiteM, -1.0);
  }

  void guess_recentCrsLeadsWhenNothingElseDecides() {
    const CadCrsResult r = CadCrsGuess::guess(kGasuri, crs5187(), {.recent = {"EPSG:2097", "EPSG:5186"}}, context());
    QCOMPARE(r.verdict, CadCrsVerdict::Likely);
    QCOMPARE(r.reason, QStringLiteral("최근 좌표계"));
    QCOMPARE(r.candidates[0].authId, QStringLiteral("EPSG:2097"));  // 무리 안에서도 최근 것이 앞
  }

  // 도면이 밝힌 좌표계가 가장 먼저다. 같은 숫자 무리(5174·5181·2097) 안에서도 밝힌 것을 쓴다.
  void guess_statedCrsWins() {
    const CadCrsResult r = CadCrsGuess::guess(kGasuri, crs5187(), {.stated = {"EPSG:5181"}, .site = kBusan}, context());
    QCOMPARE(r.verdict, CadCrsVerdict::Certain);
    QCOMPARE(r.reason, QStringLiteral("도면 표시"));
    QCOMPARE(r.candidates[0].authId, QStringLiteral("EPSG:5181"));
    // 한국 밖에 떨어지는 표시는 버리고 다음 단서로 간다.
    const CadCrsResult wrong = CadCrsGuess::guess(kGasuri, crs5187(), {.stated = {"EPSG:5188"}}, context());
    QCOMPARE(wrong.reason, QStringLiteral("기본 순서"));
  }

  // 지도 화면 안에 놓이는 자리가 하나면 그 자리다. 한반도 전체 화면은 자리를 줄이기만 한다.
  void guess_viewDecidesOnlyWhenOnePlaceIsInside() {
    const QgsRectangle aroundYeongcheon(177000, 348000, 237000, 408000);  // 폭 60 km
    const CadCrsResult r = CadCrsGuess::guess(kGasuri, crs5187(), {.view = aroundYeongcheon}, context());
    QCOMPARE(r.verdict, CadCrsVerdict::Certain);
    QCOMPARE(r.reason, QStringLiteral("지도 화면"));
    QCOMPARE(r.candidates[0].authId, QStringLiteral("EPSG:5174"));
    const CadCrsResult wide = CadCrsGuess::guess(kGasuri, crs5187(), {.view = kKoreaWide}, context());
    QCOMPARE(wide.verdict, CadCrsVerdict::Likely);
  }

  // 조사 위치를 알면, 정하지 못해도 가까운 자리가 최근 좌표계보다 앞이다. 50 km 안에 자리가 없으면 로컬 도면이다.
  void guess_aKnownSiteRanksByDistanceAndRejectsFarNumbers() {
    const QgsPointXY thirtyKm(kYeongcheon.x() + 30000, kYeongcheon.y());
    const CadCrsResult near = CadCrsGuess::guess(kGasuri, crs5187(), {.site = thirtyKm, .recent = {"EPSG:5186"}}, context());
    QCOMPARE(near.verdict, CadCrsVerdict::Likely);
    QCOMPARE(near.reason, QStringLiteral("가까운 순"));
    QCOMPARE(near.candidates[0].authId, QStringLiteral("EPSG:5174"));
    const QgsRectangle jeju(148078.47, 98110.53, 148107.88, 98129.23);
    QCOMPARE(CadCrsGuess::guess(jeju, crs5187(), {.site = kYeongcheon}, context()).verdict, CadCrsVerdict::NoCrs);
  }

  // 시·도 개략 범위는 겹친다: 서부원점(5173)으로 읽은 자리는 충청남도로 적히지만 전북 범위에도 든다.
  void guess_placeNamesMatchAnyOverlappingProvince() {
    const CadCrsResult r = CadCrsGuess::guess(kGasuri, crs5187(), {.provinces = {"전북특별자치도"}}, context());
    QStringList seen;
    for (const CadCrsCandidate& c : r.candidates) seen << c.authId + QLatin1Char('=') + c.regions.join(QLatin1Char('/'));
    QVERIFY2(r.verdict == CadCrsVerdict::Certain, qUtf8Printable(seen.join(QLatin1Char(' '))));
    QCOMPARE(r.candidates[0].authId, QStringLiteral("EPSG:5173"));
    QCOMPARE(r.candidates[0].region, QStringLiteral("충청남도"));
  }

  void guess_placeNamesPickTheProvince() {
    const CadCrsResult r =
        CadCrsGuess::guess(kGasuri, crs5187(), {.view = kKoreaWide, .provinces = {"경상북도"}}, context());
    QCOMPARE(r.verdict, CadCrsVerdict::Certain);
    QCOMPARE(r.reason, QStringLiteral("지명"));
    QCOMPARE(r.candidates[0].authId, QStringLiteral("EPSG:5174"));
  }

  // 원점 가까운 로컬 숫자. 동부원점(FN 500000)으로 읽으면 제주도에 떨어지지만 후보로 남기지 않는다.
  void guess_localNumbersHaveNoCrs() {
    const CadCrsResult r =
        CadCrsGuess::guess(QgsRectangle(0, 0, 1000, 800), crs5187(), {.stated = {"EPSG:5187"}}, context());
    QCOMPARE(r.verdict, CadCrsVerdict::NoCrs);
    QVERIFY2(r.candidates.isEmpty(), qUtf8Printable(ids(r).join(QLatin1Char(','))));
  }

  void guess_jejuNumbersWithJejuSiteAreCertain5186() {
    const QgsRectangle extent(148078.47, 98110.53, 148107.88, 98129.23);
    const CadCrsResult r = CadCrsGuess::guess(extent, crs5186(), {.site = extent.center()}, context());
    QCOMPARE(r.verdict, CadCrsVerdict::Certain);
    QCOMPARE(r.candidates[0].authId, QStringLiteral("EPSG:5186"));
    QCOMPARE(r.candidates[0].region, QStringLiteral("제주특별자치도"));
  }

  void guess_degreesAreWgs84() {
    const QgsPointXY site = toCrs(QgsPointXY(129.08, 36.0), QStringLiteral("EPSG:4326"), QStringLiteral("EPSG:5187"));
    const CadCrsResult r =
        CadCrsGuess::guess(QgsRectangle(129.07, 35.99, 129.09, 36.01), crs5187(), {.site = site}, context());
    QCOMPARE(r.verdict, CadCrsVerdict::Certain);
    QCOMPARE(r.candidates[0].authId, QStringLiteral("EPSG:4326"));
  }

  void guess_utmkIsFound() {
    const QgsPointXY utmk = toCrs(QgsPointXY(129.08, 36.0), QStringLiteral("EPSG:4326"), QStringLiteral("EPSG:5179"));
    const QgsPointXY site = toCrs(QgsPointXY(129.08, 36.0), QStringLiteral("EPSG:4326"), QStringLiteral("EPSG:5187"));
    const QgsRectangle extent(utmk.x() - 300, utmk.y() - 300, utmk.x() + 300, utmk.y() + 300);
    const CadCrsResult r = CadCrsGuess::guess(extent, crs5187(), {.site = site}, context());
    QCOMPARE(r.verdict, CadCrsVerdict::Certain);
    QCOMPARE(r.candidates[0].authId, QStringLiteral("EPSG:5179"));
  }

  void describe_readsLikeTheSpec() {
    CadCrsCandidate c;
    c.authId = QStringLiteral("EPSG:5174");
    c.label = CadCrsGuess::label(c.authId);
    c.region = QStringLiteral("경상북도");
    c.distanceToSiteM = 1234.0;
    QCOMPARE(CadCrsGuess::describe(c),
             QStringLiteral("경상북도 부근 · 베셀 중부원점 보정 (EPSG:5174) · 조사 지역에서 1.2 km"));
    c.distanceToSiteM = -1;
    QCOMPARE(CadCrsGuess::describe(c), QStringLiteral("경상북도 부근 · 베셀 중부원점 보정 (EPSG:5174)"));
  }

  // 조사구역이 먼저, 없으면 지도에 이미 있는 좌표 도면(한국 안에 있는 것만)의 중심. 지도 화면은 위치로 쓰지 않는다.
  void siteLocation_prefersSurveyLayersThenPlacedDrawings() {
    QgsProject project;
    project.setCrs(crs5187());
    QgsVectorLayer* area = polygonLayer(QgsRectangle(207000, 378000, 207200, 378400));
    LayerOps::markSurveyLayer(area, QStringLiteral("survey_area"));
    project.addMapLayer(area);
    project.addMapLayer(drawingLayer(QgsRectangle(150000, 300000, 150200, 300200)));
    QCOMPARE(CadCrsGuess::siteLocation(&project), std::optional<QgsPointXY>(QgsPointXY(207100, 378200)));

    QgsProject drawings;
    drawings.setCrs(crs5187());
    drawings.addMapLayer(drawingLayer(QgsRectangle(0, 0, 900, 700)));  // 로컬 숫자 도면은 위치가 아니다
    QVERIFY(!CadCrsGuess::siteLocation(&drawings).has_value());
    QgsVectorLayer* unsure = drawingLayer(QgsRectangle(200000, 350000, 200200, 350200));  // 경남 육지
    unsure->setCustomProperty(QString::fromLatin1(CadDrawingLayers::kPropUnsure), true);  // 가장 그럴듯한 자리에 둔 도면
    drawings.addMapLayer(unsure);
    QVERIFY(!CadCrsGuess::siteLocation(&drawings).has_value());
    drawings.addMapLayer(drawingLayer(QgsRectangle(150000, 300000, 150200, 300200)));
    QCOMPARE(CadCrsGuess::siteLocation(&drawings), std::optional<QgsPointXY>(QgsPointXY(150100, 300100)));
  }

  // 앱이 가장 그럴듯한 자리에 둔 도면을 보고 있는 화면은 사용자가 고른 위치가 아니다.
  void trustedView_ignoresAViewOnAnUnsureDrawing() {
    QgsProject project;
    project.setCrs(crs5187());
    const QgsRectangle view(240000, 290000, 260000, 310000);
    QCOMPARE(CadCrsGuess::trustedView(&project, view), view);
    QgsVectorLayer* unsure = drawingLayer(QgsRectangle(250000, 300000, 250200, 300200));
    unsure->setCustomProperty(QString::fromLatin1(CadDrawingLayers::kPropUnsure), true);
    project.addMapLayer(unsure);
    QVERIFY(CadCrsGuess::trustedView(&project, view).isNull());
  }
};

int main(int argc, char** argv) {
  QStandardPaths::setTestModeEnabled(true);
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  int result = 0;
  {
    TestCadCrs tests;
    result = QTest::qExec(&tests, argc, argv);
  }
  QgsApplication::exitQgis();
  return result;
}

#include "test_cad_crs.moc"
