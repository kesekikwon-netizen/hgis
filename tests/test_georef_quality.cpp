#include <cmath>

#include <QtTest>
#include <QFile>

#include "core/GeorefQuality.h"
#include "core/GeorefService.h"

#include <qgsapplication.h>

// Least-squares numerics with absolute (200000/500000-class) source coordinates, and the
// per-point residual / shape warnings shown in the align point list.
class TestGeorefQuality : public QObject {
  Q_OBJECT
private slots:
  void absoluteSource_fourPointsFitExactly();
  void absoluteSource_threePointsPredictUnpickedPoint();
  void absoluteSource_collinearFallsBackToHelmert();
  void twoPointRaster_keepsYDownFlip();
  void residuals_flagTheWrongPoint();
  void threePoints_sayResidualNeedsAFourthPoint();
  void anisotropyAndShear_areWarned();
  void similarity_hasNoWarnings();
};

namespace {

// A CAD drawing already in map coordinates (EPSG:5186-like) moved onto another map:
// rotation 3 degrees, scale 1.0005, shift of several kilometres.
void truthMap(double sx, double sy, double* mx, double* my) {
  const double th = 3.0 * 3.14159265358979323846 / 180.0;
  const double k = 1.0005;
  const double x = sx - 200000.0, y = sy - 500000.0;
  *mx = k * (std::cos(th) * x - std::sin(th) * y) + 194000.0;
  *my = k * (std::sin(th) * x + std::cos(th) * y) + 574000.0;
}

GeorefService::Pair pairAt(double sx, double sy) {
  GeorefService::Pair p;
  p.srcX = sx;
  p.srcY = sy;
  truthMap(sx, sy, &p.mapX, &p.mapY);
  return p;
}

QVector<GeorefService::Pair> squarePairs(double span, bool withCentre) {
  const double x0 = 200000.0, y0 = 500000.0;
  QVector<GeorefService::Pair> p = {pairAt(x0, y0), pairAt(x0 + span, y0),
                                    pairAt(x0 + span, y0 + span), pairAt(x0, y0 + span)};
  if (withCentre) p.append(pairAt(x0 + span * 0.5, y0 + span * 0.5));
  return p;
}

}  // namespace

void TestGeorefQuality::absoluteSource_fourPointsFitExactly() {
  // Raw sums of 200000² lost every significant digit (median error tens of metres).
  for (double span : {30.0, 100.0, 300.0}) {
    const QVector<GeorefService::Pair> p = squarePairs(span, false);
    const GeorefService::Affine a = GeorefService::fromPairs(p, false);
    QVERIFY(a.valid);
    QCOMPARE(a.pairCount, 4);
    QVERIFY2(a.rmsMeters < 1e-6, qPrintable(QStringLiteral("span %1 rms %2").arg(span).arg(a.rmsMeters)));
  }
}

void TestGeorefQuality::absoluteSource_threePointsPredictUnpickedPoint() {
  const QVector<GeorefService::Pair> p = {pairAt(200000, 500000), pairAt(200100, 500010),
                                          pairAt(200020, 500090)};
  const GeorefService::Affine a = GeorefService::fromPairs(p, false);
  QVERIFY(a.valid);
  double gx = 0, gy = 0, mx = 0, my = 0;
  truthMap(200070, 500060, &gx, &gy);
  QVERIFY(GeorefService::transform(a, 200070, 500060, &mx, &my));
  QVERIFY2(std::hypot(mx - gx, my - gy) < 1e-6,
           qPrintable(QStringLiteral("off by %1 m").arg(std::hypot(mx - gx, my - gy))));
}

void TestGeorefQuality::absoluteSource_collinearFallsBackToHelmert() {
  const QVector<GeorefService::Pair> p = {pairAt(200000, 500000), pairAt(200050, 500000),
                                          pairAt(200100, 500000)};
  const GeorefService::Affine a = GeorefService::fromPairs(p, false);
  QVERIFY2(a.valid, "a line of points still aligns with the 2-point formula");
  double gx = 0, gy = 0, mx = 0, my = 0;
  truthMap(200030, 500040, &gx, &gy);
  QVERIFY(GeorefService::transform(a, 200030, 500040, &mx, &my));
  QVERIFY2(std::hypot(mx - gx, my - gy) < 1e-4, "Helmert keeps rotation and scale");
}

void TestGeorefQuality::twoPointRaster_keepsYDownFlip() {
  QVector<GeorefService::Pair> p = {{100, 100, 100100, 500700}, {900, 700, 100900, 500100}};
  const GeorefService::Affine a = GeorefService::fromPairs(p, true);
  QVERIFY(a.valid);
  QVERIFY2(a.a * a.e - a.b * a.d < 0.0, "image rows grow downwards: the fit must flip");
}

