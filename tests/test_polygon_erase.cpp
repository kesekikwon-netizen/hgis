// 「겹친 곳 지우기」와 도형 고르기. 위에 그린 면 모양대로 아래 면에서 그 자리만 지우고,
// 큰 면 위에 그린 작은 면과 아직 저장하지 않은 도형도 찍어서 고를 수 있어야 한다.
#include <QtTest>
#include <QTemporaryDir>
#include <QUndoStack>

#include <qgsapplication.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsgeometry.h>
#include <qgspolygon.h>
#include <qgsproject.h>
#include <qgsrectangle.h>
#include <qgsvectorlayer.h>

#include "core/FeaturePick.h"
#include "core/LayerOps.h"
#include "core/PolygonErase.h"
#include "core/SurveyProjectFactory.h"

class PolygonEraseTest : public QObject {
  Q_OBJECT

private:
  static QgsVectorLayer* surveyLayer(QgsProject& project, const QString& name,
                                     const QString& type = QStringLiteral("Polygon")) {
    auto* layer = new QgsVectorLayer(QStringLiteral("%1?crs=EPSG:5186&field=name:string(40)").arg(type), name,
                                     QStringLiteral("memory"));
    LayerOps::markSurveyLayer(layer, QStringLiteral("survey_area"));
    project.addMapLayer(layer);
    return layer;
  }
  // 저장하지 않은 채로 둔다. 돌려주는 번호는 편집 버퍼의 음수 번호다.
  static QgsFeatureId draw(QgsVectorLayer* layer, const QgsGeometry& geometry, const QString& name) {
    if (!layer->isEditable() && !layer->startEditing()) return FID_NULL;
    QgsFeature feature(layer->fields());
    feature.setAttribute(0, name);
    feature.setGeometry(geometry);
    return layer->addFeature(feature) ? feature.id() : FID_NULL;
  }
  static QgsFeatureId drawRect(QgsVectorLayer* layer, double x1, double y1, double x2, double y2,
                               const QString& name) {
    return draw(layer, QgsGeometry::fromRect(QgsRectangle(x1, y1, x2, y2)), name);
  }
  static QgsFeatureId named(QgsVectorLayer* layer, const QString& name) {
    QgsFeature feature;
    QgsFeatureIterator it = layer->getFeatures();
    while (it.nextFeature(feature)) {
      if (feature.attribute(0).toString() == name) return feature.id();
    }
    return FID_NULL;
  }
  static PolygonErase::Shape shape(QgsVectorLayer* layer, QgsFeatureId fid) {
    PolygonErase::Shape out;
    out.layer = layer;
    out.fid = fid;
    return out;
  }
  static int holes(const QgsGeometry& geometry) {
    const auto* polygon = qgsgeometry_cast<const QgsPolygon*>(geometry.constGet());
    return polygon ? polygon->numInteriorRings() : -1;
  }
  static FeaturePick::Hit pick(const QgsProject& project, const QList<QgsMapLayer*>& layers, double x, double y) {
    return FeaturePick::at(layers, QgsPointXY(x, y), 1.0, QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")),
                           project.transformContext());
  }

private slots:
  void erased_cutsOnlyTheOverlappingPlace() {
    const QgsGeometry outer = QgsGeometry::fromRect(QgsRectangle(0, 0, 100, 100));
    const QgsGeometry inside = PolygonErase::erased(outer, QgsGeometry::fromRect(QgsRectangle(40, 40, 60, 60)));
    QCOMPARE(inside.type(), Qgis::GeometryType::Polygon);
    QVERIFY(qAbs(inside.area() - 9600.0) < 1e-6);
    QCOMPARE(holes(inside), 1);
    // 가장자리에 걸친 도형은 구멍이 아니라 파인 자리가 된다.
    const QgsGeometry notch = PolygonErase::erased(outer, QgsGeometry::fromRect(QgsRectangle(80, 40, 120, 60)));
    QVERIFY(qAbs(notch.area() - 9600.0) < 1e-6);
    QCOMPARE(holes(notch), 0);
    // 통째로 덮이면 남는 면이 없다.
    QVERIFY(PolygonErase::erased(outer, QgsGeometry::fromRect(QgsRectangle(-10, -10, 110, 110))).isNull());
    // 떨어져 있으면 그대로다.
    const QgsGeometry apart = PolygonErase::erased(outer, QgsGeometry::fromRect(QgsRectangle(200, 0, 300, 100)));
    QVERIFY(qAbs(apart.area() - 10000.0) < 1e-6);
  }

