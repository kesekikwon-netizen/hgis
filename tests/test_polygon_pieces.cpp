// PolygonPieces: inner pieces of one polygon feature (a hole, or a part inside another part).
// An imported survey area carries its inner outlines this way; they are found under a point
// and taken out without touching the rest of the shape.
#include <QtTest>
#include <QFile>

#include "core/PolygonPieces.h"

#include <qgsapplication.h>
#include <qgsgeometry.h>

namespace {
using PolygonPieces::Piece;

QgsPolylineXY ring(double x, double y, double size) {
  return {{x, y}, {x + size, y}, {x + size, y + size}, {x, y + size}, {x, y}};
}
// A 60 m square with a 10 m hole at (30,30).
QgsGeometry holed() { return QgsGeometry::fromPolygonXY({ring(200010, 450010, 60), ring(200030, 450030, 10)}); }
// The same outlines stored as two overlapping parts of one shape.
QgsGeometry nested() {
  return QgsGeometry::fromMultiPolygonXY({{ring(200010, 450010, 60)}, {ring(200030, 450030, 10)}});
}
int ringCount(const QgsGeometry& g) {
  int rings = 0;
  const QgsMultiPolygonXY parts = g.isMultipart() ? g.asMultiPolygon() : QgsMultiPolygonXY{g.asPolygon()};
  for (const QgsPolygonXY& part : parts) rings += part.size();
  return rings;
}
}  // namespace

class TestPolygonPieces : public QObject {
  Q_OBJECT
private slots:
  void holeIsFoundAndFilled() {
    const QgsGeometry g = holed();
    QCOMPARE(PolygonPieces::innerPieceAt(g, QgsPointXY(200035, 450035), 0.5), std::optional<Piece>(Piece{0, 1}));
    QVERIFY2(!PolygonPieces::innerPieceAt(g, QgsPointXY(200015, 450015), 0.5), "the plain outer area is no piece");
    // A click on the inner outline itself, just on the filled side, still means the hole.
    QCOMPARE(PolygonPieces::innerPieceAt(g, QgsPointXY(200029.8, 450035), 0.5), std::optional<Piece>(Piece{0, 1}));
    QVERIFY(!PolygonPieces::innerPieceAt(g, QgsPointXY(200029.8, 450035), 0.1));
    QVERIFY(qFuzzyCompare(PolygonPieces::outline(g, Piece{0, 1}).area(), 100.0));

    const QgsGeometry filled = PolygonPieces::withoutPiece(g, Piece{0, 1});
    QVERIFY(qFuzzyCompare(filled.area(), 3600.0));
    QCOMPARE(ringCount(filled), 1);
    QVERIFY2(!filled.isMultipart(), "the geometry type is kept");
    QVERIFY2(PolygonPieces::withPieceCutOut(g, Piece{0, 1}).isNull(), "a hole is already cut out");
  }

  void innerPartIsRemovedOrCutOut() {
    const QgsGeometry g = nested();
    const auto piece = PolygonPieces::innerPieceAt(g, QgsPointXY(200035, 450035), 0.5);
    QCOMPARE(piece, std::optional<Piece>(Piece{1, 0}));
    QVERIFY(!PolygonPieces::innerPieceAt(g, QgsPointXY(200015, 450015), 0.5));

    const QgsGeometry without = PolygonPieces::withoutPiece(g, *piece);
    QVERIFY(without.isMultipart());
    QCOMPARE(without.asMultiPolygon().size(), 1);
    QVERIFY(qFuzzyCompare(without.area(), 3600.0));

    const QgsGeometry cut = PolygonPieces::withPieceCutOut(g, *piece);
    QVERIFY2(cut.isMultipart(), "the layer's geometry type is kept");
    QVERIFY(qFuzzyCompare(cut.area(), 3500.0));
    QCOMPARE(ringCount(cut), 2);
    QVERIFY(cut.isGeosValid());
  }

