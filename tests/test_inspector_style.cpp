// The inspector's 스타일 tab (KaInspectorStyle): 「채우기 없음 (선만)」·「선 없음」 next to the colour
// swatches (user 2026-10-03 「선채우기없음도 나와야한다」), checked against what a field user hits
// (code review 2026-10-03): both boxes, point layers, a change made in another window.
#include <QtTest>

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QStandardPaths>
#include <memory>

#include <qgsapplication.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgsrectangle.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorlayer.h>

#include "app/KaInspectorStyle.h"
#include "core/LayerOps.h"

namespace {
std::unique_ptr<QgsVectorLayer> surveyLayer(const QString& geometry, const QString& key) {
  auto layer = std::make_unique<QgsVectorLayer>(geometry + QStringLiteral("?crs=EPSG:5187"), key, QStringLiteral("memory"));
  LayerOps::markSurveyLayer(layer.get(), key);
  QgsFeature f(layer->fields());
  f.setGeometry(geometry == QLatin1String("Point") ? QgsGeometry::fromPointXY(QgsPointXY(200000, 450000))
                                                   : QgsGeometry::fromRect(QgsRectangle(200000, 450000, 200010, 450005)));
  layer->dataProvider()->addFeature(f);
  return layer;
}

struct Look {
  QColor fill, stroke;
  double width = 0, marker = 0;
  bool noFill = false, noStroke = false;
};

Look lookOf(QgsVectorLayer* layer) {
  Look l;
  LayerOps::readSimpleVectorStyle(layer, &l.fill, &l.stroke, &l.width, &l.marker, &l.noFill, &l.noStroke);
  return l;
}

QCheckBox* box(KaInspectorStyle& style, const char* name) { return style.findChild<QCheckBox*>(QString::fromLatin1(name)); }

QPushButton* swatch(KaInspectorStyle& style, int index) {
  return style.findChildren<QPushButton*>(QStringLiteral("inspectorStyleSwatch")).value(index);
}
}  // namespace

class TestInspectorStyle : public QObject {
  Q_OBJECT
 private slots:
  // Both boxes on would hide the shape (applySimpleVectorStyle then draws a grey hairline for good):
  // turning one on turns the other off, and the width box shows what the map draws.
  void bothBoxesNeverHideTheShape() {
    auto layer = surveyLayer(QStringLiteral("Polygon"), QStringLiteral("feature_poly"));
    LayerOps::applySimpleVectorStyle(layer.get(), QColor(0x25, 0x63, 0xEB, 50), QColor(0x25, 0x63, 0xEB), 1.8);
    KaInspectorStyle style;
    style.setLayer(layer.get());
    box(style, "inspectorStyleNoFill")->click();
    box(style, "inspectorStyleNoStroke")->click();
    QVERIFY(box(style, "inspectorStyleNoStroke")->isChecked() && !box(style, "inspectorStyleNoFill")->isChecked());
    const Look look = lookOf(layer.get());
    QVERIFY(look.noStroke && !look.noFill && look.fill.alpha() > 0);
    QCOMPARE(look.fill.rgb(), QColor(0x25, 0x63, 0xEB).rgb());  // not a grey hairline
    QCOMPARE(style.findChild<QDoubleSpinBox*>(QStringLiteral("inspectorStyleWidth"))->value(), look.width);
  }

  // A colour picked for an area without an outline colours the area and keeps the outline off.
  void polygonColourKeepsNoOutline() {
    auto layer = surveyLayer(QStringLiteral("Polygon"), QStringLiteral("survey_area"));
    KaInspectorStyle style;
    style.setLayer(layer.get());
    box(style, "inspectorStyleNoStroke")->click();
    swatch(style, 4)->click();  // 파랑
    const Look look = lookOf(layer.get());
    QVERIFY(look.noStroke && box(style, "inspectorStyleNoStroke")->isChecked());
    QCOMPARE(look.fill.rgb(), QColor(0x25, 0x63, 0xEB).rgb());
  }

  // A point layer opened from a file starts without fill: a colour picked fills it.
  void pointColourTurnsTheFillOn() {
    auto layer = surveyLayer(QStringLiteral("Point"), QStringLiteral("artifact_point"));
    LayerOps::applySimpleVectorStyle(layer.get(), QColor(0, 0, 0, 0), QColor(0, 0, 0), 0.2, 3.5, true, false);
    KaInspectorStyle style;
    style.setLayer(layer.get());
    swatch(style, 4)->click();  // 파랑
    const Look look = lookOf(layer.get());
    QVERIFY(!look.noFill && !box(style, "inspectorStyleNoFill")->isChecked());
    QCOMPARE(look.fill, QColor(0x25, 0x63, 0xEB));  // a solid point
  }

  // Turned off and on again, a point comes back solid with an outline it can be told from.
  void pointComesBackSolidWithAVisibleOutline() {
    auto layer = surveyLayer(QStringLiteral("Point"), QStringLiteral("artifact_point"));
    KaInspectorStyle style;
    style.setLayer(layer.get());
    QCheckBox* noFill = box(style, "inspectorStyleNoFill");
    QCheckBox* noStroke = box(style, "inspectorStyleNoStroke");
    noFill->click();
    noFill->click();
    QCOMPARE(lookOf(layer.get()).fill.alpha(), 255);
    noStroke->click();
    noStroke->click();
    const Look look = lookOf(layer.get());
    QVERIFY(!look.noStroke && look.stroke.alpha() == 255);
    QVERIFY(look.stroke.rgb() != look.fill.rgb());
  }

  // The right-click 「면·외곽선 색」 window changed the layer: the boxes follow at once.
  void boxesFollowAChangeMadeElsewhere() {
    auto layer = surveyLayer(QStringLiteral("Polygon"), QStringLiteral("feature_poly"));
    KaInspectorStyle style;
    style.setLayer(layer.get());
    QVERIFY(!box(style, "inspectorStyleNoFill")->isChecked());
    LayerOps::applySimpleVectorStyle(layer.get(), QColor(0, 0, 0, 0), QColor(0xDC, 0x26, 0x26), 1.0, 3.5, true, false);
    QVERIFY(box(style, "inspectorStyleNoFill")->isChecked());
  }
};

int main(int argc, char** argv) {
  QStandardPaths::setTestModeEnabled(true);
  QgsApplication app(argc, argv, true);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH", "A:/OSGeo4W/apps/qgis-dev"), true);
  QgsApplication::initQgis();
  int result = 0;
  {
    TestInspectorStyle tests;
    result = QTest::qExec(&tests, argc, argv);
  }
  QgsApplication::exitQgis();
  return result;
}

#include "test_inspector_style.moc"
