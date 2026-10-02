// Default style table, reserved colours, per-kind colours and label defaults
// (evaluation F146, F089, F144, F145).
#include <QtTest>
#include <qgsapplication.h>
#include <qgscategorizedsymbolrenderer.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgslinesymbollayer.h>
#include <qgspallabeling.h>
#include <qgsrendercontext.h>
#include <qgssinglesymbolrenderer.h>
#include <qgssymbol.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayerlabeling.h>
#include "core/HeritageStyle.h"
#include "core/LayerLabelControls.h"
#include "core/LayerOps.h"
#include "core/LayerStyleDefaults.h"
#include "core/LayerStyleKinds.h"

namespace {
std::unique_ptr<QgsVectorLayer> domain(const QString& key, const QString& geometry, const QString& fields = {}) {
  auto layer = std::make_unique<QgsVectorLayer>(geometry + QStringLiteral("?crs=EPSG:5187") + fields, key,
                                                QStringLiteral("memory"));
  LayerOps::markSurveyLayer(layer.get(), key);
  return layer;
}

void addPolygon(QgsVectorLayer* layer, const QString& kind, double x) {
  QgsFeature feature(layer->fields());
  feature.setAttribute(QStringLiteral("kind"), kind.isNull() ? QVariant() : QVariant(kind));
  feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(x, 0, x + 5, 5)));
  QgsFeatureList features{feature};
  QVERIFY(layer->dataProvider()->addFeatures(features));
}
}  // namespace

class LayerStylesTest : public QObject {
  Q_OBJECT
private slots:
  void oneDefaultTableForDrawAndRead() {
    // Without saved style properties a trench used to read back as the generic blue.
    auto trench = domain(QStringLiteral("trial_trench"), QStringLiteral("Polygon"));
    QColor fill, stroke;
    double width = 0.;
    QVERIFY(LayerOps::readSimpleVectorStyle(trench.get(), &fill, &stroke, &width, nullptr));
    QCOMPARE(stroke, LayerStyleDefaults::forKey(QStringLiteral("trial_trench")).stroke);
    QCOMPARE(width, 0.5);
    for (const QString& key : LayerOps::domainLayerKeys()) {
      const auto s = LayerStyleDefaults::forKey(key);
      QVERIFY2(s.stroke.isValid() && s.fill.isValid(), qPrintable(key));
    }
  }

  void sectionLineColourEditKeepsTheCasing() {
    auto section = domain(QStringLiteral("section_line"), QStringLiteral("LineString"));
    QVERIFY(LayerOps::applyDomainDrawStyle(section.get()));
    QVERIFY(LayerOps::applySimpleVectorStyle(section.get(), QColor(), QColor(20, 90, 200), 1.2));
    auto* single = dynamic_cast<QgsSingleSymbolRenderer*>(section->renderer());
    QVERIFY(single && single->symbol());
    QCOMPARE(single->symbol()->symbolLayerCount(), 2);
    QCOMPARE(single->symbol()->symbolLayer(0)->color(), QColor(255, 255, 255));
    QCOMPARE(single->symbol()->symbolLayer(1)->color(), QColor(20, 90, 200));
    QColor stroke;
    QVERIFY(LayerOps::readSimpleVectorStyle(section.get(), nullptr, &stroke, nullptr, nullptr));
    QCOMPARE(stroke, QColor(20, 90, 200));
    // The next drawing commit keeps both the casing and the user's core colour.
    QVERIFY(LayerOps::applyDomainDrawStyle(section.get()));
    single = dynamic_cast<QgsSingleSymbolRenderer*>(section->renderer());
    QCOMPARE(single->symbol()->symbolLayerCount(), 2);
    QCOMPARE(single->symbol()->symbolLayer(1)->color(), QColor(20, 90, 200));
  }

  void reservedColoursCoverSurveyDefaultsAndHeritage() {
    const QVector<QColor> reserved = LayerStyleDefaults::reservedColors();
    QVERIFY(reserved.contains(HeritageStyle::color(HeritageDataset::HeritageDistributionMap)));
    QVERIFY(LayerStyleDefaults::isNearReservedColor(QColor(22, 163, 74)));   // 유구면 채움
    QVERIFY(LayerStyleDefaults::isNearReservedColor(QColor(234, 179, 8)));   // 기준점
    QVERIFY(LayerStyleDefaults::isNearReservedColor(QColor(128, 128, 128)));  // 밑그림 회색
    QVERIFY(LayerStyleDefaults::deltaE76(QColor(0, 0, 0), QColor(255, 255, 255)) > 99.);
    QCOMPARE(LayerStyleDefaults::deltaE76(QColor(10, 20, 30), QColor(10, 20, 30)), 0.);
  }

