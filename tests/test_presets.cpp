#include <QtTest>
#include <QCoreApplication>
#include <QFile>
#include <QSet>
#include <memory>

#include "core/FeaturePresets.h"

#include <qgsapplication.h>
#include <qgscategorizedsymbolrenderer.h>
#include <qgsexpression.h>
#include <qgsexpressioncontext.h>
#include <qgsfeature.h>
#include <qgsfield.h>
#include <qgsfields.h>
#include <qgsgeometry.h>
#include <qgsrectangle.h>
#include <qgssymbol.h>
#include <qgssymbollayer.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorlayer.h>

namespace {
QString jsonPath() {
  const QString path = QStringLiteral("data/styles/feature_presets.json");
  return QFile::exists(path) ? path : QFINDTESTDATA("../data/styles/feature_presets.json");
}

std::unique_ptr<QgsVectorLayer> recordLayer(const QString& geometry) {
  return std::make_unique<QgsVectorLayer>(
      QStringLiteral("%1?crs=EPSG:5187&field=kind:string&field=period:string").arg(geometry), QStringLiteral("fp"),
      QStringLiteral("memory"));
}

bool addRecord(QgsVectorLayer* layer, const QString& kind, const QString& period, double offset = 0) {
  QgsFeature f(layer->fields());
  f.setAttribute(QStringLiteral("kind"), kind);
  f.setAttribute(QStringLiteral("period"), period);
  const QgsRectangle box(200000 + offset, 450000, 200005 + offset, 450005);
  if (layer->geometryType() == Qgis::GeometryType::Polygon) f.setGeometry(QgsGeometry::fromRect(box));
  else if (layer->geometryType() == Qgis::GeometryType::Line)
    f.setGeometry(QgsGeometry::fromPolylineXY({box.center(), QgsPointXY(box.xMaximum(), box.yMaximum())}));
  else f.setGeometry(QgsGeometry::fromPointXY(box.center()));
  return layer->dataProvider()->addFeature(f);
}

QgsCategorizedSymbolRenderer* categorized(QgsVectorLayer* layer) {
  return dynamic_cast<QgsCategorizedSymbolRenderer*>(layer->renderer());
}

QStringList legendLabels(QgsVectorLayer* layer) {
  QStringList out;
  if (QgsCategorizedSymbolRenderer* r = categorized(layer))
    for (const QgsRendererCategory& c : r->categories()) out << c.label();
  return out;
}

double luma(const QColor& c) { return 0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue(); }
}  // namespace

class TestPresets : public QObject {
  Q_OBJECT
private slots:
  void loadsJson();
  void jsonMatchesBuiltInTable();
  void periodRamp_isOrderedAndLightnessMonotonic();
  void kinds_differInHatchDashAndMarker();
  void spellingKeys_matchPresets();
  void keyExpressions_matchCppKeys();
  void appliesRenderer_perKindAndPeriod();
  void refreshAfterEdit_keepsPresetLook();
  void lineAndPointSymbols_carryKindPattern();
};

void TestPresets::loadsJson() {
  const QString path = jsonPath();
  QVERIFY2(QFile::exists(path), qPrintable(path));
  FeaturePresets& p = FeaturePresets::instance();
  QVERIFY(p.load(path));
  QVERIFY(p.kinds().size() >= 6);
  QVERIFY(p.periods().size() >= 6);
  QCOMPARE(p.defaultKindLabel(), QStringLiteral("기타"));
  QCOMPARE(p.periodLabels().last(), QStringLiteral("미정"));
  QVERIFY(p.ensureLoaded());
}

void TestPresets::jsonMatchesBuiltInTable() {
  FeaturePresets& p = FeaturePresets::instance();
  QVERIFY(p.load(jsonPath()));
  const auto kinds = FeaturePresets::builtInKinds();
  const auto periods = FeaturePresets::builtInPeriods();
  QCOMPARE(p.kinds().size(), kinds.size());
  QCOMPARE(p.periods().size(), periods.size());
  for (int i = 0; i < kinds.size(); ++i) {
    const auto& a = p.kinds().at(i);
    const auto& b = kinds.at(i);
    QVERIFY2(a.id == b.id && a.label == b.label && a.pattern == b.pattern && a.line == b.line && a.marker == b.marker,
             qPrintable(a.id));
  }
  for (int i = 0; i < periods.size(); ++i) {
    const auto& a = p.periods().at(i);
    QVERIFY2(a.id == periods.at(i).id && a.label == periods.at(i).label &&
                 a.color.compare(periods.at(i).color, Qt::CaseInsensitive) == 0,
             qPrintable(a.id));
  }
}

