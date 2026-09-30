// Trench generator contracts added by package B2: an empty grid always comes with a reason
// (F047), clearLayer does not report success for a file it could not open, the plan cache
// returns exactly the generator's 10%/2% plan while searching once per key (F097), and a
// re-generation is one Ctrl+Z step that keeps hand edits (F003/F156).
#include <memory>

#include <QtTest>
#include <QFile>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QUndoStack>

#include "core/LayerOps.h"
#include "core/TrenchGridGenerator.h"
#include "core/TrenchLayerEdit.h"
#include "core/TrenchPlanCache.h"

#include <gdal_priv.h>
#include <ogr_geometry.h>
#include <qgsapplication.h>
#include <qgsproject.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorlayer.h>

namespace {
using TrenchLayerEdit::PlaceOutcome;

bool emptyGpkg(const QString& path) {
  GDALAllRegister();
  GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GPKG");
  GDALDataset* dataset = driver ? driver->Create(path.toUtf8().constData(), 0, 0, 0, GDT_Unknown, nullptr) : nullptr;
  if (!dataset) return false;
  GDALClose(dataset);
  return true;
}

struct GeometryDeleter {
  void operator()(OGRGeometry* geometry) const { OGRGeometryFactory::destroyGeometry(geometry); }
};

QByteArray wktWkb(const char* wkt) {
  OGRGeometry* geometry = nullptr;
  OGRGeometryFactory::createFromWkt(wkt, nullptr, &geometry);
  std::unique_ptr<OGRGeometry, GeometryDeleter> owner(geometry);
  if (!owner) return {};
  QByteArray result(int(owner->WkbSize()), '\0');
  owner->exportToWkb(wkbNDR, reinterpret_cast<unsigned char*>(result.data()));
  return result;
}

QByteArray squareWkb(double x0, double y0, double x1, double y1) {
  return wktWkb(QStringLiteral("POLYGON((%1 %2,%3 %2,%3 %4,%1 %4,%1 %2))")
                    .arg(x0, 0, 'f', 6).arg(y0, 0, 'f', 6).arg(x1, 0, 'f', 6).arg(y1, 0, 'f', 6)
                    .toLatin1().constData());
}
}  // namespace

class TestTrenchPlan : public QObject {
  Q_OBJECT
private slots:
  void cleanup() { QgsProject::instance()->removeAllMapLayers(); }

  // A self-intersecting boundary (vertex edit gone wrong) yields no grid AND a reason.
  void invalidAreaExplainsWhyGridIsEmpty() {
    const QByteArray bowtie = wktWkb("POLYGON((0 0,100 100,100 0,0 100,0 0))");
    QVERIFY(!bowtie.isEmpty());
    TrenchGridGenerator::Spec spec;
    spec.balkWidth = 10.0;
    QString reason;
    QVERIFY(TrenchGridGenerator::buildInArea(spec, bowtie, &reason).empty());
    QVERIFY2(reason.contains(QStringLiteral("유효하지 않은")), qPrintable(reason));
    const auto plan = TrenchGridGenerator::buildForTargetRatio(bowtie, 10.0);
    QVERIFY(plan.cells.empty());
    QCOMPARE(plan.error, reason);
  }

  void everyEmptyGridHasAReason() {
    TrenchGridGenerator::Spec spec;
    spec.balkWidth = 10.0;
    QString reason;
    const QByteArray square = squareWkb(0.0, 0.0, 100.0, 100.0);
    QVERIFY(!TrenchGridGenerator::buildInArea(spec, square, &reason).empty());
    QVERIFY(reason.isEmpty());
    // Nothing fits in a 1.5 m wide strip.
    QVERIFY(TrenchGridGenerator::buildInArea(spec, squareWkb(0, 0, 1.5, 12), &reason).empty());
    QVERIFY(!reason.isEmpty());
    QVERIFY(TrenchGridGenerator::buildInArea(spec, QByteArray(), &reason).empty());
    QVERIFY(!reason.isEmpty());
    spec.trenchWidth = 2.5;  // over the 2 m limit
    QVERIFY(TrenchGridGenerator::buildInArea(spec, square, &reason).empty());
    QVERIFY(reason.contains(QStringLiteral("2 m")));
  }

