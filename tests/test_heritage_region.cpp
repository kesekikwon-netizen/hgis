#include <QtTest>
#include <QUrlQuery>
#include <qgsapplication.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

#include "core/HeritageNearbyRegions.h"
#include "core/HeritageRegionResolver.h"
#include "core/KoreaRegionCatalog.h"

// 계획서 2026-09-11-heritage-intranet-nearby-sites.md 7.1 을 붙잡는다.
// 인트라넷 검색 단위는 시/군 하나다. 읍면동 이하로 쪼개지 않는다.
class HeritageRegionTest : public QObject {
  Q_OBJECT
private slots:
  void addressTextSplitsIntoSidoAndCity() {
    const HeritageRegion r =
        HeritageRegionResolver::fromAddressText(QStringLiteral("경상북도 안동시 풍천면 하회리 100"));
    QVERIFY(r.ok());
    QCOMPARE(r.sido, QStringLiteral("경상북도"));
    QCOMPARE(r.city, QStringLiteral("안동시"));
    QCOMPARE(r.display(), QStringLiteral("경상북도 안동시"));
  }

  void countryPrefixAndShortSidoStillWork() {
    const HeritageRegion a =
        HeritageRegionResolver::fromAddressText(QStringLiteral("대한민국 경상북도 안동시"));
    QCOMPARE(a.city, QStringLiteral("안동시"));
    // 강원도·전라북도는 특별자치도로 바뀌었다. 옛 표기로 와도 같은 곳이어야 한다.
    const HeritageRegion b =
        HeritageRegionResolver::fromAddressText(QStringLiteral("강원도 춘천시 효자동"));
    QCOMPARE(b.sido, QStringLiteral("강원특별자치도"));
    QCOMPARE(b.city, QStringLiteral("춘천시"));
  }

  void districtInsideBigCityFoldsToTheCity() {
    // 사이트 시군구 드롭다운은 「수원시」 단위다. 「수원시 장안구」로 와도 접어야 한다.
    const HeritageRegion r =
        HeritageRegionResolver::fromAddressText(QStringLiteral("경기도 수원시 장안구 파장동"));
    QCOMPARE(r.sido, QStringLiteral("경기도"));
    QCOMPARE(r.city, QStringLiteral("수원시"));
  }

  void metropolitanDistrictIsTheCityUnit() {
    const HeritageRegion r =
        HeritageRegionResolver::fromAddressText(QStringLiteral("서울특별시 종로구 세종로"));
    QCOMPARE(r.sido, QStringLiteral("서울특별시"));
    QCOMPARE(r.city, QStringLiteral("종로구"));
  }

  void garbageDoesNotPretendToResolve() {
    QVERIFY(!HeritageRegionResolver::fromAddressText(QString()).ok());
    QVERIFY(!HeritageRegionResolver::fromAddressText(QStringLiteral("어딘가 알 수 없는 곳")).ok());
  }