  void oneSelectedShapeCutsTheShapeUnderItAndOneUndoRestoresBoth() {
    QgsProject project;
    auto* layer = surveyLayer(project, QStringLiteral("조사구역"));
    drawRect(layer, 0, 0, 100, 100, QStringLiteral("바깥"));
    drawRect(layer, 300, 0, 400, 100, QStringLiteral("멀리"));
    QVERIFY(layer->commitChanges());
    const QgsFeatureId outer = named(layer, QStringLiteral("바깥"));
    // 위에 그린 도형은 아직 저장하지 않았다(음수 번호). 그래도 자르는 면으로 쓸 수 있어야 한다.
    const QgsFeatureId inner = drawRect(layer, 40, 40, 60, 60, QStringLiteral("안쪽"));
    QVERIFY(inner < 0);
    const int undoIndex = layer->undoStack()->index();

    const PolygonErase::Plan plan = PolygonErase::plan({shape(layer, inner)}, &project);
    QVERIFY2(plan.ready(), qPrintable(plan.hint));
    QCOMPARE(plan.cutters.size(), 1);
    QCOMPARE(plan.targets.size(), 1);
    QCOMPARE(plan.targets.first().fid, outer);

    const PolygonErase::Outcome outcome = PolygonErase::apply(plan, &project);
    QVERIFY2(outcome.ok, qPrintable(outcome.error));
    QCOMPARE(outcome.erased, 1);
    QCOMPARE(outcome.added, 0);
    QCOMPARE(outcome.layers.size(), 1);
    const QgsGeometry cut = layer->getFeature(outer).geometry();
    QVERIFY(qAbs(cut.area() - 9600.0) < 1e-6);
    QCOMPARE(holes(cut), 1);
    QVERIFY2(!layer->getFeature(inner).isValid(), "위에 그린 도형이 남아 있습니다.");
    QVERIFY(qAbs(layer->getFeature(named(layer, QStringLiteral("멀리"))).geometry().area() - 10000.0) < 1e-6);
    QCOMPARE(layer->featureCount(), 2);

    // 한 번의 편집 명령이어야 Ctrl+Z 한 번에 구멍도 메워지고 위 도형도 돌아온다.
    QCOMPARE(layer->undoStack()->index(), undoIndex + 1);
    layer->undoStack()->undo();
    QVERIFY(qAbs(layer->getFeature(outer).geometry().area() - 10000.0) < 1e-6);
    QVERIFY(layer->getFeature(inner).isValid());
    QCOMPARE(layer->featureCount(), 3);
  }

  void selectingTheBigShapeOrALonelyShapeOnlyExplains() {
    QgsProject project;
    auto* layer = surveyLayer(project, QStringLiteral("조사구역"));
    const QgsFeatureId outer = drawRect(layer, 0, 0, 100, 100, QStringLiteral("바깥"));
    drawRect(layer, 40, 40, 60, 60, QStringLiteral("안쪽"));
    const QgsFeatureId apart = drawRect(layer, 300, 0, 400, 100, QStringLiteral("멀리"));

    const PolygonErase::Plan covering = PolygonErase::plan({shape(layer, outer)}, &project);
    QVERIFY(!covering.ready());
    QVERIFY2(covering.hint.contains(QStringLiteral("덮고")), qPrintable(covering.hint));
    const PolygonErase::Plan lonely = PolygonErase::plan({shape(layer, apart)}, &project);
    QVERIFY(!lonely.ready());
    QVERIFY2(lonely.hint.contains(QStringLiteral("겹친 면이 없습니다")), qPrintable(lonely.hint));
    const PolygonErase::Plan nothing = PolygonErase::plan({}, &project);
    QVERIFY(!nothing.ready());
    QVERIFY2(nothing.hint.contains(QStringLiteral("도형선택")), qPrintable(nothing.hint));
    // 계획이 서지 않으면 아무것도 고치지 않는다.
    QVERIFY(!PolygonErase::apply(covering, &project).ok);
    QCOMPARE(layer->featureCount(), 3);
    QVERIFY(qAbs(layer->getFeature(outer).geometry().area() - 10000.0) < 1e-6);
  }

