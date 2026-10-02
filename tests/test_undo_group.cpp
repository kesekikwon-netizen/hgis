// 도형을 그리고 이름·번호를 넣으면 Ctrl+Z 한 번에 둘 다 사라지게 묶는 KaUndoGroup.
// MainWindow::onGeometryCaptured 와 같은 순서(묶음 → 도형 넣기 → 이름·번호 편집 → 닫기)로 시험한다.
// 「겹친 곳 지우기」를 고르면 지우기 전에 묶음을 닫아, 지우기는 지금처럼 따로 한 단계다.
#include <QStandardPaths>
#include <QUndoStack>
#include <QtTest>

#include "app/KaUndoGroup.h"
#include "core/LayerOps.h"

#include <qgsapplication.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgsvectorlayer.h>

namespace {

std::unique_ptr<QgsVectorLayer> polygons() {
  auto layer = std::make_unique<QgsVectorLayer>(QStringLiteral("Polygon?crs=EPSG:5186&field=name:string"),
                                                QStringLiteral("유구"), QStringLiteral("memory"));
  layer->startEditing();
  return layer;
}

QgsFeature square(const QgsVectorLayer& layer) {
  QgsFeature f(layer.fields());
  f.setGeometry(QgsGeometry::fromRect(QgsRectangle(0, 0, 10, 10)));
  return f;
}

bool rename(QgsVectorLayer* layer, QgsFeatureId fid, const QString& name) {
  return LayerOps::runEditCommand(layer, QStringLiteral("이름·번호"), [&]() {
    return layer->changeAttributeValue(fid, 0, name);
  });
}

}  // namespace

class TestUndoGroup : public QObject {
  Q_OBJECT
 private slots:
  void shapeAndName_undoAndRedoInOneStep() {
    const auto layer = polygons();
    QgsFeature f = square(*layer);
    {
      KaUndoGroup step(layer.get(), QStringLiteral("도형 그리기"));
      QVERIFY(layer->addFeature(f));
      QVERIFY(rename(layer.get(), f.id(), QStringLiteral("1호 주거지")));
    }
    QUndoStack* stack = layer->undoStack();
    QCOMPARE(stack->count(), 1);
    QCOMPARE(stack->text(0), QStringLiteral("도형 그리기"));
    stack->undo();
    QCOMPARE(layer->featureCount(), 0);
    stack->redo();
    QCOMPARE(layer->featureCount(), 1);
    QgsFeature back;
    layer->getFeatures().nextFeature(back);
    QCOMPARE(back.attribute(0).toString(), QStringLiteral("1호 주거지"));
  }

  // 「겹친 곳 지우기」: 지우기 전에 묶음을 닫는다. Ctrl+Z 한 번은 지우기만 되돌리고 그린 도형은 남는다.
  void closingBeforeAnotherEdit_keepsTwoSteps() {
    const auto layer = polygons();
    QgsFeature f = square(*layer);
    KaUndoGroup step(layer.get(), QStringLiteral("도형 그리기"));
    QVERIFY(layer->addFeature(f));
    step.close();
    QVERIFY(LayerOps::runEditCommand(layer.get(), QStringLiteral("겹친 곳 지우기"),
                                     [&]() { return layer->deleteFeature(f.id()); }));
    QCOMPARE(layer->undoStack()->count(), 2);
    layer->undoStack()->undo();
    QCOMPARE(layer->featureCount(), 1);
  }

  // 도형을 넣지 못했으면 빈 단계를 남기지 않는다: Ctrl+Z 가 아무 일도 안 하는 칸이 생기지 않는다.
  void nothingInside_leavesNoStep() {
    const auto layer = polygons();
    { KaUndoGroup step(layer.get(), QStringLiteral("도형 그리기")); }
    QCOMPARE(layer->undoStack()->count(), 0);
    QVERIFY(!layer->undoStack()->canRedo());
  }
};

int main(int argc, char** argv) {
  QStandardPaths::setTestModeEnabled(true);
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  int result = 0;
  {
    TestUndoGroup tests;
    result = QTest::qExec(&tests, argc, argv);
  }
  QgsApplication::exitQgis();
  return result;
}

#include "test_undo_group.moc"