  void vworldResponseIsParsed() {
    const QByteArray body = QStringLiteral(R"({"response":{"status":"OK","result":[
      {"type":"parcel","text":"경상북도 안동시 풍천면 하회리 산1",
       "structure":{"level0":"대한민국","level1":"경상북도","level2":"안동시","level3":"풍천면"}}]}})")
                                .toUtf8();
    const HeritageRegion r = HeritageRegionResolver::parseAddress(body);
    QVERIFY(r.ok());
    QCOMPARE(r.city, QStringLiteral("안동시"));
    QCOMPARE(r.source, QStringLiteral("vworld"));
  }

  void failedResponseIsNotTreatedAsSuccess() {
    const QByteArray body =
        QStringLiteral(R"({"response":{"status":"NOT_FOUND","result":[]}})").toUtf8();
    QVERIFY(!HeritageRegionResolver::parseAddress(body).ok());
    QVERIFY(!HeritageRegionResolver::parseAddress(QByteArray("<html>error</html>")).ok());
  }

  void rejectedKeyExplainsBothAutomaticRegionAndCadastral() {
    for (const QByteArray code : {QByteArray("INVALID_KEY"), QByteArray("INCORRECT_KEY")}) {
      const QByteArray body = "{\"response\":{\"status\":\"ERROR\",\"error\":{\"code\":\"" + code
          + "\",\"text\":\"private-key-must-not-appear\"}}}";
      QString error;
      QVERIFY(!HeritageRegionResolver::parseAddress(body, &error).ok());
      QVERIFY(error.contains(QStringLiteral("VWorld API 키")));
      QVERIFY(error.contains(QStringLiteral("자동 시·도")));
      QVERIFY(error.contains(QStringLiteral("지적도")));
      QVERIFY(error.contains(QString::fromLatin1(code)));
      QVERIFY(!error.contains(QStringLiteral("private-key")));
    }
    QString error = QStringLiteral("old failure");
    const QByteArray ok = QStringLiteral(R"({"response":{"status":"OK","result":[
      {"structure":{"level1":"경상북도","level2":"안동시"}}]}})").toUtf8();
    QVERIFY(HeritageRegionResolver::parseAddress(ok, &error).ok());
    QVERIFY(error.isEmpty());
  }

  void addressUrlCarriesPointAndKey() {
    const QUrl url = HeritageRegionResolver::buildAddressUrl(QStringLiteral("KEY123"), 128.5, 36.5);
    const QUrlQuery q(url);
    QCOMPARE(url.host(), QStringLiteral("api.vworld.kr"));
    QCOMPARE(q.queryItemValue(QStringLiteral("request")), QStringLiteral("getAddress"));
    QCOMPARE(q.queryItemValue(QStringLiteral("crs")), QStringLiteral("EPSG:4326"));
    QVERIFY(q.queryItemValue(QStringLiteral("point")).startsWith(QStringLiteral("128.5")));
    QCOMPARE(q.queryItemValue(QStringLiteral("key")), QStringLiteral("KEY123"));
  }

  void lookupPointStaysInsideAConcaveSurveyArea() {
    // ㄷ 자 조사구역. 무게중심은 도형 밖으로 나간다.
    const QgsGeometry area = QgsGeometry::fromWkt(QStringLiteral(
        "POLYGON((0 0, 30 0, 30 10, 10 10, 10 20, 30 20, 30 30, 0 30, 0 0))"));
    QVERIFY(!area.isNull());
    const QgsPointXY inside = HeritageRegionResolver::lookupPoint(area);
    QVERIFY2(area.contains(QgsGeometry::fromPointXY(inside)),
             qPrintable(QStringLiteral("%1,%2").arg(inside.x()).arg(inside.y())));
  }

  void projectCrsPointBecomesLonLat() {
    // 5186 안동 부근 좌표가 위경도로 제대로 넘어가는지.
    const QgsPointXY wgs = HeritageRegionResolver::toWgs84(
        QgsPointXY(388000., 400000.), QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    QVERIFY2(wgs.x() > 124. && wgs.x() < 132., qPrintable(QString::number(wgs.x())));
    QVERIFY2(wgs.y() > 33. && wgs.y() < 39., qPrintable(QString::number(wgs.y())));
  }

  void boundaryLayerFallbackFindsTheCity() {
    QgsProject project;
    auto* layer = new QgsVectorLayer(
        QStringLiteral("Polygon?crs=EPSG:4326&field=full_nm:string(80)"),
        QStringLiteral("행정경계"), QStringLiteral("memory"));
    QVERIFY(layer->isValid());
    layer->startEditing();
    QgsFeature f(layer->fields());
    f.setGeometry(QgsGeometry::fromWkt(
        QStringLiteral("POLYGON((128.5 36.4, 128.9 36.4, 128.9 36.7, 128.5 36.7, 128.5 36.4))")));
    f.setAttribute(QStringLiteral("full_nm"), QStringLiteral("경상북도 안동시 풍천면"));
    layer->addFeature(f);
    layer->commitChanges();
    project.addMapLayer(layer);

    const HeritageRegion hit =
        HeritageRegionResolver::fromBoundaryLayers(&project, QgsPointXY(128.7, 36.55));
    QVERIFY(hit.ok());
    QCOMPARE(hit.city, QStringLiteral("안동시"));
    QCOMPARE(hit.source, QStringLiteral("layer"));
    QVERIFY(hit.insideSidoBounds);

    // 경계 밖 점은 못 찾았다고 해야 한다. 아무 시/군이나 돌려주면 안 된다.
    QVERIFY(!HeritageRegionResolver::fromBoundaryLayers(&project, QgsPointXY(127.0, 37.5)).ok());
  }

  // F120: the 5 km scope decides which neighbouring 시/군 are offered in the confirm dialog.
  void fiveKmScopeNeedsAMetricCrs() {
    const QgsGeometry area =
        QgsGeometry::fromRect(QgsRectangle(388000., 400000., 388100., 400100.));
    const QgsGeometry scope = HeritageNearbyRegions::scopeWgs84(
        area, QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    QVERIFY(!scope.isEmpty());
    // 100 m + 2 × 5 km ≈ 0.11° of longitude near 36.5°N.
    const double width = scope.boundingBox().width();
    QVERIFY2(width > 0.10 && width < 0.14, qPrintable(QString::number(width)));
    // A degree CRS cannot be buffered by metres. No guess: no scope.
    QVERIFY(HeritageNearbyRegions::scopeWgs84(
                QgsGeometry::fromRect(QgsRectangle(128.6, 36.5, 128.61, 36.51)),
                QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4326")))
                .isEmpty());
  }

  void districtAnswerListsEachTouchedCityOnce() {
    const QgsGeometry scope = QgsGeometry::fromWkt(QStringLiteral(
        "POLYGON((128.60 36.50, 128.80 36.50, 128.80 36.65, 128.60 36.65, 128.60 36.50))"));
    auto feature = [](const char* name, double x0, double y0, double x1, double y1) {
      return QStringLiteral(
                 R"({"type":"Feature","properties":{"sig_cd":"47000","full_nm":"%1"},)"
                 R"("geometry":{"type":"MultiPolygon","coordinates":[[[[%2,%3],[%4,%3],[%4,%5],[%2,%5],[%2,%3]]]]}})")
          .arg(QString::fromUtf8(name))
          .arg(x0).arg(y0).arg(x1).arg(y1);
    };
    const QStringList features = {
        feature("경상북도 안동시", 128.55, 36.45, 128.75, 36.70),
        feature("경상북도 예천군", 128.30, 36.40, 128.62, 36.80),
        feature("경상북도 포항시 남구", 128.78, 36.40, 129.00, 36.60),
        feature("경상북도 안동시", 128.70, 36.55, 128.90, 36.70),
        feature("경기도 수원시 장안구", 126.90, 37.25, 127.05, 37.35),
    };
    const QByteArray body =
        (QStringLiteral(R"({"response":{"status":"OK","page":{"total":"1","current":"1"},)"
                        R"("result":{"featureCollection":{"type":"FeatureCollection","features":[)") +
         features.join(QLatin1Char(',')) + QStringLiteral("]}}}}"))
            .toUtf8();
    bool ok = false;
    bool more = true;
    const QList<HeritageCity> cities = HeritageNearbyRegions::parseDistricts(body, scope, &ok, &more);
    QVERIFY(ok);
    QVERIFY(!more);
    QCOMPARE(cities.size(), 3);
    QCOMPARE(cities.at(0).display(), QStringLiteral("경상북도 안동시"));
    QCOMPARE(cities.at(1).display(), QStringLiteral("경상북도 예천군"));
    // A 구 of a big city folds to the intranet unit (포항시), never to 읍면동.
    QCOMPARE(cities.at(2).display(), QStringLiteral("경상북도 포항시"));
  }

  void districtFailuresAreNotSilentSuccess() {
    const QgsGeometry scope = QgsGeometry::fromRect(QgsRectangle(128.6, 36.5, 128.8, 36.65));
    bool ok = true;
    QVERIFY(HeritageNearbyRegions::parseDistricts(
                QByteArray(R"({"response":{"status":"ERROR"}})"), scope, &ok).isEmpty());
    QVERIFY(!ok);
    QVERIFY(HeritageNearbyRegions::parseDistricts(QByteArray("<html/>"), scope, &ok).isEmpty());
    QVERIFY(!ok);
    // NOT_FOUND is a real answer (no 시·군·구 in the box), not a failure.
    QVERIFY(HeritageNearbyRegions::parseDistricts(
                QByteArray(R"({"response":{"status":"NOT_FOUND"}})"), scope, &ok).isEmpty());
    QVERIFY(ok);
  }

  void districtUrlAsksForTheScopeBox() {
    const QUrl url = HeritageNearbyRegions::districtsUrl(QStringLiteral("KEY123"),
                                                         QgsRectangle(128.6, 36.5, 128.8, 36.65));
    const QUrlQuery q(url);
    QCOMPARE(url.host(), QStringLiteral("api.vworld.kr"));
    QCOMPARE(q.queryItemValue(QStringLiteral("data")), QStringLiteral("LT_C_ADSIGG_INFO"));
    QCOMPARE(q.queryItemValue(QStringLiteral("crs")), QStringLiteral("EPSG:4326"));
    QVERIFY(q.queryItemValue(QStringLiteral("geomFilter")).startsWith(QStringLiteral("BOX(128.6")));
    QCOMPARE(q.queryItemValue(QStringLiteral("key")), QStringLiteral("KEY123"));
  }

  void boundaryLayersOfferNeighboursOffline() {
    QgsProject project;
    auto* layer = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:4326&field=full_nm:string(80)"),
                                     QStringLiteral("행정경계"), QStringLiteral("memory"));
    QVERIFY(layer->isValid());
    QVERIFY(layer->startEditing());
    const QList<QPair<QString, QString>> parts = {
        {QStringLiteral("경상북도 안동시"), QStringLiteral("POLYGON((128.5 36.4, 128.7 36.4, 128.7 36.7, 128.5 36.7, 128.5 36.4))")},
        {QStringLiteral("경상북도 예천군"), QStringLiteral("POLYGON((128.3 36.4, 128.5 36.4, 128.5 36.7, 128.3 36.7, 128.3 36.4))")},
        {QStringLiteral("경상북도 울진군"), QStringLiteral("POLYGON((129.2 36.8, 129.4 36.8, 129.4 37.0, 129.2 37.0, 129.2 36.8))")},
    };
    for (const auto& part : parts) {
      QgsFeature f(layer->fields());
      f.setGeometry(QgsGeometry::fromWkt(part.second));
      f.setAttribute(QStringLiteral("full_nm"), part.first);
      QVERIFY(layer->addFeature(f));
    }
    QVERIFY(layer->commitChanges());
    project.addMapLayer(layer);
    const QgsGeometry scope = QgsGeometry::fromRect(QgsRectangle(128.45, 36.5, 128.65, 36.6));
    const QList<HeritageCity> cities = HeritageNearbyRegions::fromBoundaryLayers(&project, scope);
    QCOMPARE(cities.size(), 2);
    QCOMPARE(cities.at(0).city, QStringLiteral("안동시"));
    QCOMPARE(cities.at(1).city, QStringLiteral("예천군"));
  }

  // F123: the 시·도/시·군 table can be replaced by data after a reform, without a rebuild.
  void regionTableIsUpdatableData() {
    QHash<QString, QString> aliases;
    QString error;
    const QVector<KoreaSido> table = KoreaRegionCatalog::parseTable(
        QStringLiteral(R"({"version":1,"sido":[{"name":"전남광주통합특별시","short":"전남광주",)"
                       R"("aliases":["광주광역시","전라남도"],"cities":["광산구","목포시","광산구"]}]})")
            .toUtf8(),
        &aliases, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(table.size(), 1);
    QCOMPARE(table.first().shortName, QStringLiteral("전남광주"));
    QCOMPARE(table.first().cities, (QStringList{QStringLiteral("광산구"), QStringLiteral("목포시")}));
    QCOMPARE(aliases.value(QStringLiteral("전라남도")), QStringLiteral("전남광주통합특별시"));
    // One broken entry rejects the file; the built-in table is then kept.
    QVERIFY(KoreaRegionCatalog::parseTable(
                QStringLiteral(R"({"sido":[{"name":"경기도","cities":["수원시"]},{"name":"","cities":["x"]}]})")
                    .toUtf8(),
                nullptr, &error)
                .isEmpty());
    QVERIFY(!error.isEmpty());
    QVERIFY(KoreaRegionCatalog::parseTable(QByteArray("not json")).isEmpty());
    QVERIFY(!KoreaRegionCatalog::tableSource().isEmpty());
    if (KoreaRegionCatalog::tableSource() == QLatin1String("built-in"))
      QVERIFY(KoreaRegionCatalog::sidoNames().contains(QStringLiteral("광주광역시")));
  }

  void emptyGeometrySaysSoInsteadOfGuessing() {
    HeritageRegionResolver resolver;
    QSignalSpy failed(&resolver, &HeritageRegionResolver::failed);
    QSignalSpy resolvedSpy(&resolver, &HeritageRegionResolver::resolved);
    resolver.resolve(QgsGeometry(), QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    QCOMPARE(failed.count(), 1);
    QCOMPARE(resolvedSpy.count(), 0);
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(
      qEnvironmentVariable("QGIS_PREFIX_PATH", "D:/OSGeo4W/apps/qgis-dev"), true);
  QgsApplication::initQgis();
  HeritageRegionTest test;
  const int result = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis();
  return result;
}
#include "test_heritage_region.moc"