void TestGeorefQuality::residuals_flagTheWrongPoint() {
  QVector<GeorefService::Pair> p = squarePairs(100.0, true);
  p[4].mapX += 3.0;  // the centre point was clicked 3 m off
  const GeorefQuality::Report r = GeorefQuality::assess(p, false);
  QVERIFY(r.measurable);
  QCOMPARE(r.residuals.size(), 5);
  QCOMPARE(r.maxIndex, 4);
  QVERIFY2(r.maxResidual > GeorefQuality::kResidualWarn, qPrintable(QString::number(r.maxResidual)));
  QVERIFY(GeorefQuality::rowText(r, 4).contains(QStringLiteral("(가장 큼)")));
  QVERIFY(GeorefQuality::rowText(r, 0).contains(QStringLiteral(" m")));
  const QStringList notes = GeorefQuality::notes(r);
  QVERIFY2(!notes.isEmpty() && notes.first().startsWith(QStringLiteral("주의: 5번 점")),
           qPrintable(notes.join('|')));
  QVERIFY(GeorefQuality::summary(r).contains(QStringLiteral("최대")));
}

void TestGeorefQuality::threePoints_sayResidualNeedsAFourthPoint() {
  const QVector<GeorefService::Pair> p = {pairAt(200000, 500000), pairAt(200100, 500000),
                                          pairAt(200000, 500100)};
  const GeorefQuality::Report r = GeorefQuality::assess(p, false);
  QVERIFY(!r.measurable);
  QVERIFY(r.residuals.isEmpty());
  QVERIFY2(!GeorefQuality::rowText(r, 0).contains(QStringLiteral("어긋남")),
           "3 points always fit exactly; a 0.00 m residual would mislead");
  QVERIFY(GeorefQuality::notes(r).contains(QStringLiteral("점 4개부터 점마다 어긋남을 잽니다")));
  QVERIFY(GeorefQuality::summary(r).contains(QStringLiteral("한 점 더")));
}

void TestGeorefQuality::anisotropyAndShear_areWarned() {
  // x keeps its scale, y is stretched 20 %: a wrongly clicked third point looks like this.
  QVector<GeorefService::Pair> p = {{0, 0, 1000, 2000}, {100, 0, 1100, 2000}, {0, 100, 1000, 2120}};
  GeorefQuality::Report r = GeorefQuality::assess(p, false);
  QVERIFY(r.shape.valid);
  QVERIFY2(std::abs(r.shape.anisotropy - 1.2) < 1e-9, qPrintable(QString::number(r.shape.anisotropy)));
  QVERIFY(GeorefQuality::notes(r).join('|').contains(QStringLiteral("20% 다릅니다")));

  p = {{0, 0, 1000, 2000}, {100, 0, 1100, 2000}, {0, 100, 1020, 2100}};  // 11.3 degree shear
  r = GeorefQuality::assess(p, false);
  QVERIFY2(r.shape.shearDeg > 10.0 && r.shape.shearDeg < 12.0, qPrintable(QString::number(r.shape.shearDeg)));
  QVERIFY(GeorefQuality::notes(r).join('|').contains(QStringLiteral("비뚤어집니다")));
}

void TestGeorefQuality::similarity_hasNoWarnings() {
  // Scan pixels (rows grow down) onto the map: rotation + scale + flip only.
  QVector<GeorefService::Pair> p;
  const double th = 0.2, k = 0.05;
  for (const QPointF& s : {QPointF(0, 0), QPointF(4000, 0), QPointF(4000, 3000), QPointF(0, 3000)}) {
    GeorefService::Pair pr;
    pr.srcX = s.x();
    pr.srcY = s.y();
    pr.mapX = 194000 + k * (std::cos(th) * s.x() + std::sin(th) * s.y());
    pr.mapY = 574000 + k * (std::sin(th) * s.x() - std::cos(th) * s.y());
    p.append(pr);
  }
  const GeorefQuality::Report r = GeorefQuality::assess(p, true);
  QVERIFY(r.measurable);
  QVERIFY2(r.maxResidual < 1e-6, qPrintable(QString::number(r.maxResidual)));
  QVERIFY2(std::abs(r.shape.anisotropy - 1.0) < 1e-9, qPrintable(QString::number(r.shape.anisotropy)));
  QVERIFY2(r.shape.shearDeg < 1e-6, qPrintable(QString::number(r.shape.shearDeg)));
  QVERIFY2(GeorefQuality::notes(r).isEmpty(), qPrintable(GeorefQuality::notes(r).join('|')));
}

#include "test_georef_quality.moc"

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  const QString prefix = qEnvironmentVariable(
      "QGIS_PREFIX_PATH", QFile::exists(QStringLiteral("D:/OSGeo4W/apps/qgis-dev"))
                              ? QStringLiteral("D:/OSGeo4W/apps/qgis-dev")
                              : QStringLiteral("C:/OSGeo4W/apps/qgis-dev"));
  QgsApplication::setPrefixPath(prefix, true);
  QgsApplication::initQgis();
  TestGeorefQuality tc;
  const int rc = QTest::qExec(&tc, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