  void kindColoursAreStableAndStayVisible() {
    const QColor house = LayerStyleKinds::colorForKind(QStringLiteral("주거지"));
    QCOMPARE(LayerStyleKinds::colorForKind(QStringLiteral("주거지")), house);
    QVERIFY(!LayerStyleDefaults::isNearReservedColor(house));
    auto poly = domain(QStringLiteral("feature_poly"), QStringLiteral("Polygon"),
                       QStringLiteral("&field=kind:string&field=feature_no:string"));
    addPolygon(poly.get(), QStringLiteral("수혈"), 0);
    addPolygon(poly.get(), QStringLiteral("주거지"), 10);
    QVERIFY(LayerOps::applyFeaturePolyStyle(poly.get()));
    QVERIFY(LayerStyleKinds::isByKind(poly.get()));
    auto* cats = dynamic_cast<QgsCategorizedSymbolRenderer*>(poly->renderer());
    QVERIFY(cats);
    QCOMPARE(cats->categories().size(), 3);
    QCOMPARE(cats->categories().at(0).value().toString(), QStringLiteral("수혈"));
    QCOMPARE(cats->categories().at(1).value().toString(), QStringLiteral("주거지"));
    QVERIFY(cats->categories().at(2).value().isNull());
    QCOMPARE(cats->categories().at(2).label(), LayerStyleKinds::unclassifiedLabel());
    const QColor firstHouse = cats->categories().at(1).symbol()->color();
    QVERIFY(firstHouse != cats->categories().at(0).symbol()->color());
    // Same kinds, same colours on every run (not QSet order, not a seeded qHash).
    QVERIFY(LayerOps::applyFeaturePolyStyle(poly.get()));
    cats = dynamic_cast<QgsCategorizedSymbolRenderer*>(poly->renderer());
    QCOMPARE(cats->categories().at(1).symbol()->color(), firstHouse);
    // A freshly drawn feature has no kind yet (no popup while drawing): still drawn.
    addPolygon(poly.get(), QString(), 20);
    QgsFeatureIterator it = poly->getFeatures();
    QgsFeature feature;
    QgsRenderContext context;
    cats->startRender(context, poly->fields());
    int drawn = 0;
    while (it.nextFeature(feature)) drawn += cats->symbolForFeature(feature, context) ? 1 : 0;
    cats->stopRender(context);
    QCOMPARE(drawn, 3);
    // The drawing/attribute path rebuilds the categories instead of one symbol.
    addPolygon(poly.get(), QStringLiteral("가마"), 30);
    QVERIFY(LayerOps::applyDomainDrawStyle(poly.get()));
    cats = dynamic_cast<QgsCategorizedSymbolRenderer*>(poly->renderer());
    QVERIFY2(cats, "종류별 자동 색 must survive a drawing commit");
    QCOMPARE(cats->categories().size(), 4);
    QSet<QRgb> kindColours;
    for (const auto& category : cats->categories()) {
      if (category.value().isNull()) continue;
      const QColor fill = category.symbol()->color();
      kindColours.insert(fill.rgb());
      QVERIFY2(!LayerStyleDefaults::isNearReservedColor(QColor(fill.rgb())), qPrintable(category.label()));
    }
    QCOMPARE(kindColours.size(), 3);  // every kind its own colour
    // Picking one colour in the style dialog ends the mode.
    QVERIFY(LayerOps::applySimpleVectorStyle(poly.get(), QColor(1, 2, 3, 90), QColor(1, 2, 3), 1.0));
    QVERIFY(!LayerStyleKinds::isByKind(poly.get()));
    QVERIFY(LayerOps::applyDomainDrawStyle(poly.get()));
    QVERIFY(dynamic_cast<QgsSingleSymbolRenderer*>(poly->renderer()));
  }

  void labelsStartAtOneDefaultSize() {
    auto area = domain(QStringLiteral("survey_area"), QStringLiteral("Polygon"));
    QVERIFY(LayerOps::applyAreaM2Labels(area.get()));
    QCOMPARE(LayerOps::labelFontSize(area.get()), LayerOps::kDefaultLabelSizePt);
    QCOMPARE(area->labeling()->settings().format().size(), LayerOps::kDefaultLabelSizePt);
    QgsVectorLayer cad(QStringLiteral("Polygon?crs=EPSG:5187&field=JIBUN:string"), QStringLiteral("지적도"),
                       QStringLiteral("memory"));
    QVERIFY(LayerLabelControls::setVisible(&cad, true));  // first switch-on from the list
    QCOMPARE(cad.labeling()->settings().format().size(), LayerOps::kDefaultLabelSizePt);
    QVERIFY(LayerOps::setLabelFontSize(area.get(), 12.));
    QVERIFY(LayerOps::applyDomainDrawStyle(area.get()));
    QCOMPARE(LayerOps::labelFontSize(area.get()), 12.);  // a user size is kept
  }

  void labelColourAndHaloKeepContent() {
    QgsVectorLayer layer(QStringLiteral("Polygon?crs=EPSG:5187&field=name:string"), QStringLiteral("유구"),
                         QStringLiteral("memory"));
    QVERIFY(!LayerOps::setLabelColor(&layer, Qt::blue));  // nothing to style yet
    QVERIFY(LayerOps::applyNameAttributeLabels(&layer, QStringLiteral("name"), 9., true));
    layer.setLabelsEnabled(false);
    const QString expression = layer.labeling()->settings().fieldName;
    QVERIFY(LayerOps::labelHalo(&layer));
    QVERIFY(LayerOps::setLabelColor(&layer, QColor(30, 58, 138)));
    QVERIFY(LayerOps::setLabelHalo(&layer, false));
    QCOMPARE(LayerOps::labelColor(&layer), QColor(30, 58, 138));
    QVERIFY(!LayerOps::labelHalo(&layer));
    QCOMPARE(layer.labeling()->settings().fieldName, expression);
    QCOMPARE(layer.labeling()->settings().format().size(), 9.);
    QVERIFY(!layer.labelsEnabled());
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  LayerStylesTest test;
  const int rc = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
#include "test_layer_styles.moc"