  void anotherLayerIsCutOnlyWhenExactlyOneShapeLiesUnder() {
    QgsProject project;
    auto* area = surveyLayer(project, QStringLiteral("조사구역"));
    auto* easy = surveyLayer(project, QStringLiteral("쉽게그리기"));
    auto* pits = surveyLayer(project, QStringLiteral("유구면"));
    const QgsFeatureId outer = drawRect(area, 0, 0, 100, 100, QStringLiteral("바깥"));
    const QgsFeatureId cutter = drawRect(easy, 40, 40, 60, 60, QStringLiteral("뺄 자리"));

    PolygonErase::Plan plan = PolygonErase::plan({shape(easy, cutter)}, &project);
    QVERIFY2(plan.ready(), qPrintable(plan.hint));
    QCOMPARE(plan.targets.size(), 1);
    QVERIFY(plan.targets.first().layer == area);
    QCOMPARE(plan.targets.first().fid, outer);

    // 아래에 도형이 둘이면 어느 것을 지울지 알 수 없다. 고르게 한다.
    drawRect(pits, 30, 30, 70, 70, QStringLiteral("수혈"));
    plan = PolygonErase::plan({shape(easy, cutter)}, &project);
    QVERIFY(!plan.ready());
    QVERIFY2(plan.hint.contains(QStringLiteral("2개")) && plan.hint.contains(QStringLiteral("Shift")),
             qPrintable(plan.hint));
    QVERIFY(qAbs(area->getFeature(outer).geometry().area() - 10000.0) < 1e-6);
  }

  void severalSelectedShapesCutTheLargestAcrossLayers() {
    QgsProject project;
    auto* area = surveyLayer(project, QStringLiteral("조사구역"));
    auto* easy = surveyLayer(project, QStringLiteral("쉽게그리기"));
    const QgsFeatureId outer = drawRect(area, 0, 0, 100, 100, QStringLiteral("바깥"));
    const QgsFeatureId first = drawRect(easy, 10, 10, 30, 30, QStringLiteral("하나"));
    const QgsFeatureId second = drawRect(area, 60, 60, 80, 80, QStringLiteral("둘"));
    const QgsFeatureId apart = drawRect(easy, 300, 0, 310, 10, QStringLiteral("멀리"));

    const PolygonErase::Plan plan = PolygonErase::plan(
        {shape(easy, first), shape(area, outer), shape(area, second), shape(easy, apart)}, &project);
    QVERIFY2(plan.ready(), qPrintable(plan.hint));
    QCOMPARE(plan.targets.size(), 1);
    QCOMPARE(plan.targets.first().fid, outer);
    QCOMPARE(plan.cutters.size(), 2);  // 겹치지 않는 「멀리」는 쓰지도 지우지도 않는다.

    const PolygonErase::Outcome outcome = PolygonErase::apply(plan, &project);
    QVERIFY2(outcome.ok, qPrintable(outcome.error));
    QCOMPARE(outcome.layers.size(), 2);
    QVERIFY(qAbs(area->getFeature(outer).geometry().area() - 9200.0) < 1e-6);
    QCOMPARE(holes(area->getFeature(outer).geometry()), 2);
    QVERIFY(!easy->getFeature(first).isValid());
    QVERIFY(!area->getFeature(second).isValid());
    QVERIFY(easy->getFeature(apart).isValid());
  }

