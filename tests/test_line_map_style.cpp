// 선으로 된 지도(도면 선·받은 지적도·등고선 같은 선 레이어)는 사용자가 선 색·굵기를 바꾸고 원래 색으로 되돌린다
// (2026-10-03 사용자 목표: 「선색은 사용자가 직접 바꿀수있게하라. 선으로 된지도는 사용자가 색을 바꿀수있게하라.」).
// 토양도·지질도처럼 면을 색으로 채운 지도와 점·글자는 공식 범례·도면 표시라서 바꾸지 않는다.
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "core/CadDrawingLayers.h"
#include "core/LineMapStyle.h"

#include <qgsapplication.h>
#include <qgscategorizedsymbolrenderer.h>
#include <qgsfillsymbol.h>
#include <qgsfillsymbollayer.h>
#include <qgslinesymbol.h>
#include <qgsproject.h>
#include <qgsproperty.h>
#include <qgssinglesymbolrenderer.h>
#include <qgssymbollayer.h>
#include <qgsvectorlayer.h>

namespace {

QgsVectorLayer* memoryLayer(const QString& geometry, const QString& name) {
  return new QgsVectorLayer(geometry + QStringLiteral("?crs=EPSG:5187&field=color:string"), name, QStringLiteral("memory"));
}

// CadDrawingLayers 처럼: 선 색은 도형마다 color 칸(CAD 원래 색)에서 온다.
QgsVectorLayer* cadLines() {
  QgsVectorLayer* layer = memoryLayer(QStringLiteral("LineString"), QStringLiteral("선"));
  auto* symbol = new QgsLineSymbol();
  symbol->symbolLayer(0)->setDataDefinedProperty(QgsSymbolLayer::Property::StrokeColor,
                                                 QgsProperty::fromField(QStringLiteral("color")));
  layer->setRenderer(new QgsSingleSymbolRenderer(symbol));
  layer->setCustomProperty(QString::fromLatin1(CadDrawingLayers::kPropDrawing), QStringLiteral("d1"));
  return layer;
}

bool usesCadColours(const QgsVectorLayer* layer) {
  const auto* single = dynamic_cast<const QgsSingleSymbolRenderer*>(layer->renderer());
  return single && single->symbol()->symbolLayer(0)->dataDefinedProperties().isActive(QgsSymbolLayer::Property::StrokeColor);
}

QColor lineColour(const QgsVectorLayer* layer) {
  const auto* single = dynamic_cast<const QgsSingleSymbolRenderer*>(layer->renderer());
  if (!single) return {};
  const QgsSymbolLayer* top = single->symbol()->symbolLayer(single->symbol()->symbolLayerCount() - 1);
  return layer->geometryType() == Qgis::GeometryType::Line ? top->color() : top->strokeColor();
}

}  // namespace

class TestLineMapStyle : public QObject {
  Q_OBJECT
 private slots:
  void isLineMap_linesOutlinesAndDrawingsOnly() {
    std::unique_ptr<QgsVectorLayer> lines(cadLines());
    QVERIFY(LineMapStyle::isLineMap(lines.get()));
    std::unique_ptr<QgsVectorLayer> contours(memoryLayer(QStringLiteral("LineString"), QStringLiteral("등고선")));
    QVERIFY(LineMapStyle::isLineMap(contours.get()));

    // 받은 지적도처럼 외곽선만 그린 면
    std::unique_ptr<QgsVectorLayer> parcels(memoryLayer(QStringLiteral("Polygon"), QStringLiteral("지적")));
    auto* outline = new QgsSimpleFillSymbolLayer(QColor(0, 0, 0, 0), Qt::NoBrush, QColor(230, 120, 0));
    parcels->setRenderer(new QgsSingleSymbolRenderer(new QgsFillSymbol({outline})));
    QVERIFY(LineMapStyle::isLineMap(parcels.get()));

    // 토양도처럼 종류별로 면을 채운 지도, 점
    std::unique_ptr<QgsVectorLayer> soil(memoryLayer(QStringLiteral("Polygon"), QStringLiteral("토양도")));
    soil->setRenderer(new QgsCategorizedSymbolRenderer(QStringLiteral("color"), {}));
    QVERIFY(!LineMapStyle::isLineMap(soil.get()));
    std::unique_ptr<QgsVectorLayer> points(memoryLayer(QStringLiteral("Point"), QStringLiteral("점")));
    QVERIFY(!LineMapStyle::isLineMap(points.get()));
    QVERIFY(!LineMapStyle::isLineMap(nullptr));
  }

  void apply_paintsAllLinesOneColourAndRestoreBringsBackCadColours() {
    std::unique_ptr<QgsVectorLayer> lines(cadLines());
    QVERIFY(!LineMapStyle::hasOriginal(lines.get()));
    QVERIFY(LineMapStyle::apply(lines.get(), QColor(220, 0, 0), 0.8));
    QVERIFY(!usesCadColours(lines.get()));
    QCOMPARE(lineColour(lines.get()), QColor(220, 0, 0));
    QCOMPARE(LineMapStyle::currentColor(lines.get()), QColor(220, 0, 0));
    QVERIFY(LineMapStyle::hasOriginal(lines.get()));
    QVERIFY(LineMapStyle::apply(lines.get(), QColor(0, 0, 200), 0.5));  // 두 번째로 바꿔도 원래는 처음 것
    QVERIFY(LineMapStyle::restoreOriginal(lines.get()));
    QVERIFY(usesCadColours(lines.get()));
    QVERIFY(!LineMapStyle::hasOriginal(lines.get()));
  }

  void outlineOnlyPolygonsKeepNoFill() {
    std::unique_ptr<QgsVectorLayer> parcels(memoryLayer(QStringLiteral("Polygon"), QStringLiteral("지적")));
    auto* outline = new QgsSimpleFillSymbolLayer(QColor(0, 0, 0, 0), Qt::NoBrush, QColor(230, 120, 0));
    parcels->setRenderer(new QgsSingleSymbolRenderer(new QgsFillSymbol({outline})));
    QVERIFY(LineMapStyle::apply(parcels.get(), QColor(0, 120, 0), 0.4));
    QVERIFY(LineMapStyle::isLineMap(parcels.get()));
    QCOMPARE(lineColour(parcels.get()), QColor(0, 120, 0));
  }

  // 바꾼 색과 되돌릴 원래 색은 조사 작업공간(프로젝트)에 같이 저장되어, 다시 열어도 되돌릴 수 있다.
  void theOriginalSurvivesSavingTheWorkspace() {
    QTemporaryDir tmp;
    const QString qgz = tmp.filePath(QStringLiteral("작업공간.qgz"));
    {
      QgsProject project;
      QgsVectorLayer* lines = cadLines();
      project.addMapLayer(lines);
      QVERIFY(LineMapStyle::apply(lines, QColor(220, 0, 0), 0.8));
      QVERIFY(project.write(qgz));
    }
    QgsProject reopened;
    QVERIFY(reopened.read(qgz));
    const QList<QgsVectorLayer*> layers = reopened.layers<QgsVectorLayer*>();
    QCOMPARE(layers.size(), 1);
    QCOMPARE(lineColour(layers.first()), QColor(220, 0, 0));
    QVERIFY(LineMapStyle::restoreOriginal(layers.first()));
    QVERIFY(usesCadColours(layers.first()));
  }
};

int main(int argc, char** argv) {
  QStandardPaths::setTestModeEnabled(true);
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  int result = 0;
  {
    TestLineMapStyle tests;
    result = QTest::qExec(&tests, argc, argv);
  }
  QgsApplication::exitQgis();
  return result;
}

#include "test_line_map_style.moc"
