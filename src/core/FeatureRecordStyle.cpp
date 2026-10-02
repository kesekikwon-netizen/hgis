// The 「시대색·종류 무늬」 look of FeaturePresets (split from FeaturePresets.cpp to keep
// each file small): period = fill colour from the ordered ramp, kind = hatch / dash /
// marker shape, so a black-and-white print still tells both apart.
#include "FeaturePresets.h"

#include <QMap>
#include <algorithm>
#include <memory>

#include <qgis.h>
#include <qgscategorizedsymbolrenderer.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgsfields.h>
#include <qgsfillsymbol.h>
#include <qgsfillsymbollayer.h>
#include <qgslinesymbol.h>
#include <qgslinesymbollayer.h>
#include <qgsmarkersymbol.h>
#include <qgssymbollayerutils.h>
#include <qgsvectorlayer.h>

namespace {
constexpr const char* kPropStyleMode = "ka_hgis/style_mode";
constexpr int kFillAlpha = 190;
constexpr double kHatchDistanceMm = 1.8;
const QColor kOutline(63, 63, 70);    // outline of every class: the fill carries the period
const QColor kHatch(31, 41, 55, 200);  // hatch lines and dots
const QColor kCasing(51, 65, 85);     // keeps pale period colours visible as lines

QString mm(double value) { return QString::number(value, 'f', 2); }

void appendLineHatch(QgsFillSymbol* fill, double angle) {
  // QGIS 개발판 1201 이후(1237에서 확인) 기호·기호 층을 만드는 함수가 unique_ptr 를 돌려준다(1201은 맨 포인터).
  // 두 판 모두에서 컴파일되게 unique_ptr 로 감싼 뒤 꺼낸다. LayerOps·BasemapOps·HeritageLayoutNumbers 도 같다.
  QgsSymbolLayer* layer = std::unique_ptr<QgsSymbolLayer>(QgsLinePatternFillSymbolLayer::create({})).release();
  auto* hatch = dynamic_cast<QgsLinePatternFillSymbolLayer*>(layer);
  if (!hatch) {
    delete layer;
    return;
  }
  hatch->setSubSymbol(QgsLineSymbol::createSimple({{QStringLiteral("line_color"), QgsSymbolLayerUtils::encodeColor(kHatch)},
                                                   {QStringLiteral("line_width"), mm(0.18)},
                                                   {QStringLiteral("line_width_unit"), QStringLiteral("MM")}})
                          .release());
  hatch->setLineWidth(0.18);
  hatch->setLineWidthUnit(Qgis::RenderUnit::Millimeters);
  hatch->setLineAngle(angle);
  hatch->setDistance(kHatchDistanceMm);
  hatch->setDistanceUnit(Qgis::RenderUnit::Millimeters);
  hatch->setColor(kHatch);
  fill->appendSymbolLayer(hatch);
}

void appendDots(QgsFillSymbol* fill) {
  QgsSymbolLayer* layer = std::unique_ptr<QgsSymbolLayer>(QgsPointPatternFillSymbolLayer::create({})).release();
  auto* dots = dynamic_cast<QgsPointPatternFillSymbolLayer*>(layer);
  if (!dots) {
    delete layer;
    return;
  }
  dots->setSubSymbol(QgsMarkerSymbol::createSimple({{QStringLiteral("name"), QStringLiteral("circle")},
                                                    {QStringLiteral("color"), QgsSymbolLayerUtils::encodeColor(kHatch)},
                                                    {QStringLiteral("outline_style"), QStringLiteral("no")},
                                                    {QStringLiteral("size"), mm(0.45)},
                                                    {QStringLiteral("size_unit"), QStringLiteral("MM")}})
                         .release());
  dots->setDistanceX(kHatchDistanceMm);
  dots->setDistanceY(kHatchDistanceMm);
  dots->setDistanceXUnit(Qgis::RenderUnit::Millimeters);
  dots->setDistanceYUnit(Qgis::RenderUnit::Millimeters);
  fill->appendSymbolLayer(dots);
}

void appendHatch(QgsFillSymbol* fill, const QString& pattern) {
  if (pattern == QLatin1String("bdiagonal")) appendLineHatch(fill, 45);
  else if (pattern == QLatin1String("fdiagonal")) appendLineHatch(fill, 135);
  else if (pattern == QLatin1String("horizontal")) appendLineHatch(fill, 0);
  else if (pattern == QLatin1String("vertical")) appendLineHatch(fill, 90);
  else if (pattern == QLatin1String("cross")) {
    appendLineHatch(fill, 0);
    appendLineHatch(fill, 90);
  } else if (pattern == QLatin1String("diagcross")) {
    appendLineHatch(fill, 45);
    appendLineHatch(fill, 135);
  } else if (pattern == QLatin1String("dots") || pattern == QLatin1String("dense6")) {
    appendDots(fill);
  }
}

QVariantMap lineProps(const QColor& color, double widthMm, const QString& dash) {
  QVariantMap props{{QStringLiteral("line_color"), QgsSymbolLayerUtils::encodeColor(color)},
                    {QStringLiteral("line_width"), mm(widthMm)},
                    {QStringLiteral("line_width_unit"), QStringLiteral("MM")},
                    {QStringLiteral("capstyle"), QStringLiteral("flat")},
                    {QStringLiteral("joinstyle"), QStringLiteral("round")}};
  if (!dash.isEmpty() && dash != QLatin1String("solid")) {
    // Millimetres, so the casing and the core dash in step at every width.
    props.insert(QStringLiteral("use_custom_dash"), QStringLiteral("1"));
    props.insert(QStringLiteral("customdash"), dash);
    props.insert(QStringLiteral("customdash_unit"), QStringLiteral("MM"));
  }
  return props;
}
}  // namespace