// Periods are ordered in time: the ramp must stay ordered in black-and-white print and
// under colour-vision deficiency, i.e. its lightness must rise with every step.
void TestPresets::periodRamp_isOrderedAndLightnessMonotonic() {
  FeaturePresets& p = FeaturePresets::instance();
  QVERIFY(p.load(jsonPath()));
  const QStringList labels = p.periodLabels();
  QCOMPARE(labels.first(), QStringLiteral("구석기"));
  QCOMPARE(labels.last(), QStringLiteral("미정"));
  QVERIFY(!p.periodColor(QStringLiteral("미정")).isValid());  // no fill: outline and hatch only
  double previous = -1;
  double previousOnWhite = -1;
  for (const QString& label : labels) {
    const QColor c = p.periodColor(label);
    if (!c.isValid()) continue;
    const double y = luma(c);
    const double onWhite = 255.0 - (190.0 / 255.0) * (255.0 - y);  // the fill alpha of the preset look
    if (previous >= 0) {
      QVERIFY2(y - previous >= 18.0, qPrintable(QStringLiteral("%1 luma step %2").arg(label).arg(y - previous)));
      QVERIFY2(onWhite - previousOnWhite >= 12.0, qPrintable(label));
    }
    previous = y;
    previousOnWhite = onWhite;
  }
}

void TestPresets::kinds_differInHatchDashAndMarker() {
  FeaturePresets& p = FeaturePresets::instance();
  QVERIFY(p.load(jsonPath()));
  QSet<QString> patterns, lines, markers;
  for (const auto& k : p.kinds()) {
    QVERIFY2(!patterns.contains(k.pattern), qPrintable(k.label + QStringLiteral(" pattern ") + k.pattern));
    QVERIFY2(!lines.contains(k.line), qPrintable(k.label + QStringLiteral(" line ") + k.line));
    QVERIFY2(!markers.contains(k.marker), qPrintable(k.label + QStringLiteral(" marker ") + k.marker));
    patterns.insert(k.pattern);
    lines.insert(k.line);
    markers.insert(k.marker);
  }
}

void TestPresets::spellingKeys_matchPresets() {
  FeaturePresets& p = FeaturePresets::instance();
  QVERIFY(p.ensureLoaded());
  QCOMPARE(FeaturePresets::kindKey(QStringLiteral(" 주거 지 ")), QStringLiteral("주거지"));
  QCOMPARE(FeaturePresets::periodKey(QStringLiteral("청동기 시대")), QStringLiteral("청동기"));
  QVERIFY(p.matchPeriod(QStringLiteral("청동기시대")));
  QCOMPARE(p.matchPeriod(QStringLiteral("청동기시대"))->label, QStringLiteral("청동기"));
  QVERIFY(p.matchKind(QStringLiteral("house")));
  QCOMPARE(p.matchKind(QStringLiteral("house"))->label, QStringLiteral("주거지"));
  QVERIFY(!p.matchKind(QStringLiteral("토기가마")));
  QVERIFY(p.periodOrder(QStringLiteral("구석기")) < p.periodOrder(QStringLiteral("조선")));
  QCOMPARE(p.periodOrder(QStringLiteral("미상")), static_cast<int>(p.periods().size()));
}

void TestPresets::keyExpressions_matchCppKeys() {
  auto layer = recordLayer(QStringLiteral("Polygon"));
  QgsFeature f(layer->fields());
  f.setAttribute(QStringLiteral("kind"), QStringLiteral(" 주거 지 "));
  f.setAttribute(QStringLiteral("period"), QStringLiteral("청동기 시대"));
  QgsExpressionContext context;
  context.setFields(layer->fields());
  context.setFeature(f);
  QgsExpression kind(FeaturePresets::kindKeyExpression());
  QgsExpression period(FeaturePresets::periodKeyExpression());
  QCOMPARE(kind.evaluate(&context).toString(), FeaturePresets::kindKey(QStringLiteral(" 주거 지 ")));
  QCOMPARE(period.evaluate(&context).toString(), FeaturePresets::periodKey(QStringLiteral("청동기 시대")));
}

