// PolygonPieces: inner pieces of one polygon feature (a hole, or a part inside another part).
// An imported survey area carries its inner outlines this way; they are found under a point
// and taken out without touching the rest of the shape.
#include <QtTest>
#include <QFile>

#include "core/FeaturePick.h"
#include "core/LayerOps.h"
#include "core/PolygonPieces.h"

#include <algorithm>

#include <qgsapplication.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsgeometry.h>
#include <qgsproject.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorlayer.h>

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

  void pickListKeepsSurveyShapesAndFindsHolesCutBeforeSaving() {
    // The list 도형선택 cycles through (core/FeaturePick): survey shapes only where any lies
    // under the click, and the hole of a shape changed in the edit buffer (not saved yet) is a
    // pick of its own. A filter-rect request alone skips that shape when the click is in its hole.
    QgsProject project;
    auto* reference = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"), QStringLiteral("참고 면"),
                                         QStringLiteral("memory"));
    LayerOps::markReferenceLayer(reference);
    QgsFeature cover(reference->fields());
    cover.setGeometry(QgsGeometry::fromPolygonXY({ring(199950, 449950, 200)}));
    reference->dataProvider()->addFeature(cover);
    project.addMapLayer(reference);
    auto* layer = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"), QStringLiteral("조사구역"),
                                     QStringLiteral("memory"));
    LayerOps::markSurveyLayer(layer, QStringLiteral("survey_area"));
    QgsFeature area(layer->fields());
    area.setGeometry(QgsGeometry::fromPolygonXY({ring(200010, 450010, 60)}));
    layer->dataProvider()->addFeature(area);
    project.addMapLayer(layer);
    const QgsFeatureId outer = *layer->allFeatureIds().constBegin();
    const QList<QgsMapLayer*> layers{reference, layer};
    const auto list = [&](double x, double y) {
      return FeaturePick::candidates(layers, QgsPointXY(x, y), 0.5,
                                     QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")),
                                     project.transformContext());
    };
    const auto hasReference = [&](const QList<FeaturePick::Hit>& hits) {
      return std::any_of(hits.cbegin(), hits.cend(), [&](const FeaturePick::Hit& hit) { return hit.layer == reference; });
    };

    QList<FeaturePick::Hit> hits = list(200015, 450015);
    QCOMPARE(hits.size(), 1);
    QVERIFY(hits.first().layer == layer && hits.first().fid == outer && !hits.first().piece.isValid());
    QVERIFY(list(200500, 450500).isEmpty());
    hits = list(200120, 450120);  // nothing of the survey there: the reference shape may be picked
    QCOMPARE(hits.size(), 1);
    QVERIFY(hits.first().layer == reference);
    QVERIFY(!FeaturePick::isSurveyLayer(reference));
    QVERIFY(FeaturePick::isSurveyLayer(layer));

    QVERIFY(layer->startEditing());
    QgsGeometry cut = holed();
    QVERIFY(layer->changeGeometry(outer, cut));
    hits = list(200035, 450035);
    QVERIFY2(!hits.isEmpty(), "the hole cut before saving was not found");
    QVERIFY(hits.first().layer == layer && hits.first().fid == outer);
    QCOMPARE(hits.first().piece, (Piece{0, 1}));
    QVERIFY2(!hasReference(hits), "a survey shape lies under the click; the reference shape stays out of the list");
    QVERIFY(FeaturePick::at(layers, QgsPointXY(200035, 450035), 0.5,
                            QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")), project.transformContext())
                .piece.isHole());
    QVERIFY(layer->rollBack());
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