QgsSymbol* FeaturePresets::symbolFor(int geomType, const Kind& kind, const QColor& periodColor) const {
  if (geomType == static_cast<int>(Qgis::GeometryType::Polygon)) {
    QVariantMap props{{QStringLiteral("outline_color"), QgsSymbolLayerUtils::encodeColor(kOutline)},
                      {QStringLiteral("outline_width"), mm(0.6)},
                      {QStringLiteral("outline_width_unit"), QStringLiteral("MM")}};
    if (periodColor.isValid()) {
      QColor fill = periodColor;
      fill.setAlpha(kFillAlpha);
      props.insert(QStringLiteral("color"), QgsSymbolLayerUtils::encodeColor(fill));
      props.insert(QStringLiteral("style"), QStringLiteral("solid"));
    } else {
      props.insert(QStringLiteral("style"), QStringLiteral("no"));  // 미정: outline and hatch only
    }
    std::unique_ptr<QgsFillSymbol> fill = QgsFillSymbol::createSimple(props);
    appendHatch(fill.get(), kind.pattern);
    return fill.release();
  }
  if (geomType == static_cast<int>(Qgis::GeometryType::Line)) {
    std::unique_ptr<QgsLineSymbol> line = QgsLineSymbol::createSimple(lineProps(kCasing, 1.1, kind.line));
    // 미정: a hollow (white) core inside the casing.
    const QColor core = periodColor.isValid() ? periodColor : QColor(Qt::white);
    line->appendSymbolLayer(
        std::unique_ptr<QgsSymbolLayer>(QgsSimpleLineSymbolLayer::create(lineProps(core, 0.6, kind.line))).release());
    return line.release();
  }
  const QColor fill = periodColor.isValid() ? periodColor : QColor(Qt::white);
  return QgsMarkerSymbol::createSimple({{QStringLiteral("name"), kind.marker},
                                       {QStringLiteral("color"), QgsSymbolLayerUtils::encodeColor(fill)},
                                       {QStringLiteral("outline_color"), QgsSymbolLayerUtils::encodeColor(kOutline)},
                                       {QStringLiteral("outline_width"), mm(0.3)},
                                       {QStringLiteral("outline_width_unit"), QStringLiteral("MM")},
                                       {QStringLiteral("size"), mm(3.6)},
                                       {QStringLiteral("size_unit"), QStringLiteral("MM")}})
      .release();
}

bool FeaturePresets::canStyle(const QgsVectorLayer* layer) {
  if (!layer || !layer->isValid() || layer->fields().lookupField(QStringLiteral("kind")) < 0) return false;
  const Qgis::GeometryType gt = layer->geometryType();
  return gt == Qgis::GeometryType::Polygon || gt == Qgis::GeometryType::Line || gt == Qgis::GeometryType::Point;
}