  void cellsValidMatchesWriterRules() {
    TrenchGridGenerator::Spec spec;
    spec.cols = 2;
    auto cells = TrenchGridGenerator::build(spec);
    QString error;
    QVERIFY(TrenchGridGenerator::cellsValid(cells, &error));
    QVERIFY(error.isEmpty());
    cells[1].length = 21.0;
    QVERIFY(!TrenchGridGenerator::cellsValid(cells, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!TrenchGridGenerator::cellsValid({}, &error));
  }

  // A missing file has nothing to clear; a file that is there but cannot be opened is a failure.
  void clearLayerReportsUnreadableFile() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QString err;
    QVERIFY(TrenchGridGenerator::clearLayer(tmp.filePath(QStringLiteral("none.gpkg")),
                                            QStringLiteral("trial_trench"), &err));
    QVERIFY(err.isEmpty());
    const QString broken = tmp.filePath(QStringLiteral("broken.gpkg"));
    QFile file(broken);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("not a geopackage");
    file.close();
    QVERIFY(!TrenchGridGenerator::clearLayer(broken, QStringLiteral("trial_trench"), &err));
    QVERIFY(!err.isEmpty());
  }

  // The cache returns exactly the generator's plan (exact 10% contract) and searches once per key.
  void planCacheReturnsGeneratorResult() {
    TrenchPlanCache::clear();
    const QByteArray area = squareWkb(0.0, 0.0, 100.0, 100.0);
    const auto first = TrenchPlanCache::ratioPlan(area, 10.0, 2.0, 0.0);
    QCOMPARE(TrenchPlanCache::computeCount(), 1);
    const auto again = TrenchPlanCache::ratioPlan(area, 10.0, 2.0, 0.0);
    QCOMPARE(TrenchPlanCache::computeCount(), 1);
    TrenchGridGenerator::RatioFill known;
    QVERIFY(TrenchPlanCache::lookup(area, 10.0, 2.0, 0.0, &known));
    QVERIFY(!TrenchPlanCache::lookup(area, 10.0, 2.0, 30.0, &known));
    const auto direct = TrenchGridGenerator::buildForTargetRatio(area, 10.0, 2.0, 0.0);
    QVERIFY(!direct.cells.empty());
    QCOMPARE(first.cells.size(), direct.cells.size());
    QCOMPARE(again.cells.size(), direct.cells.size());
    for (size_t i = 0; i < direct.cells.size(); ++i) QVERIFY(first.cells[i].ring == direct.cells[i].ring);
    QCOMPARE(first.ratioPct, direct.ratioPct);
    QVERIFY(qAbs(TrenchGridGenerator::totalArea(first.cells) - 1000.0) < 1e-6);
    TrenchPlanCache::ratioPlan(area, 2.0, 2.0, 0.0);  // another target is another search
    QCOMPARE(TrenchPlanCache::computeCount(), 2);
    TrenchPlanCache::clear();
    QVERIFY(!TrenchPlanCache::lookup(area, 10.0, 2.0, 0.0, &known));
  }

  // F003/F156: the first grid is written and remembered; a new grid replaces it as one
  // Ctrl+Z step without writing; unsaved hand edits block it; saved hand edits ask first.
  void placeGridKeepsHandEditsAndUndoesReplacement() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString gpkg = tmp.filePath(QStringLiteral("survey.gpkg"));
    QVERIFY(emptyGpkg(gpkg));
    QgsProject* project = QgsProject::instance();
    // Release the survey file before the temporary folder goes, even when a check fails.
    const auto dropLayers = qScopeGuard([project] { project->removeAllMapLayers(); });
    const auto load = [&]() {
      return LayerOps::ensureDomainLayer(project, gpkg, QStringLiteral("trial_trench"), QStringLiteral("시굴격자"));
    };
    int asked = 0;
    bool answer = false;
    const auto confirm = [&](qint64 count) {
      ++asked;
      return count > 0 && answer;
    };
    const QString crs = QStringLiteral("EPSG:5186");
    TrenchGridGenerator::Spec spec;
    spec.originX = 200000.0;
    spec.originY = 450000.0;
    spec.cols = 2;
    spec.trenchLength = 10.0;
    spec.balkWidth = 5.0;
    const auto twoCells = TrenchGridGenerator::build(spec);
    auto placed = TrenchLayerEdit::placeGrid(project, gpkg, twoCells, crs, load, confirm);
    QVERIFY2(placed.outcome == PlaceOutcome::Placed, qPrintable(placed.message + placed.detail));
    QgsVectorLayer* layer = placed.layer;
    QVERIFY(layer);
    QCOMPARE(layer->featureCount(), 2);
    QVERIFY(!layer->isModified());

    // An untouched grid is replaced without a question, as one undo step, nothing written.
    spec.cols = 3;
    placed = TrenchLayerEdit::placeGrid(project, gpkg, TrenchGridGenerator::build(spec), crs, load, confirm);
    QVERIFY2(placed.outcome == PlaceOutcome::Placed, qPrintable(placed.message));
    QCOMPARE(placed.layer, layer);
    QCOMPARE(asked, 0);
    QCOMPARE(layer->featureCount(), 3);
    QCOMPARE(layer->dataProvider()->featureCount(), 2);
    QCOMPARE(layer->undoStack()->count(), 1);