  void referenceAndCadastralShapesAreNeverCutOrUsed() {
    QgsProject project;
    auto* area = surveyLayer(project, QStringLiteral("조사구역"));
    auto* heritage = surveyLayer(project, QStringLiteral("문화유적분포지도"));
    heritage->removeCustomProperty(QString::fromUtf8(LayerOps::kPropLayerKey));
    LayerOps::markReferenceLayer(heritage);
    auto* parcels = surveyLayer(project, QStringLiteral("받은 필지"));
    parcels->removeCustomProperty(QString::fromUtf8(LayerOps::kPropLayerKey));
    LayerOps::markCadastralLayer(parcels);
    QVERIFY(PolygonErase::editable(area));
    QVERIFY(!PolygonErase::editable(heritage));
    QVERIFY(!PolygonErase::editable(parcels));

    drawRect(heritage, 0, 0, 100, 100, QStringLiteral("유물산포지"));
    drawRect(parcels, 0, 0, 100, 100, QStringLiteral("100-1"));
    const QgsFeatureId cutter = drawRect(area, 40, 40, 60, 60, QStringLiteral("뺄 자리"));
    QVERIFY(heritage->commitChanges());
    QVERIFY(parcels->commitChanges());

    // 조사 도형 아래에 참조·지적 도형만 있으면 지울 것이 없다.
    const PolygonErase::Plan under = PolygonErase::plan({shape(area, cutter)}, &project);
    QVERIFY(!under.ready());
    // 참조·지적 도형을 골라도 자르는 데 쓰지 않는다.
    const PolygonErase::Plan picked =
        PolygonErase::plan({shape(heritage, named(heritage, QStringLiteral("유물산포지")))}, &project);
    QVERIFY(!picked.ready());
    QVERIFY2(picked.hint.contains(QStringLiteral("지울 수 없습니다")), qPrintable(picked.hint));
    QCOMPARE(heritage->featureCount(), 1);
    QCOMPARE(parcels->featureCount(), 1);
  }

  void shapeCutInTwoStaysValidInASinglePolygonFile() {
    // 조사 파일의 조사구역 테이블은 한 면짜리(POLYGON)다. 위에 그린 띠가 면을 둘로 가르면
    // 조각마다 도형 하나가 되어야 저장한 뒤에도 넓이가 맞는다.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString error;
    const QString path =
        SurveyProjectFactory::createNewSurvey(dir.path(), QStringLiteral("갈림"), &error, QStringLiteral("EPSG:5187"));
    QVERIFY2(!path.isEmpty(), qPrintable(error));
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    auto* layer = LayerOps::ensureDomainLayer(&project, path, QStringLiteral("survey_area"), QStringLiteral("조사구역"),
                                              &error);
    QVERIFY2(layer, qPrintable(error));
    QVERIFY(!QgsWkbTypes::isMultiType(layer->wkbType()));
    const int nameField = layer->fields().indexOf(QStringLiteral("survey_name"));
    QVERIFY(nameField >= 0);
    const auto add = [&](const QgsRectangle& box, const QString& name) {
      QgsFeature feature(layer->fields());
      feature.setAttribute(nameField, name);
      feature.setGeometry(QgsGeometry::fromRect(box));
      return layer->addFeature(feature) ? feature.id() : FID_NULL;
    };
    QVERIFY(layer->startEditing());
    QVERIFY(!FID_IS_NULL(add(QgsRectangle(190000, 560000, 190100, 560100), QStringLiteral("바깥"))));
    QVERIFY(layer->commitChanges(false));
    const QgsFeatureId band = add(QgsRectangle(190040, 559990, 190060, 560110), QStringLiteral("띠"));
    QVERIFY(!FID_IS_NULL(band));

    const PolygonErase::Plan plan = PolygonErase::plan({shape(layer, band)}, &project);
    QVERIFY2(plan.ready(), qPrintable(plan.hint));
    const PolygonErase::Outcome outcome = PolygonErase::apply(plan, &project);
    QVERIFY2(outcome.ok, qPrintable(outcome.error));
    QCOMPARE(outcome.erased, 1);
    QCOMPARE(outcome.added, 1);
    QVERIFY(layer->commitChanges());

    int count = 0;
    double total = 0.0;
    QgsFeature feature;
    QgsFeatureIterator it = layer->getFeatures();
    while (it.nextFeature(feature)) {
      ++count;
      const QgsGeometry geometry = feature.geometry();
      QVERIFY2(geometry.isGeosValid(), "저장한 도형이 깨졌습니다.");
      QVERIFY2(!geometry.isMultipart() || geometry.constGet()->partCount() == 1,
               "한 면짜리 테이블에 여러 조각이 한 도형으로 들어갔습니다.");
      QVERIFY(qAbs(geometry.area() - 4000.0) < 1e-3);
      QCOMPARE(feature.attribute(nameField).toString(), QStringLiteral("바깥"));
      total += geometry.area();
    }
    QCOMPARE(count, 2);
    QVERIFY(qAbs(total - 8000.0) < 1e-3);
  }

