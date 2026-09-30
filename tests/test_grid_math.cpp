#include "core/CanvasGridMath.h"
#include "core/TrenchGridGenerator.h"

#include <QtTest>

#include <cmath>

namespace {
constexpr double kPi = 3.14159265358979323846;
bool near(double a, double b, double tolerance = 1e-9) { return std::abs(a - b) <= tolerance; }
}  // namespace

class TestGridMath : public QObject {
  Q_OBJECT
private slots:
  void rotationIsClockwiseFromEast();
  void gridFrameMatchesTrenchAzimuth();
  void roundTripKeepsPoints();
  void snapHonoursOriginAndRotation();
  void denseGridsAreThinnedNotCut();
  void labelsKeepSubMetreSteps();
  void niceStepsAreUnchanged();
};

void TestGridMath::rotationIsClockwiseFromEast() {
  // The tooltip says "동쪽 기준 시계 방향": turning east clockwise by 90 degrees points south.
  double x = 0, y = 0;
  CanvasGridMath::toMap({0, 0, 90}, 1, 0, &x, &y);
  QVERIFY2(near(x, 0) && near(y, -1), qPrintable(QStringLiteral("%1 %2").arg(x).arg(y)));
  CanvasGridMath::toMap({0, 0, 30}, 1, 0, &x, &y);
  QVERIFY(near(x, std::cos(kPi / 6)) && near(y, -std::sin(kPi / 6)));
  QVERIFY(!CanvasGridMath::isTurned({0, 0, 0}));
  QVERIFY(!CanvasGridMath::isTurned({0, 0, 360}));
  QVERIFY(CanvasGridMath::isTurned({0, 0, 0.5}));
}

void TestGridMath::gridFrameMatchesTrenchAzimuth() {
  TrenchGridGenerator::Spec spec;
  spec.originX = 200123.5;
  spec.originY = 551234.25;
  spec.azimuthDeg = 30;
  spec.trenchWidth = 2;
  spec.trenchLength = 20;
  const auto cells = TrenchGridGenerator::build(spec);
  QCOMPARE(static_cast<int>(cells.size()), 1);
  const CanvasGridMath::Frame frame{spec.originX, spec.originY, spec.azimuthDeg};
  double x = 0, y = 0;
  CanvasGridMath::toMap(frame, spec.trenchWidth, 0, &x, &y);
  QVERIFY(near(x, cells[0].ring[1].first, 1e-6) && near(y, cells[0].ring[1].second, 1e-6));
  CanvasGridMath::toMap(frame, 0, spec.trenchLength, &x, &y);
  QVERIFY(near(x, cells[0].ring[3].first, 1e-6) && near(y, cells[0].ring[3].second, 1e-6));
}

void TestGridMath::roundTripKeepsPoints() {
  const CanvasGridMath::Frame frame{210000.0, 450000.0, 37.5};
  for (const auto& point : {std::pair{210012.3, 450001.7}, std::pair{209000.0, 451000.0}}) {
    double u = 0, v = 0, x = 0, y = 0;
    CanvasGridMath::toGrid(frame, point.first, point.second, &u, &v);
    CanvasGridMath::toMap(frame, u, v, &x, &y);
    QVERIFY(near(x, point.first, 1e-6) && near(y, point.second, 1e-6));
  }
  double u = 0, v = 0;
  CanvasGridMath::toGrid(frame, frame.originX, frame.originY, &u, &v);
  QVERIFY(near(u, 0) && near(v, 0));
}

void TestGridMath::snapHonoursOriginAndRotation() {
  double x = 1.1, y = 0.9;
  CanvasGridMath::snapToNode({0.3, 0.2, 0}, 0.5, &x, &y);
  QVERIFY2(near(x, 1.3) && near(y, 0.7), qPrintable(QStringLiteral("%1 %2").arg(x).arg(y)));
  const CanvasGridMath::Frame turned{100.0, 200.0, 25.0};
  x = 117.3;
  y = 188.1;
  CanvasGridMath::snapToNode(turned, 2.0, &x, &y);
  double u = 0, v = 0;
  CanvasGridMath::toGrid(turned, x, y, &u, &v);
  QVERIFY(near(std::remainder(u, 2.0), 0, 1e-6) && near(std::remainder(v, 2.0), 0, 1e-6));
  x = 5.0;
  y = 6.0;
  CanvasGridMath::snapToNode(turned, 0.0, &x, &y);
  QVERIFY(near(x, 5.0) && near(y, 6.0));
}

void TestGridMath::denseGridsAreThinnedNotCut() {
  QCOMPARE(CanvasGridMath::lineStride(50, 1, 80), 1);
  QCOMPARE(CanvasGridMath::lineStride(79, 1, 80), 1);
  // 201 lines of 0.5 m over 100 m: every third line keeps the whole view covered.
  QCOMPARE(CanvasGridMath::lineStride(100, 0.5, 80), 3);
  QCOMPARE(CanvasGridMath::lineStride(0, 1, 80), 1);
  QCOMPARE(CanvasGridMath::lineStride(100, 0, 80), 1);
}

void TestGridMath::labelsKeepSubMetreSteps() {
  QCOMPARE(CanvasGridMath::labelDecimals(20), 0);
  QCOMPARE(CanvasGridMath::labelDecimals(0.5), 1);
  QCOMPARE(CanvasGridMath::labelDecimals(0.1), 1);
  QCOMPARE(CanvasGridMath::labelDecimals(0.25), 2);
  QCOMPARE(CanvasGridMath::labelDecimals(1, 200000.25), 2);
  QCOMPARE(CanvasGridMath::labelDecimals(1, 0.125), 3);
}

void TestGridMath::niceStepsAreUnchanged() {
  QCOMPARE(CanvasGridMath::niceStepMeters(120.0), 100.0);
  QCOMPARE(CanvasGridMath::niceStepMeters(8.0), 10.0);
}

QTEST_APPLESS_MAIN(TestGridMath)
#include "test_grid_math.moc"
