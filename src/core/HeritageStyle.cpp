#include "HeritageStyle.h"

#include <qgscategorizedsymbolrenderer.h>
#include <qgsfillsymbol.h>
#include <qgslinesymbol.h>
#include <qgsmarkersymbol.h>
#include <qgssinglesymbolrenderer.h>
#include <qgssymbol.h>
#include <qgsvectorlayer.h>

#include <QSet>
#include <QStringList>
#include <QVariant>
#include <memory>

namespace {

struct Entry {
  HeritageDataset ds;
  const char* name;
  const char* tab;
  const char* hex;
};

// 계획서 2026-09-11-heritage-intranet-nearby-sites.md 7.4 표. 여기가 색의 유일한 출처다.
const Entry kTable[] = {
    {HeritageDataset::DesignatedHeritage, "지정유산", "", "#8E44AD"},
    {HeritageDataset::AlterationStandard, "현상변경허용기준", "P", "#E67E22"},
    {HeritageDataset::BuriedHeritageArea, "매장유산유존지역", "B", "#2E86C1"},
    {HeritageDataset::HeritageDistributionMap, "문화유적분포지도", "U", "#27AE60"},
    {HeritageDataset::SurfaceSurveyArea, "지표조사구역", "R", "#D4AA00"},
    {HeritageDataset::ExcavationSurveyArea, "발굴조사구역", "E", "#C2187D"},
};

const Entry* entryOf(HeritageDataset ds) {
  for (const Entry& e : kTable) {
    if (e.ds == ds) return &e;
  }
  return nullptr;
}

QgsSymbol* makeSymbol(const QColor& c, Qgis::GeometryType gt) {
  const QString hex = c.name(QColor::HexRgb);
  const QString width = QString::number(HeritageStyle::outlineWidthMm(), 'f', 2);
  if (gt == Qgis::GeometryType::Polygon) {
    // 테두리만 그린다. 채우면 위성·지적·수치지형도를 가린다.
    return QgsFillSymbol::createSimple({
                                           {QStringLiteral("style"), QStringLiteral("no")},
                                           // 채우지는 않지만 색은 종류 색으로 둔다.
                                           // 심볼에게 색을 물어보는 쪽(범례·색 선택)이 엉뚱한 기본색을 보면 안 된다.
                                           {QStringLiteral("color"), hex},
                                           {QStringLiteral("outline_color"), hex},
                                           {QStringLiteral("outline_width"), width},
                                           {QStringLiteral("outline_width_unit"), QStringLiteral("MM")},
                                       })
        .release();
  }
  if (gt == Qgis::GeometryType::Line) {
    return QgsLineSymbol::createSimple({
                                           {QStringLiteral("line_color"), hex},
                                           {QStringLiteral("line_width"), width},
                                           {QStringLiteral("line_width_unit"), QStringLiteral("MM")},
                                       })
        .release();
  }
  return QgsMarkerSymbol::createSimple({
                                           {QStringLiteral("name"), QStringLiteral("circle")},
                                           {QStringLiteral("color"), hex},
                                           {QStringLiteral("outline_color"), hex},
                                           {QStringLiteral("size"), QStringLiteral("2.0")},
                                           {QStringLiteral("size_unit"), QStringLiteral("MM")},
                                       })
      .release();
}

}  // namespace

QVector<HeritageDataset> HeritageStyle::allDatasets() {
  QVector<HeritageDataset> out;
  out.reserve(int(std::size(kTable)));
  for (const Entry& e : kTable) out.append(e.ds);
  return out;
}

QString HeritageStyle::layerName(HeritageDataset ds) {
  const Entry* e = entryOf(ds);
  return e ? QString::fromUtf8(e->name) : QString();
}

QString HeritageStyle::tabCode(HeritageDataset ds) {
  const Entry* e = entryOf(ds);
  return e ? QString::fromUtf8(e->tab) : QString();
}

QColor HeritageStyle::color(HeritageDataset ds) {
  const Entry* e = entryOf(ds);
  return e ? QColor(QString::fromUtf8(e->hex)) : QColor();
}

QColor HeritageStyle::layoutColor(HeritageDataset ds, const QColor& sourceColor) {
  QColor legacy;
  if (ds == HeritageDataset::SurfaceSurveyArea)
    legacy = QColor(QStringLiteral("#16A085"));
  else if (ds == HeritageDataset::ExcavationSurveyArea)
    legacy = QColor(QStringLiteral("#A0522D"));
  if (!sourceColor.isValid() || !legacy.isValid() || sourceColor.rgb() != legacy.rgb())
    return sourceColor;
  QColor result = color(ds);
  result.setAlphaF(sourceColor.alphaF());
  return result;
}

