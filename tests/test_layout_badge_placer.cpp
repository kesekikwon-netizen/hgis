#include <QtTest>
#include <QPainterPath>
#include <QSet>
#include <QTransform>

#include <cmath>

#include <qgspointxy.h>

#include "core/LayoutBadgePlacer.h"

// 요청 기록 docs/intent/2026-10-03-layout-number-cluster-rows.md:
// 번호는 제 유적에 붙고, 겹칠 때만 가장 가까운 빈자리로 옮기며, 지시선은 서로 엇갈리지 않는다.
class LayoutBadgePlacerTest : public QObject {
  Q_OBJECT

  static constexpr double kStep = 10.0;
  static constexpr double kBadgeMm = 3.0;

  // Sites packed inside a fifth of a step around one spot: no two badges fit on them.
  static QVector<QgsPointXY> crowd(const QgsPointXY& hub, int count) {
    QVector<QgsPointXY> sites;
    for (int i = 0; i < count; ++i)
      sites.append(QgsPointXY(hub.x() + (i % 3) * 1.0, hub.y() + (i / 3) * 0.5));
    return sites;
  }

  static void verifyClear(const QVector<QgsPointXY>& placed, double step) {
    for (int i = 0; i < placed.size(); ++i) {
      for (int j = i + 1; j < placed.size(); ++j)
        QVERIFY2(placed.at(i).distance(placed.at(j)) >= step * 0.96 - 1e-6, "number circles must not cover each other");
    }
  }

private slots:
  void loneSitesKeepTheirBadgeOnTheSite() {
    LayoutBadgePlacer placer(QTransform(), QPainterPath(), kStep);
    const QVector<QgsPointXY> sites{{0., 0.}, {100., 0.}, {0., 100.}};
    const QVector<QgsPointXY> placed = placer.placeAll(sites, QVector<double>(sites.size(), kBadgeMm));
    QCOMPARE(placed, sites);
  }

  void smallCrowdStaysOnShortRings() {
    LayoutBadgePlacer placer(QTransform(), QPainterPath(), kStep);
    const QVector<QgsPointXY> sites(4, QgsPointXY(50., 50.));
    const QVector<QgsPointXY> placed = placer.placeAll(sites, QVector<double>(sites.size(), kBadgeMm));
    QCOMPARE(placed.size(), 4);
    QCOMPARE(placed.first(), sites.first());
    for (const QgsPointXY& at : placed)
      QVERIFY2(at.distance(sites.first()) <= kStep + 1e-6, "a few stacked sites keep their badges one step away");
    verifyClear(placed, kStep);
  }

  void crowdBadgesStayAttachedToTheirSites() {
    LayoutBadgePlacer placer(QTransform(), QPainterPath(), kStep);
    const QgsPointXY hub(100., 100.);
    const QVector<QgsPointXY> sites = crowd(hub, 12);
    const QVector<QgsPointXY> placed = placer.placeAll(sites, QVector<double>(sites.size(), kBadgeMm));
    QCOMPARE(placed.size(), sites.size());
    verifyClear(placed, kStep);
    int onSite = 0;
    for (int i = 0; i < sites.size(); ++i) {
      // One badge on the spot, six around it, the rest on the second ring: nothing farther.
      QVERIFY2(placed.at(i).distance(sites.at(i)) <= kStep * 2.3, "a badge moves only as far as overlap forces it");
      if (placed.at(i).distance(sites.at(i)) < 1e-6) ++onSite;
    }
    QVERIFY2(onSite >= 1, "the first badge stays on its own site");
  }

  void crowdAtThePaperCornerStaysOnThePaper() {
    // Map units are millimetres here. Paper 100 x 100, crowd 3 mm from the corner.
    QPainterPath paper;
    paper.addRect(QRectF(0., 0., 100., 100.));
    LayoutBadgePlacer placer(QTransform(), paper, 4.6);
    const QVector<QgsPointXY> sites = crowd(QgsPointXY(95., 96.), 20);
    const QVector<QgsPointXY> placed = placer.placeAll(sites, QVector<double>(sites.size(), kBadgeMm));
    QCOMPARE(placed.size(), sites.size());
    verifyClear(placed, 4.6);
    for (const QgsPointXY& at : placed) {
      QVERIFY2(paper.contains(QRectF(at.x() - 1.5, at.y() - 1.5, 3., 3.)), "every badge sits fully on the paper");
      // A quarter disc of 20 badges around the corner reaches about five steps.
      QVERIFY2(at.distance(QgsPointXY(96., 97.)) <= 4.6 * 6.5, "the badges stay next to the crowd at the corner");
    }
  }