    // Reopening the dialog plans the same grid: kept as it is, no second undo step.
    placed = TrenchLayerEdit::placeGrid(project, gpkg, TrenchGridGenerator::build(spec), crs, load, confirm);
    QVERIFY(placed.outcome == PlaceOutcome::Kept);
    QVERIFY2(placed.message.contains(QStringLiteral("같은 시굴격자")), qPrintable(placed.message));
    QCOMPARE(asked, 0);
    QCOMPARE(layer->undoStack()->count(), 1);

    // An unsaved hand move blocks the re-generation and stays as it is.
    const QgsFeatureId one = *layer->allFeatureIds().constBegin();
    QVERIFY(TrenchLayerEdit::translate(layer, {one}, 1.0, 0.0, QStringLiteral("트렌치 이동")));
    const QString moved = TrenchLayerEdit::layoutSignature(layer);
    placed = TrenchLayerEdit::placeGrid(project, gpkg, twoCells, crs, load, confirm);
    QVERIFY(placed.outcome == PlaceOutcome::Failed);
    QVERIFY2(placed.message.contains(QStringLiteral("저장하지 않은 편집")), qPrintable(placed.message));
    QCOMPARE(asked, 0);
    QCOMPARE(TrenchLayerEdit::layoutSignature(layer), moved);

    // Saved hand edits: asked first, 「그대로 두기」 keeps the grid.
    QVERIFY(layer->commitChanges(false));
    placed = TrenchLayerEdit::placeGrid(project, gpkg, twoCells, crs, load, confirm);
    QVERIFY(placed.outcome == PlaceOutcome::Kept);
    QCOMPARE(asked, 1);
    QCOMPARE(layer->featureCount(), 3);
    QVERIFY(!layer->isModified());

    // Replacing anyway leaves the hand-adjusted grid one Ctrl+Z away.
    answer = true;
    placed = TrenchLayerEdit::placeGrid(project, gpkg, twoCells, crs, load, confirm);
    QVERIFY2(placed.outcome == PlaceOutcome::Placed, qPrintable(placed.message));
    QCOMPARE(asked, 2);
    QCOMPARE(layer->featureCount(), 2);
    QCOMPARE(layer->dataProvider()->featureCount(), 3);
    layer->undoStack()->undo();
    QCOMPARE(TrenchLayerEdit::layoutSignature(layer), moved);
    QVERIFY(layer->rollBack());
  }

  // Trenches from before the record existed are treated as possibly hand-adjusted.
  void unknownGridIsTreatedAsAdjusted() {
    auto* layer = new QgsVectorLayer(
        QStringLiteral("Polygon?crs=EPSG:5186&field=name:string&field=width:double&field=length:double"),
        QStringLiteral("시굴격자"), QStringLiteral("memory"));
    LayerOps::markSurveyLayer(layer, QStringLiteral("trial_trench"));
    QgsProject::instance()->addMapLayer(layer);
    TrenchGridGenerator::Spec spec;
    spec.originX = 200000.0;
    spec.originY = 450000.0;
    QString error;
    QVERIFY2(TrenchLayerEdit::replaceWithCells(layer, TrenchGridGenerator::build(spec),
                                               QStringLiteral("EPSG:5186"), &error), qPrintable(error));
    QVERIFY(layer->commitChanges(false));
    QVERIFY(!TrenchLayerEdit::hasHandAdjustments(layer));
    layer->removeCustomProperty(QString::fromLatin1(TrenchLayerEdit::kPlacedLayoutKey));
    QVERIFY(TrenchLayerEdit::hasHandAdjustments(layer));
    int asked = 0;
    spec.cols = 2;  // a different grid; the same one would simply be kept
    const auto placed = TrenchLayerEdit::placeGrid(
        QgsProject::instance(), QString(), TrenchGridGenerator::build(spec), QStringLiteral("EPSG:5186"),
        [] { return static_cast<QgsVectorLayer*>(nullptr); }, [&](qint64) { return ++asked < 0; });
    QVERIFY(placed.outcome == PlaceOutcome::Kept);
    QCOMPARE(asked, 1);
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  const QString prefix = qEnvironmentVariable(
      "QGIS_PREFIX_PATH", QFile::exists(QStringLiteral("A:/OSGeo4W/apps/qgis-dev"))
                              ? QStringLiteral("A:/OSGeo4W/apps/qgis-dev")
                              : QStringLiteral("C:/OSGeo4W/apps/qgis-dev"));
  QgsApplication::setPrefixPath(prefix, true);
  QgsApplication::setPluginPath(prefix + QStringLiteral("/plugins"));
  QgsApplication::initQgis();
  TestTrenchPlan tc;
  const int rc = QTest::qExec(&tc, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}

#include "test_trench_plan.moc"