std::optional<HeritageDataset> HeritageStyle::fromLayerName(const QString& name) {
  const QString t = name.trimmed();
  if (t.isEmpty()) return std::nullopt;
  for (const Entry& e : kTable) {
    if (t == QString::fromUtf8(e.name)) return e.ds;
  }
  return std::nullopt;
}

bool HeritageStyle::isReservedColor(const QColor& c) {
  if (!c.isValid()) return true;
  const int s = c.hsvSaturation();
  const int v = c.value();
  if (v < 40) return true;   // 거의 검정
  if (s < 45) return true;   // 회색 계열 — 수치지형도 밑그림이 쓴다
  const int h = c.hsvHue();
  // 빨강 — 조사구역 팔레트가 쓴다(#DC2626). 갈색(#A0522D, 색상 19°)은 여기 걸리지 않는다.
  if (h >= 0 && (h <= 12 || h >= 348) && s >= 90) return true;
  return false;
}

double HeritageStyle::outlineWidthMm() { return 0.35; }

int HeritageStyle::maxLegendCategories() { return 2000; }

QString HeritageStyle::unnamedLabel() { return QStringLiteral("무명"); }

HeritageStyleResult HeritageStyle::apply(QgsVectorLayer* layer, HeritageDataset ds,
                                         const QString& nameField, int shadeIndex) {
  Q_UNUSED(shadeIndex);  // 색은 종류 하나로 통일한다(2026-09-13 사용자 확인)
  HeritageStyleResult out;
  if (!layer || !layer->isValid()) {
    out.message = QStringLiteral("레이어가 유효하지 않아 스타일을 걸지 못했습니다.");
    return out;
  }
  const QColor c = color(ds);
  if (!c.isValid()) {
    out.message = QStringLiteral("색표에 없는 자료 종류입니다.");
    return out;
  }
  const Qgis::GeometryType gt = layer->geometryType();

  const int idx = nameField.trimmed().isEmpty() ? -1 : layer->fields().indexOf(nameField.trimmed());
  if (idx < 0) {
    // 유적명 필드를 못 찾았다. 단색으로 걸되 그렇다고 말한다.
    if (QgsSymbol* sym = makeSymbol(c, gt)) layer->setRenderer(new QgsSingleSymbolRenderer(sym));
    layer->triggerRepaint();
    out.ok = true;
    out.categoryCount = 1;
    out.message = QStringLiteral("'%1' 필드가 없어 범례에 유적명을 넣지 못했습니다. 속성 컬럼 이름을 확인하세요.")
                      .arg(nameField.trimmed());
    return out;
  }

  const int cap = maxLegendCategories();
  const QSet<QVariant> uniq = layer->uniqueValues(idx, cap + 1);

  QStringList names;
  QHash<QString, QVariant> byText;
  bool sawEmpty = false;
  for (const QVariant& v : uniq) {
    const QString t = v.toString().trimmed();
    if (t.isEmpty()) {
      sawEmpty = true;
      continue;
    }
    if (!byText.contains(t)) {
      byText.insert(t, v);
      names.append(t);
    }
  }
  names.sort();

  if (names.size() > cap) {
    // 조용히 단색으로 떨어뜨리지 않는다. 호출자가 사용자에게 말해야 한다.
    if (QgsSymbol* sym = makeSymbol(c, gt)) layer->setRenderer(new QgsSingleSymbolRenderer(sym));
    layer->triggerRepaint();
    out.ok = true;
    out.overCap = true;
    out.categoryCount = 1;
    out.message = QStringLiteral("유적이 %1곳을 넘어 범례에 유적명을 한 줄씩 넣지 못했습니다. 범위를 좁혀 주세요.")
                      .arg(cap);
    return out;
  }

  QgsCategoryList cats;
  for (const QString& t : std::as_const(names)) {
    if (QgsSymbol* sym = makeSymbol(c, gt))
      cats.append(QgsRendererCategory(byText.value(t), sym, t));
  }
  // 유적명이 빈 레코드와 뒤에 늘어난 값을 받는 자리. 이게 없으면 그 도형이 아예 안 그려진다.
  if (QgsSymbol* rest = makeSymbol(c, gt))
    cats.append(QgsRendererCategory(QVariant(), rest, unnamedLabel()));

  if (cats.isEmpty()) {
    if (QgsSymbol* sym = makeSymbol(c, gt)) layer->setRenderer(new QgsSingleSymbolRenderer(sym));
    layer->triggerRepaint();
    out.ok = true;
    out.categoryCount = 1;
    return out;
  }

  layer->setRenderer(new QgsCategorizedSymbolRenderer(nameField.trimmed(), cats));
  layer->triggerRepaint();
  out.ok = true;
  out.categoryCount = cats.size();
  if (sawEmpty)
    out.message = QStringLiteral("유적명이 비어 있는 도형이 있어 '%1'으로 묶었습니다.").arg(unnamedLabel());
  return out;
}