  void smallestPieceWinsWhenPartsAreStacked() {
    // Three parts, each inside the one before: the innermost is the one under the cursor.
    const QgsGeometry g = QgsGeometry::fromMultiPolygonXY(
        {{ring(200010, 450010, 60)}, {ring(200020, 450020, 30)}, {ring(200030, 450030, 10)}});
    QCOMPARE(PolygonPieces::innerPieceAt(g, QgsPointXY(200035, 450035), 0.5), std::optional<Piece>(Piece{2, 0}));
    QCOMPARE(PolygonPieces::innerPieceAt(g, QgsPointXY(200023, 450023), 0.5), std::optional<Piece>(Piece{1, 0}));
    QCOMPARE(PolygonPieces::withoutPiece(g, Piece{1, 0}).asMultiPolygon().size(), 2);
  }

  void separatePartsAndStalePiecesAreLeftAlone() {
    const QgsGeometry apart =
        QgsGeometry::fromMultiPolygonXY({{ring(200010, 450010, 20)}, {ring(200050, 450010, 20)}});
    QVERIFY2(!PolygonPieces::innerPieceAt(apart, QgsPointXY(200055, 450015), 0.5),
             "two zones of one survey area are not inner pieces of each other");
    QVERIFY2(PolygonPieces::withoutPiece(QgsGeometry::fromPolygonXY({ring(200010, 450010, 60)}), Piece{0, 0}).isNull(),
             "the only part is the shape itself");
    QVERIFY(PolygonPieces::withoutPiece(holed(), Piece{0, 2}).isNull());
    QVERIFY(PolygonPieces::withoutPiece(holed(), Piece{}).isNull());
    QVERIFY(PolygonPieces::outline(holed(), Piece{3, 0}).isNull());
    QVERIFY(PolygonPieces::withPieceCutOut(apart, Piece{5, 0}).isNull());
    const QgsGeometry line = QgsGeometry::fromPolylineXY({{200010, 450010}, {200050, 450050}});
    QVERIFY(!PolygonPieces::innerPieceAt(line, QgsPointXY(200030, 450030), 1.0));
    QVERIFY(!PolygonPieces::innerPieceAt(QgsGeometry(), QgsPointXY(200030, 450030), 1.0));
  }

  void islandGoesWithItsHole() {
    // Outer square with a 30 m hole, and a 10 m island standing in that hole.
    const QgsGeometry g = QgsGeometry::fromMultiPolygonXY(
        {{ring(200010, 450010, 60), ring(200020, 450020, 30)}, {ring(200030, 450030, 10)}});
    QCOMPARE(PolygonPieces::innerPieceAt(g, QgsPointXY(200035, 450035), 0.5), std::optional<Piece>(Piece{1, 0}));
    QCOMPARE(PolygonPieces::innerPieceAt(g, QgsPointXY(200023, 450023), 0.5), std::optional<Piece>(Piece{0, 1}));
    const QgsGeometry filled = PolygonPieces::withoutPiece(g, Piece{0, 1});
    QCOMPARE(filled.asMultiPolygon().size(), 1);
    QCOMPARE(ringCount(filled), 1);
    QVERIFY(qFuzzyCompare(filled.area(), 3600.0));
    // Taking only the island leaves the hole as it was.
    const QgsGeometry noIsland = PolygonPieces::withoutPiece(g, Piece{1, 0});
    QCOMPARE(ringCount(noIsland), 2);
    QVERIFY(qFuzzyCompare(noIsland.area(), 2700.0));
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  const QString prefix = qEnvironmentVariable(
      "QGIS_PREFIX_PATH", QFile::exists(QStringLiteral("A:/OSGeo4W/apps/qgis-dev"))
                              ? QStringLiteral("A:/OSGeo4W/apps/qgis-dev")
                              : QStringLiteral("C:/OSGeo4W/apps/qgis-dev"));
  QgsApplication::setPrefixPath(prefix, true);
  QgsApplication::initQgis();
  TestPolygonPieces tc;
  const int rc = QTest::qExec(&tc, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}

#include "test_polygon_pieces.moc"