void TestPresets::appliesRenderer_perKindAndPeriod() {
  FeaturePresets& p = FeaturePresets::instance();
  QVERIFY(p.ensureLoaded());
  auto layer = recordLayer(QStringLiteral("Polygon"));
  QVERIFY(addRecord(layer.get(), QStringLiteral("주거지"), QStringLiteral("청동기")));
  QVERIFY(addRecord(layer.get(), QStringLiteral("주거 지"), QStringLiteral("청동기시대"), 10));  // same class
  QVERIFY(addRecord(layer.get(), QStringLiteral("수혈"), QStringLiteral("삼국"), 20));
  QVERIFY(addRecord(layer.get(), QStringLiteral("토기가마"), QString(), 30));
  QVERIFY(addRecord(layer.get(), QString(), QString(), 40));
  QVERIFY(FeaturePresets::canStyle(layer.get()));
  QVERIFY(p.applyRenderer(layer.get()));
  QVERIFY(FeaturePresets::isPresetStyled(layer.get()));
  QCOMPARE(layer->customProperty(QStringLiteral("ka_hgis/style_mode")).toString(),
           QString::fromLatin1(FeaturePresets::kStyleModePreset));
  // Oldest period first, free text after presets, empty values last, 미분류 always there.
  QCOMPARE(legendLabels(layer.get()),
           QStringList({QStringLiteral("주거지 · 청동기"), QStringLiteral("수혈 · 삼국"),
                        QStringLiteral("토기가마 · 시대 없음"), QStringLiteral("종류 없음 · 시대 없음"),
                        QStringLiteral("미분류")}));
  const QgsCategoryList categories = categorized(layer.get())->categories();
  QgsSymbol* house = categories.at(0).symbol();
  QCOMPARE(house->symbolLayer(0)->color().rgb(), p.periodColor(QStringLiteral("청동기")).rgb());
  QVERIFY2(categories.at(1).symbol()->symbolLayerCount() >= 2, "수혈: fill + hatch");
  QCOMPARE(categories.at(0).symbol()->symbolLayerCount(), 1);  // 주거지: fill only
  QVERIFY(!FeaturePresets::canStyle(nullptr));
  QgsVectorLayer noKind(QStringLiteral("Polygon?crs=EPSG:5187&field=note:string"), QStringLiteral("x"),
                        QStringLiteral("memory"));
  QVERIFY(!p.applyRenderer(&noKind));
}

void TestPresets::refreshAfterEdit_keepsPresetLook() {
  FeaturePresets& p = FeaturePresets::instance();
  QVERIFY(p.ensureLoaded());
  auto layer = recordLayer(QStringLiteral("Polygon"));
  QVERIFY(addRecord(layer.get(), QStringLiteral("주거지"), QStringLiteral("청동기")));
  QVERIFY(!FeaturePresets::refreshIfPresetStyled(layer.get()));  // not preset-styled yet
  QVERIFY(p.applyRenderer(layer.get()));
  QVERIFY(addRecord(layer.get(), QStringLiteral("가마"), QStringLiteral("고려"), 10));
  QVERIFY(!legendLabels(layer.get()).contains(QStringLiteral("가마 · 고려")));
  QVERIFY(FeaturePresets::refreshIfPresetStyled(layer.get()));
  QVERIFY(legendLabels(layer.get()).contains(QStringLiteral("가마 · 고려")));
}

void TestPresets::lineAndPointSymbols_carryKindPattern() {
  FeaturePresets& p = FeaturePresets::instance();
  QVERIFY(p.ensureLoaded());
  auto lines = recordLayer(QStringLiteral("LineString"));
  QVERIFY(addRecord(lines.get(), QStringLiteral("구"), QStringLiteral("조선")));
  QVERIFY(p.applyRenderer(lines.get()));
  QgsSymbol* ditch = categorized(lines.get())->categories().at(0).symbol();
  QCOMPARE(ditch->symbolLayerCount(), 2);  // dark casing + period core
  QCOMPARE(ditch->symbolLayer(0)->properties().value(QStringLiteral("use_custom_dash")).toString(),
           QStringLiteral("1"));

  auto points = recordLayer(QStringLiteral("Point"));
  QVERIFY(addRecord(points.get(), QStringLiteral("가마"), QStringLiteral("고려")));
  QVERIFY(addRecord(points.get(), QStringLiteral("분묘"), QStringLiteral("고려"), 10));
  QVERIFY(p.applyRenderer(points.get()));
  const QgsCategoryList categories = categorized(points.get())->categories();
  QVERIFY(categories.size() >= 3);
  QVERIFY(categories.at(0).symbol()->symbolLayer(0)->properties().value(QStringLiteral("name")) !=
          categories.at(1).symbol()->symbolLayer(0)->properties().value(QStringLiteral("name")));
}

#include "test_presets.moc"

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH", QStringLiteral("A:/OSGeo4W/apps/qgis-dev")),
                                true);
  QgsApplication::initQgis();
  TestPresets tc;
  const int rc = QTest::qExec(&tc, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