  void shapeCutInTwoStaysOneShapeInAMultiPolygonLayer() {
    QgsProject project;
    auto* layer = surveyLayer(project, QStringLiteral("받은 조사지역"), QStringLiteral("MultiPolygon"));
    const QgsFeatureId outer = drawRect(layer, 0, 0, 100, 100, QStringLiteral("바깥"));
    const QgsFeatureId band = drawRect(layer, 40, -10, 60, 110, QStringLiteral("띠"));
    const PolygonErase::Outcome outcome =
        PolygonErase::apply(PolygonErase::plan({shape(layer, band)}, &project), &project);
    QVERIFY2(outcome.ok, qPrintable(outcome.error));
    QCOMPARE(outcome.added, 0);
    QCOMPARE(layer->featureCount(), 1);
    const QgsGeometry cut = layer->getFeature(outer).geometry();
    QVERIFY(cut.isMultipart());
    QCOMPARE(cut.constGet()->partCount(), 2);
    QVERIFY(qAbs(cut.area() - 8000.0) < 1e-6);
  }

  void pick_takesTheSmallShapeDrawnOnTopEvenWhenUnsaved() {
    QgsProject project;
    auto* layer = surveyLayer(project, QStringLiteral("조사구역"));
    drawRect(layer, 0, 0, 100, 100, QStringLiteral("바깥"));
    QVERIFY(layer->commitChanges());
    const QgsFeatureId outer = named(layer, QStringLiteral("바깥"));
    const QgsFeatureId inner = drawRect(layer, 40, 40, 60, 60, QStringLiteral("안쪽"));
    QVERIFY(inner < 0);
    const QList<QgsMapLayer*> layers{layer};

    const FeaturePick::Hit onInner = pick(project, layers, 50, 50);
    QVERIFY(onInner.valid());
    QCOMPARE(onInner.fid, inner);
    const FeaturePick::Hit onOuter = pick(project, layers, 10, 10);
    QVERIFY(onOuter.valid());
    QCOMPARE(onOuter.fid, outer);
    QVERIFY(!pick(project, layers, 500, 500).valid());
  }

  void pick_findsAShapeInsideAShapeOfTheLayerAbove() {
    // 조사구역 레이어가 유구면 레이어보다 위에 있어도, 조사구역 안의 유구를 찍으면 유구가 잡혀야 한다.
    QgsProject project;
    auto* area = surveyLayer(project, QStringLiteral("조사구역"));
    auto* pits = surveyLayer(project, QStringLiteral("유구면"));
    const QgsFeatureId outer = drawRect(area, 0, 0, 100, 100, QStringLiteral("바깥"));
    const QgsFeatureId pit = drawRect(pits, 40, 40, 60, 60, QStringLiteral("수혈"));
    const QList<QgsMapLayer*> layers{area, pits};
    const FeaturePick::Hit hit = pick(project, layers, 50, 50);
    QVERIFY(hit.layer == pits);
    QCOMPARE(hit.fid, pit);
    const FeaturePick::Hit beside = pick(project, layers, 10, 10);
    QVERIFY(beside.layer == area);
    QCOMPARE(beside.fid, outer);
  }