bool FeaturePresets::isPresetStyled(const QgsVectorLayer* layer) {
  return layer && layer->renderer() && layer->renderer()->type() == QLatin1String("categorizedSymbol") &&
         layer->customProperty(QString::fromLatin1(kPropStyleMode)).toString() == QLatin1String(kStyleModePreset);
}

bool FeaturePresets::refreshIfPresetStyled(QgsVectorLayer* layer) {
  return isPresetStyled(layer) && instance().applyRenderer(layer);
}

bool FeaturePresets::applyRenderer(QgsVectorLayer* layer) {
  if (!canStyle(layer)) return false;
  ensureLoaded();
  const int ik = layer->fields().lookupField(QStringLiteral("kind"));
  const int ip = layer->fields().lookupField(QStringLiteral("period"));
  struct Combo {
    QString kind;
    QString period;
  };
  QMap<QString, Combo> combos;  // "kindKey|periodKey" -> first spelling seen
  QgsFeatureRequest request;
  request.setFlags(Qgis::FeatureRequestFlag::NoGeometry);
  request.setSubsetOfAttributes(ip >= 0 ? QgsAttributeList{ik, ip} : QgsAttributeList{ik});
  QgsFeatureIterator it = layer->getFeatures(request);
  QgsFeature f;
  const auto text = [&f](int index) {
    const QVariant v = index >= 0 ? f.attribute(index) : QVariant();
    return v.isNull() ? QString() : v.toString().trimmed();
  };
  while (it.nextFeature(f)) {
    const Combo c{text(ik), text(ip)};
    const QString key = kindKey(c.kind) + QLatin1Char('|') + periodKey(c.period);
    if (!combos.contains(key)) combos.insert(key, c);
  }
  // Legend order: periods oldest first, then kinds in list order; free text after, empty last.
  const int kindCount = static_cast<int>(m_kinds.size());
  const auto kindRank = [&](const QString& kind) {
    if (kind.isEmpty()) return kindCount + 1;
    const Kind* k = matchKind(kind);
    return k ? static_cast<int>(k - m_kinds.constData()) : kindCount;
  };
  const auto periodRank = [&](const QString& period) {
    return period.isEmpty() ? static_cast<int>(m_periods.size()) + 1 : periodOrder(period);
  };
  QList<QString> keys = combos.keys();
  std::stable_sort(keys.begin(), keys.end(), [&](const QString& a, const QString& b) {
    const Combo& x = combos[a];
    const Combo& y = combos[b];
    if (periodRank(x.period) != periodRank(y.period)) return periodRank(x.period) < periodRank(y.period);
    return kindRank(x.kind) < kindRank(y.kind);
  });

  const Kind blank{QString(), QString(), QStringLiteral("none"), QStringLiteral("solid"), QStringLiteral("circle")};
  const Kind* other = matchKind(defaultKindLabel());
  const int gt = static_cast<int>(layer->geometryType());
  QgsCategoryList categories;
  for (const QString& key : std::as_const(keys)) {
    const Combo& c = combos[key];
    const Kind* preset = matchKind(c.kind);
    // Free-text kinds share the 기타 pattern but keep their own legend text.
    const Kind& kind = c.kind.isEmpty() ? blank : (preset ? *preset : (other ? *other : blank));
    const QString kindText = c.kind.isEmpty() ? QStringLiteral("종류 없음") : (preset ? preset->label : c.kind);
    const Period* period = matchPeriod(c.period);
    const QString periodText = c.period.isEmpty() ? QStringLiteral("시대 없음") : (period ? period->label : c.period);
    if (QgsSymbol* symbol = symbolFor(gt, kind, periodColor(c.period)))
      categories.append(QgsRendererCategory(key, symbol, QStringLiteral("%1 · %2").arg(kindText, periodText)));
  }
  // Values added after this build (and anything unexpected) still draw.
  if (QgsSymbol* rest = symbolFor(gt, blank, QColor()))
    categories.append(QgsRendererCategory(QVariant(), rest, QStringLiteral("미분류")));
  const QString classExpression = QStringLiteral("concat(%1, '|', %2)")
                                      .arg(kindKeyExpression(), ip >= 0 ? periodKeyExpression() : QStringLiteral("''"));
  layer->setRenderer(new QgsCategorizedSymbolRenderer(classExpression, categories));
  layer->setCustomProperty(QString::fromLatin1(kPropStyleMode), QString::fromLatin1(kStyleModePreset));
  layer->triggerRepaint();
  return true;
}
