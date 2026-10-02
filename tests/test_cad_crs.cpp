// 좌표계 없는 CAD 도면 숫자로 한국 좌표계 후보를 찾는 CadCrsGuess.
// 기준 숫자는 사용자 가수리 DWG(5174 로 읽으면 영천 고경면)와 제주 DXF(5186 제주시)에서 잰 값이다(2026-10-02).
#include <QStandardPaths>
#include <QtTest>

#include "core/CadCrsGuess.h"
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
    const QgsRectangle extent(387199.70, 279739.90, 388199.70, 280739.90);
    const CadCrsResult r = CadCrsGuess::guess(extent, crs5187(), QgsPointXY(207440.79, 378546.00), context());
    QCOMPARE(r.verdict, CadCrsVerdict::Certain);
    QVERIFY2(r.candidates.size() >= 3, qUtf8Printable(ids(r).join(QLatin1Char(','))));
    QCOMPARE(r.candidates[0].authId, QStringLiteral("EPSG:5174"));
    QCOMPARE(r.candidates[0].region, QStringLiteral("경상북도"));
    QVERIFY(r.candidates[0].distanceToSiteM >= 0 && r.candidates[0].distanceToSiteM < 1000);
    QCOMPARE(r.candidates[1].authId, QStringLiteral("EPSG:5181"));
    QCOMPARE(r.candidates[2].authId, QStringLiteral("EPSG:2097"));
  }

  void guess_withoutSiteListsPlacesToChoose() {
    const QgsRectangle extent(387199.70, 279739.90, 388199.70, 280739.90);
    const CadCrsResult r = CadCrsGuess::guess(extent, crs5187(), std::nullopt, context());
    QCOMPARE(r.verdict, CadCrsVerdict::Choose);
    QVERIFY(!r.candidates.isEmpty());
    QCOMPARE(r.candidates[0].authId, QStringLiteral("EPSG:5186"));
    QCOMPARE(r.candidates[0].region, QStringLiteral("부산광역시"));
    const CadCrsCandidate* bessel = find(r, QStringLiteral("EPSG:5174"));
    QVERIFY2(bessel, qUtf8Printable(ids(r).join(QLatin1Char(','))));
    QCOMPARE(bessel->region, QStringLiteral("경상북도"));
    for (const CadCrsCandidate& c : r.candidates) QCOMPARE(c.distanceToSiteM, -1.0);
  }

  // 원점 가까운 로컬 숫자. 동부원점(FN 500000)으로 읽으면 제주도에 떨어지지만 후보로 남기지 않는다.
  void guess_localNumbersHaveNoCrs() {
    const CadCrsResult r = CadCrsGuess::guess(QgsRectangle(0, 0, 1000, 800), crs5187(), std::nullopt, context());
    QCOMPARE(r.verdict, CadCrsVerdict::NoCrs);
    QVERIFY2(r.candidates.isEmpty(), qUtf8Printable(ids(r).join(QLatin1Char(','))));
  }

  void guess_jejuNumbersWithJejuSiteAreCertain5186() {
    const QgsRectangle extent(148078.47, 98110.53, 148107.88, 98129.23);
    const CadCrsResult r = CadCrsGuess::guess(extent, crs5186(), extent.center(), context());
    QCOMPARE(r.verdict, CadCrsVerdict::Certain);
    QCOMPARE(r.candidates[0].authId, QStringLiteral("EPSG:5186"));
    QCOMPARE(r.candidates[0].region, QStringLiteral("제주특별자치도"));
  }

  void guess_degreesAreWgs84() {
    const QgsPointXY site = toCrs(QgsPointXY(129.08, 36.0), QStringLiteral("EPSG:4326"), QStringLiteral("EPSG:5187"));
    const CadCrsResult r = CadCrsGuess::guess(QgsRectangle(129.07, 35.99, 129.09, 36.01), crs5187(), site, context());
    QCOMPARE(r.verdict, CadCrsVerdict::Certain);
    QCOMPARE(r.candidates[0].authId, QStringLiteral("EPSG:4326"));
  }

  void guess_utmkIsFound() {
    const QgsPointXY utmk = toCrs(QgsPointXY(129.08, 36.0), QStringLiteral("EPSG:4326"), QStringLiteral("EPSG:5179"));
    const QgsPointXY site = toCrs(QgsPointXY(129.08, 36.0), QStringLiteral("EPSG:4326"), QStringLiteral("EPSG:5187"));
    const QgsRectangle extent(utmk.x() - 300, utmk.y() - 300, utmk.x() + 300, utmk.y() + 300);
    const CadCrsResult r = CadCrsGuess::guess(extent, crs5187(), site, context());
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

  void siteLocation_prefersSurveyLayersThenANarrowView() {
    QgsProject project;
    project.setCrs(crs5187());
    auto* area = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187"), QStringLiteral("조사구역"),
                                    QStringLiteral("memory"));
    QVERIFY(area->isValid());
    QgsFeature feature(area->fields());
    feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(207000, 378000, 207200, 378400)));
    QVERIFY(area->dataProvider()->addFeature(feature));
    area->updateExtents();
    LayerOps::markSurveyLayer(area, QStringLiteral("survey_area"));
    project.addMapLayer(area);
    const QgsRectangle tenKm(150000, 300000, 160000, 305000);
    const std::optional<QgsPointXY> fromLayers = CadCrsGuess::siteLocation(&project, tenKm);
    QVERIFY(fromLayers.has_value());
    QCOMPARE(*fromLayers, QgsPointXY(207100, 378200));

    QgsProject empty;
    empty.setCrs(crs5187());
    const std::optional<QgsPointXY> fromView = CadCrsGuess::siteLocation(&empty, tenKm);
    QVERIFY(fromView.has_value());
    QCOMPARE(*fromView, QgsPointXY(155000, 302500));
    QVERIFY(!CadCrsGuess::siteLocation(&empty, QgsRectangle(100000, 300000, 180000, 330000)).has_value());
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