  void loneSiteNextToAFewStackedSitesKeepsItsOwnSite() {
    LayoutBadgePlacer placer(QTransform(), QPainterPath(), kStep);
    QVector<QgsPointXY> sites(4, QgsPointXY(0., 0.));
    const QgsPointXY lone(kStep, 0.);
    sites.append(lone);
    const QVector<QgsPointXY> placed = placer.placeAll(sites, QVector<double>(sites.size(), kBadgeMm));
    QCOMPARE(placed.last(), lone);
    verifyClear(placed, kStep);
  }

  // Proper crossing of two leader lines (site -> badge); shared end points do not count.
  static bool crosses(const QgsPointXY& a, const QgsPointXY& b, const QgsPointXY& c, const QgsPointXY& d) {
    auto side = [](const QgsPointXY& p, const QgsPointXY& q, const QgsPointXY& r) {
      return (q.x() - p.x()) * (r.y() - p.y()) - (q.y() - p.y()) * (r.x() - p.x());
    };
    return side(a, b, c) * side(a, b, d) < -1e-9 && side(c, d, a) * side(c, d, b) < -1e-9;
  }

  void crowdLeaderLinesDoNotCrossEachOther() {
    // 16 sites scattered over about one step and a half, numbered in no spatial order,
    // next to the paper's right edge as in the 2026-10-04 screenshot.
    QPainterPath paper;
    paper.addRect(QRectF(0., 0., 100., 200.));
    LayoutBadgePlacer placer(QTransform(), paper, 4.6);
    QVector<QgsPointXY> sites;
    quint32 seed = 7;
    for (int i = 0; i < 16; ++i) {
      seed = seed * 1664525u + 1013904223u;
      const double x = 90. + (seed >> 8) % 700 / 100.;
      seed = seed * 1664525u + 1013904223u;
      sites.append(QgsPointXY(x, 97. + (seed >> 8) % 700 / 100.));
    }
    const QVector<QgsPointXY> placed = placer.placeAll(sites, QVector<double>(sites.size(), kBadgeMm));
    verifyClear(placed, 4.6);
    for (int i = 0; i < sites.size(); ++i) {
      QVERIFY2(paper.contains(QRectF(placed.at(i).x() - 1.5, placed.at(i).y() - 1.5, 3., 3.)), "every badge sits fully on the paper");
      for (int j = i + 1; j < sites.size(); ++j)
        QVERIFY2(!crosses(sites.at(i), placed.at(i), sites.at(j), placed.at(j)),
                 qPrintable(QStringLiteral("leader lines %1 and %2 cross").arg(i).arg(j)));
    }
  }

  void leaderLinesOfNeighbouringGroupsDoNotCross() {
    // 2026-10-04 screenshot: about 16 sites spread over 1-3 cm at the paper's right edge. They are
    // not all chained into one crowd, and the spilled badges of one group crossed another's lines.
    QPainterPath paper;
    paper.addRect(QRectF(0., 0., 100., 200.));
    for (quint32 round = 1; round <= 60; ++round) {
      LayoutBadgePlacer placer(QTransform(), paper, 4.6);
      QVector<QgsPointXY> sites;
      quint32 seed = round;
      const int spread = 1000 * (1 + round % 3);
      for (int i = 0; i < 16; ++i) {
        seed = seed * 1664525u + 1013904223u;
        const double x = 96. - (seed >> 8) % spread / 100.;
        seed = seed * 1664525u + 1013904223u;
        sites.append(QgsPointXY(x, 80. + (seed >> 8) % (spread * 13 / 10) / 100.));
      }
      const QVector<QgsPointXY> placed = placer.placeAll(sites, QVector<double>(sites.size(), kBadgeMm));
      verifyClear(placed, 4.6);
      for (int i = 0; i < sites.size(); ++i) {
        for (int j = i + 1; j < sites.size(); ++j)
          QVERIFY2(!crosses(sites.at(i), placed.at(i), sites.at(j), placed.at(j)),
                   qPrintable(QStringLiteral("round %1: leader lines %2 and %3 cross").arg(round).arg(i).arg(j)));
      }
    }
  }

  void loneSiteNextToACrowdKeepsItsOwnSite() {
    LayoutBadgePlacer placer(QTransform(), QPainterPath(), kStep);
    QVector<QgsPointXY> sites = crowd(QgsPointXY(100., 100.), 9);
    const QgsPointXY lone(100., 130.);
    sites.append(lone);
    const QVector<QgsPointXY> placed = placer.placeAll(sites, QVector<double>(sites.size(), kBadgeMm));
    QCOMPARE(placed.last(), lone);
    verifyClear(placed, kStep);
  }
};

QTEST_APPLESS_MAIN(LayoutBadgePlacerTest)
#include "test_layout_badge_placer.moc"