  void pick_prefersSurveyShapesAndNearbyLines() {
    QgsProject project;
    auto* parcels = surveyLayer(project, QStringLiteral("받은 필지"));
    parcels->removeCustomProperty(QString::fromUtf8(LayerOps::kPropLayerKey));
    LayerOps::markCadastralLayer(parcels);
    auto* area = surveyLayer(project, QStringLiteral("조사구역"));
    auto* lines = new QgsVectorLayer(QStringLiteral("LineString?crs=EPSG:5186&field=name:string(40)"),
                                     QStringLiteral("유구선"), QStringLiteral("memory"));
    LayerOps::markSurveyLayer(lines, QStringLiteral("feature_line"));
    project.addMapLayer(lines);
    drawRect(parcels, 45, 45, 55, 55, QStringLiteral("100-1"));
    const QgsFeatureId outer = drawRect(area, 0, 0, 100, 100, QStringLiteral("바깥"));
    const QgsFeatureId wall = draw(lines, QgsGeometry::fromWkt(QStringLiteral("LINESTRING(0 80, 100 80)")),
                                   QStringLiteral("석렬"));
    // 지적 필지가 더 작고 위 레이어여도 조사 도형이 먼저다.
    const QList<QgsMapLayer*> layers{parcels, area, lines};
    const FeaturePick::Hit onArea = pick(project, layers, 50, 50);
    QVERIFY(onArea.layer == area);
    QCOMPARE(onArea.fid, outer);
    // 면 안에 그은 선은 가까이 찍으면 선이 잡힌다.
    const FeaturePick::Hit onLine = pick(project, layers, 50, 80.5);
    QVERIFY(onLine.layer == lines);
    QCOMPARE(onLine.fid, wall);
    // 조사 도형이 없는 자리에서는 지적 필지도 고를 수 있다.
    const FeaturePick::Hit onParcel = pick(project, {parcels}, 50, 50);
    QVERIFY(onParcel.layer == parcels);
  }

  void movingTheFirstVertexKeepsAHoleClosed() {
    // 구멍이 있는 면은 고리마다 따로 닫힌다. 예전에는 면 전체의 첫 점과 끝 점을 짝지어,
    // 바깥 고리의 첫 점을 옮기면 구멍의 닫는 점이 같이 끌려갔다.
    QgsProject project;
    auto* layer = surveyLayer(project, QStringLiteral("조사구역"));
    const QgsGeometry holed = QgsGeometry::fromWkt(QStringLiteral(
        "POLYGON((0 0, 100 0, 100 100, 0 100, 0 0),(40 40, 60 40, 60 60, 40 60, 40 40))"));
    QCOMPARE(LayerOps::ringClosingVertex(holed, 0), 4);
    QCOMPARE(LayerOps::ringClosingVertex(holed, 4), 0);
    QCOMPARE(LayerOps::ringClosingVertex(holed, 5), 9);
    QCOMPARE(LayerOps::ringClosingVertex(holed, 9), 5);
    QCOMPARE(LayerOps::ringClosingVertex(holed, 2), -1);
    QCOMPARE(LayerOps::ringClosingVertex(holed, 7), -1);

    const QgsFeatureId fid = draw(layer, holed, QStringLiteral("구멍"));
    QString error;
    QVERIFY2(LayerOps::applyVertexMove(layer, fid, 0, -10.0, -10.0, false, &error), qPrintable(error));
    QgsGeometry moved = layer->getFeature(fid).geometry();
    QVERIFY2(moved.isGeosValid(), "바깥 고리의 첫 점을 옮겼더니 도형이 깨졌습니다.");
    QCOMPARE(holes(moved), 1);
    QVERIFY(qAbs(moved.vertexAt(9).x() - 40.0) < 1e-9 && qAbs(moved.vertexAt(9).y() - 40.0) < 1e-9);
    QVERIFY(qAbs(moved.vertexAt(4).x() + 10.0) < 1e-9 && qAbs(moved.vertexAt(4).y() + 10.0) < 1e-9);
    // 구멍의 첫 점을 옮기면 구멍의 닫는 점이 따라온다.
    QVERIFY2(LayerOps::applyVertexMove(layer, fid, 5, 45.0, 45.0, false, &error), qPrintable(error));
    moved = layer->getFeature(fid).geometry();
    QVERIFY(moved.isGeosValid());
    QVERIFY(qAbs(moved.vertexAt(9).x() - 45.0) < 1e-9 && qAbs(moved.vertexAt(9).y() - 45.0) < 1e-9);
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  PolygonEraseTest test;
  const int result = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis();
  return result;
}
#include "test_polygon_erase.moc"
