#include "KaSessionLog.h"
#include "LayerOps.h"
#include "LayerLabelControls.h"
#include "DemPresentation.h"
#include "DemColorRampLegend.h"
#include "GeorefService.h"
#include "SoilMapService.h"
#include "VworldSettings.h"
#include "KaPortableRuntime.h"

#include <QSignalBlocker>
#include <QDateTime>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTextStream>
#include <QStringConverter>
#include <QRegularExpression>
#include <cmath>
#include <memory>
#include <limits>
#include <algorithm>
#include <QPainter>
#include <QScreen>
#include <QSize>
#include <QUrl>
#include <QWindow>
#include <QColor>
#include <QFont>
#include <QDir>
#include <QDomDocument>
#include <QUrlQuery>
#include <QSet>
#include <QTemporaryFile>
#include <QPointer>
#include <QScopedValueRollback>
#include <QTimer>
#include <QHash>
#include <functional>
#include <QNetworkRequest>

#include <qgis.h>
#include <QUndoStack>
#include <qgsproject.h>
#include <qgssnappingconfig.h>
#include <qgsvectorlayer.h>
#include <qgsrasterlayer.h>
#include <qgsbrightnesscontrastfilter.h>
#include <qgsmapcanvas.h>
#include <qgsvectorfilewriter.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransformcontext.h>
#include <qgscoordinatetransform.h>
#include <qgsexception.h>
#include <qgsfield.h>
#include <qgsfields.h>
#include <qgsfeature.h>
#include <qgsfeaturerequest.h>
#include <qgsfeatureiterator.h>
#include <qgsgeometry.h>
#include <qgspoint.h>
#include <qgspointxy.h>
#include <qgslinestring.h>
#include <qgscategorizedsymbolrenderer.h>
#include <qgssinglesymbolrenderer.h>
#include <qgsinvertedpolygonrenderer.h>
#include <qgssymbol.h>
#include <qgssymbollayer.h>
#include <qgsfillsymbol.h>
#include <qgsfillsymbollayer.h>
#include <qgslinesymbol.h>
#include <qgslinesymbollayer.h>
#include <qgsmarkersymbol.h>
#include <qgsrenderer.h>
#include <qgsrectangle.h>
#include <qgslayertree.h>
#include <qgslayertreelayer.h>
#include <qgsbilinearrasterresampler.h>
#include <qgsrasterresamplefilter.h>
#include <qgsrasterdataprovider.h>
#include <qgsvectordataprovider.h>
#include <qgsrasterrenderer.h>
#include <qgsrastertransparency.h>
#include <qgssinglebandpseudocolorrenderer.h>
#include <qgsrastershader.h>
#include <qgscolorrampshader.h>
#include <qgscolorramplegendnodesettings.h>
#include <qgshillshaderenderer.h>
#include <qgsrasterbandstats.h>
#include <cpl_conv.h>
#include <cpl_error.h>
#include <gdal.h>
#include <gdal_utils.h>
#include <ogr_api.h>
#include <qgsnetworkaccessmanager.h>
#include <qgslayertreegroup.h>
#include <qgsdataprovider.h>
#include <qgsprojectviewsettings.h>
#include <qgspallabeling.h>
#include <qgsvectorlayerlabeling.h>
#include <qgstextformat.h>
#include <qgslabelobstaclesettings.h>
#include <qgsreferencedgeometry.h>

bool LayerOps::applyNameAttributeLabels(QgsVectorLayer* layer, const QString& fieldName,
                                       double fontSizePt, bool showArea) {
  if (!layer || !layer->isValid()) return false;

  QString targetField = fieldName;
  if (targetField.isEmpty())
    targetField = detectNameField(layer);

  if (fontSizePt <= 0.0)
    fontSizePt = 5.0;

  if (!targetField.isEmpty())
    layer->setCustomProperty(QStringLiteral("ka_hgis/label_field"), targetField);
  layer->setCustomProperty(QStringLiteral("ka_hgis/label_font_size"), fontSizePt);
  layer->setCustomProperty(QStringLiteral("ka_hgis/label_show_area"), showArea);

  QgsPalLayerSettings s;
  s.drawLabels = true;

  const bool isPolygon = (layer->geometryType() == Qgis::GeometryType::Polygon);
  const bool hasField = !targetField.isEmpty() && layer->fields().indexOf(targetField) >= 0;

  if (hasField && isPolygon && showArea) {
    s.fieldName = QStringLiteral("coalesce(\"%1\", '') || '\\n(' || format_number(area($geometry), 1) || ' ㎡)'")
                      .arg(targetField);
    s.isExpression = true;
  } else if (hasField) {
    s.fieldName = QStringLiteral("\"%1\"").arg(targetField);
    s.isExpression = true;
  } else if (isPolygon && showArea) {
    s.fieldName = QStringLiteral("format_number(area($geometry), 1) || ' ㎡'");
    s.isExpression = true;
  } else if (isPolygon) {
    s.fieldName = QStringLiteral("''");
    s.isExpression = true;
  } else if (layer->fields().count() > 0) {
    s.fieldName = QStringLiteral("\"%1\"").arg(layer->fields().at(0).name());
    s.isExpression = true;
  } else {
    return false;
  }

  if (isPolygon) {
    s.placement = Qgis::LabelPlacement::OverPoint;
    s.setPolygonPlacementFlags(Qgis::LabelPolygonPlacementFlag::AllowPlacementInsideOfPolygon);
  } else if (layer->geometryType() == Qgis::GeometryType::Line) {
    s.placement = Qgis::LabelPlacement::Line;
  } else {
    s.placement = Qgis::LabelPlacement::AroundPoint;
  }

  QgsLabelObstacleSettings obs = s.obstacleSettings();
  obs.setIsObstacle(false);
  s.setObstacleSettings(obs);

  QgsTextFormat fmt;
  QFont font = fmt.font();
  font.setFamily(QStringLiteral("Malgun Gothic"));
  font.setPointSizeF(fontSizePt);
  font.setBold(true);
  fmt.setFont(font);
  fmt.setSize(fontSizePt);
  fmt.setSizeUnit(Qgis::RenderUnit::Points);
  fmt.setColor(QColor(31, 35, 40));

  QgsTextBufferSettings buf = fmt.buffer();
  buf.setEnabled(true);
  buf.setSize(0.8);
  buf.setColor(QColor(255, 255, 255, 230));
  fmt.setBuffer(buf);
  s.setFormat(fmt);

  layer->setLabeling(new QgsVectorLayerSimpleLabeling(s));
  layer->setLabelsEnabled(true);
  layer->triggerRepaint();
  return true;
}

bool LayerOps::setLabelFontSize(QgsVectorLayer* layer, double fontSizePt) {
  if (!layer || !layer->isValid() || !std::isfinite(fontSizePt) || fontSizePt <= 0.)
    return false;
  if (!layer->labeling()) {
    const bool visible = layer->labelsEnabled();
    const bool ok = applyNameAttributeLabels(layer, currentLabelField(layer), fontSizePt,
                                             layer->geometryType() == Qgis::GeometryType::Polygon);
    if (ok) layer->setLabelsEnabled(visible);
    return ok;
  }
  // Keep expressions, rules, placement, color, buffers and visibility intact.
  // In particular an area-only label must not become an empty name field.
  std::unique_ptr<QgsAbstractVectorLayerLabeling> labeling(layer->labeling()->clone());
  if (!labeling) return false;
  for (const QString& provider : labeling->subProviders()) {
    auto settings = std::make_unique<QgsPalLayerSettings>(labeling->settings(provider));
    QgsTextFormat format = settings->format();
    QFont font = format.font();
    font.setPointSizeF(fontSizePt);
    format.setFont(font);
    format.setSize(fontSizePt);
    format.setSizeUnit(Qgis::RenderUnit::Points);
    settings->setFormat(format);
    // The explicit pt choice supersedes imported size expressions, while
    // unrelated data-defined label properties remain untouched.
    settings->dataDefinedProperties().setProperty(QgsPalLayerSettings::Property::Size, QgsProperty());
    settings->dataDefinedProperties().setProperty(QgsPalLayerSettings::Property::FontSizeUnit, QgsProperty());
    labeling->setSettings(settings.release(), provider); // QGIS takes ownership.
  }
  layer->setLabeling(labeling.release());
  layer->setCustomProperty(QStringLiteral("ka_hgis/label_font_size"), fontSizePt);
  layer->triggerRepaint();
  return true;
}

double LayerOps::labelFontSize(const QgsVectorLayer* layer, double defaultSize) {
  if (!layer) return defaultSize;
  const QVariant v = layer->customProperty(QStringLiteral("ka_hgis/label_font_size"));
  if (v.isValid() && v.toDouble() > 0.0)
    return v.toDouble();
  if (layer->labeling()) {
    const QgsPalLayerSettings s = layer->labeling()->settings();
    if (s.format().size() > 0.0)
      return s.format().size();
  }
  return defaultSize;
}

bool LayerOps::labelShowArea(const QgsVectorLayer* layer, bool defaultShow) {
  if (!layer) return defaultShow;
  // Old drawing code enabled the expression but left an earlier false flag.
  // The checkbox must reflect what the current labeling actually displays.
  if (layer->labeling()) {
    const auto settings = layer->labeling()->settings();
    if (settings.isExpression && settings.fieldName.contains(QLatin1String("area($geometry)")))
      return true;
  }
  const QVariant v = layer->customProperty(QStringLiteral("ka_hgis/label_show_area"));
  if (v.isValid())
    return v.toBool();
  return defaultShow;
}

QString LayerOps::currentLabelField(const QgsVectorLayer* layer) {
  if (!layer) return {};
  const QString f = layer->customProperty(QStringLiteral("ka_hgis/label_field")).toString();
  if (!f.isEmpty()) return f;
  return detectNameField(layer);
}

bool LayerOps::applyAreaM2Labels(QgsVectorLayer* layer) {
  if (!layer || !layer->isValid()) return false;
  if (layer->geometryType() != Qgis::GeometryType::Polygon) return false;
  // Geometry expressions update themselves when a feature changes. Replacing
  // existing labeling here would discard the user's menu choices on each edit.
  if (layer->labeling()) return true;

  QgsPalLayerSettings s;
  s.drawLabels = true;
  s.fieldName = QStringLiteral("format_number(area($geometry), 2) || ' ㎡'");
  s.isExpression = true;
  s.placement = Qgis::LabelPlacement::OverPoint;
  s.setPolygonPlacementFlags(Qgis::LabelPolygonPlacementFlag::AllowPlacementInsideOfPolygon);

  QgsLabelObstacleSettings obs = s.obstacleSettings();
  obs.setIsObstacle(false);
  s.setObstacleSettings(obs);

  QgsTextFormat fmt;
  QFont font = fmt.font();
  font.setFamily(QStringLiteral("Malgun Gothic"));
  font.setPointSize(5);
  font.setBold(true);
  fmt.setFont(font);
  fmt.setSize(5);
  fmt.setSizeUnit(Qgis::RenderUnit::Points);
  fmt.setColor(QColor(15, 23, 42));
  QgsTextBufferSettings buf = fmt.buffer();
  buf.setEnabled(true);
  buf.setSize(0.8);
  buf.setColor(QColor(255, 255, 255, 230));
  fmt.setBuffer(buf);
  s.setFormat(fmt);

  layer->setLabeling(new QgsVectorLayerSimpleLabeling(s));
  layer->setLabelsEnabled(true);
  layer->setCustomProperty(QStringLiteral("ka_hgis/label_show_area"), true);
  layer->setCustomProperty(QStringLiteral("ka_hgis/label_font_size"), 5.0);
  layer->triggerRepaint();
  return true;
}

// 「레이어가 밑에 있으면 글자도 밑으로 간다」
//
// QGIS 는 레이어를 아래에서 위로 전부 그린 다음, 라벨만 한 번에 맨 위에 얹는다.
// 그래서 아래에 있는 지적도의 지번이 위에 있는 빨간 구역선 위로 올라와 보였다.
// 레이어 순서가 글자에는 안 먹히는 셈이다. 두 가지를 같이 걸어 순서를 지킨다.
//
//  1) 글자가 없는 레이어 중 아래에 글자 있는 레이어가 하나라도 있으면
//     rendering/renderAboveLabels 를 켠다. QGIS 가 그 레이어를 라벨 뒤에,
//     즉 글자 위에 그린다.
//  2) 라벨끼리는 zIndex 로 위 레이어가 이기게 한다.
//
// 예외는 딱 하나다: 글자를 가지면서 채움이 불투명한 면 레이어. 켜면 자기 채움이
// 자기 글자를 덮어 글자가 통째로 사라진다. 테두리만 있는 면·선·점 레이어는
// 글자가 있어도 켠다 — 자기 선이 자기 글자를 조금 스칠 뿐이고, 그래야 사용자가
// 말한 「밑에 있는 레이어는 무조건 글자도 밑으로」가 지켜진다.
static bool hidesOwnLabelsWhenDrawnLast(QgsVectorLayer* vl) {
  if (!vl || vl->geometryType() != Qgis::GeometryType::Polygon) return false;
  if (vl->opacity() < 0.9) return false;
  QgsFeatureRenderer* r = vl->renderer();
  if (!r) return false;
  QgsRenderContext ctx;
  const QgsSymbolList syms = r->symbols(ctx);
  for (QgsSymbol* sym : syms) {
    if (!sym || sym->type() != Qgis::SymbolType::Fill) continue;
    if (sym->opacity() < 0.9) continue;
    for (int i = 0; i < sym->symbolLayerCount(); ++i) {
      QgsSymbolLayer* sl = sym->symbolLayer(i);
      if (!sl || !sl->enabled()) continue;
      // 테두리만 그리는 면(브러시 없음)은 색이 진해도 아무것도 칠하지 않는다.
      // 이걸 안 보고 색만 봐서, 빨간 구역선처럼 속이 빈 면까지 「불투명」으로
      // 잘못 세고 예외에 넣었다.
      if (auto* fill = dynamic_cast<QgsSimpleFillSymbolLayer*>(sl)) {
        if (fill->brushStyle() == Qt::NoBrush) continue;
      }
      // 실제로 칠하는 채움이 진하면 그 위에 글자를 놓을 수 없다.
      if (sl->color().alphaF() >= 0.9) return true;
    }
  }
  return false;
}

// 2차 패스로 다시 그려도 화면이 달라지지 않으려면 심볼이 완전히 불투명해야 한다.
// 반투명 심볼을 두 번 그리면 색이 진해져 원래와 달라진다.
static bool paintsFullyOpaque(QgsVectorLayer* vl) {
  if (!vl) return false;
  if (vl->opacity() < 0.999) return false;
  QgsFeatureRenderer* r = vl->renderer();
  if (!r) return false;
  QgsRenderContext ctx;
  const QgsSymbolList syms = r->symbols(ctx);
  if (syms.isEmpty()) return false;
  for (QgsSymbol* sym : syms) {
    if (!sym) continue;
    if (sym->opacity() < 0.999) return false;
    for (int i = 0; i < sym->symbolLayerCount(); ++i) {
      QgsSymbolLayer* sl = sym->symbolLayer(i);
      if (!sl || !sl->enabled()) continue;
      if (auto* fill = dynamic_cast<QgsSimpleFillSymbolLayer*>(sl)) {
        // 브러시가 없거나 채움색이 완전 투명이면 속을 칠하지 않는다.
        // 이때는 테두리만 보면 된다. (알파 0 + Solid 인 「속 빈 면」 스타일이
        // 흔한데, 색만 보고 반투명으로 세는 바람에 대상에서 빠졌다.)
        if (fill->brushStyle() == Qt::NoBrush || fill->color().alphaF() <= 0.02) {
          if (fill->strokeStyle() != Qt::NoPen && fill->strokeColor().alphaF() < 0.98 &&
              fill->strokeColor().alphaF() > 0.02)
            return false;
          continue;
        }
      }
      // 아무것도 안 칠하거나(투명) 꽉 칠하거나(불투명) 둘 중 하나여야
      // 두 번 그려도 화면이 같다. 어중간한 반투명만 위험하다.
      const double a = sl->color().alphaF();
      if (a > 0.02 && a < 0.98) return false;
    }
  }
  return true;
}

// ── 라벨 순서 분석 (캐시) ────────────────────────────────────────────────
// 이 계산은 renderer()->symbols() 로 심볼을 복제해 보므로 레이어당 비용이 있다.
// 지도를 다시 그릴 때마다 돌면 40개 레이어에서 1.9ms 가 나가 프레임을 갉아먹는다
// (실측: perf_labelOrderAnalysisIsCheapPerRefresh). 결과는 레이어 순서·표시·스타일이
// 바뀌어야만 달라지므로, 그 상태를 서명으로 만들어 같으면 통째로 건너뛴다.
namespace {

QString labelOrderSignature(QgsLayerTree* root) {
  QString sig;
  if (!root) return sig;
  const QList<QgsLayerTreeLayer*> nodes = root->findLayers();
  sig.reserve(nodes.size() * 64);
  for (QgsLayerTreeLayer* n : nodes) {
    QgsMapLayer* ml = n ? n->layer() : nullptr;
    if (!ml) { sig += QLatin1Char('-'); continue; }
    sig += ml->id();
    sig += n->isVisible() ? QLatin1Char('1') : QLatin1Char('0');
    if (auto* vl = qobject_cast<QgsVectorLayer*>(ml)) {
      // 스타일·라벨을 고치면 QGIS 가 객체를 새로 만든다. 포인터가 곧 변경 표시다.
      sig += vl->labelsEnabled() ? QLatin1Char('L') : QLatin1Char('l');
      sig += QString::number(reinterpret_cast<quintptr>(vl->labeling()), 16);
      sig += QString::number(reinterpret_cast<quintptr>(vl->renderer()), 16);
      sig += QString::number(vl->opacity(), 'f', 3);
    }
    sig += QLatin1Char(';');
  }
  return sig;
}

struct LabelOrderCache {
  QPointer<QgsProject> project;
  QString sig;
  QList<QPointer<QgsMapLayer>> above;  // 맨 위가 앞
  bool valid = false;
};
LabelOrderCache g_labelOrder;

// 한 번의 순회로 둘 다 한다: 속성·zIndex 적용 + 덧그림 대상 수집.
// 바뀐 것이 있으면 true.
bool refreshLabelOrderCache(QgsProject* project) {
  if (!project) return false;
  QgsLayerTree* root = project->layerTreeRoot();
  if (!root) return false;
  const QString sig = labelOrderSignature(root);
  if (g_labelOrder.valid && g_labelOrder.project == project && g_labelOrder.sig == sig)
    return false;

  const QList<QgsLayerTreeLayer*> nodes = root->findLayers();  // 위 → 아래
  const int n = nodes.size();
  bool labeledBelow = false;
  bool changed = false;
  QList<QPointer<QgsMapLayer>> bottomUp;

  for (int i = n - 1; i >= 0; --i) {  // 아래 → 위
    QgsLayerTreeLayer* node = nodes[i];
    QgsMapLayer* ml = node ? node->layer() : nullptr;
    if (!ml || !ml->isValid()) continue;
    auto* vl = qobject_cast<QgsVectorLayer*>(ml);
    const bool hasLabels = vl && vl->labelsEnabled() && vl->labeling();
    const bool visible = node->isVisible();
    const bool hidesOwn = hasLabels && hidesOwnLabelsWhenDrawnLast(vl);

    // 1) 레이어 속성. 병렬 렌더러(composeImage)와 QGIS 데스크톱에서 쓰인다.
    const bool wantAbove = labeledBelow && !hidesOwn;
    const QVariant cur = ml->customProperty(QStringLiteral("rendering/renderAboveLabels"));
    if (!cur.isValid() || cur.toBool() != wantAbove) {
      ml->setCustomProperty(QStringLiteral("rendering/renderAboveLabels"), wantAbove);
      ml->triggerRepaint();
      changed = true;
    }

    // 2) 덧그림(2차 패스) 대상. 두 번 그려도 화면이 같은 벡터만.
    //    수치지형도는 회색 0.2mm 밑그림이라 라벨 위로 올릴 이유가 없다. 오히려
    //    선 16만 개를 한 번 더 그리느라 넓은 범위로 이동할 때 화면이 수십 초
    //    멎었다. 밑그림은 2차 패스에서 뺀다.
    const bool isBackdrop =
        !ml->customProperty(QStringLiteral("ka_hgis/topographic_group")).toString().isEmpty();
    if (visible && labeledBelow && vl && !hidesOwn && !isBackdrop && paintsFullyOpaque(vl))
      bottomUp.append(QPointer<QgsMapLayer>(ml));

    // 3) 라벨끼리는 위 레이어가 이긴다.
    if (hasLabels) {
      const double want = double(n - i);
      if (vl->labeling()->type() == QLatin1String("simple")) {
        QgsPalLayerSettings ls = vl->labeling()->settings();
        if (!qFuzzyCompare(ls.zIndex + 1.0, want + 1.0)) {
          ls.zIndex = want;
          vl->setLabeling(new QgsVectorLayerSimpleLabeling(ls));
          vl->triggerRepaint();
          changed = true;
        }
      }
      if (visible) labeledBelow = true;
    }
  }

  QList<QPointer<QgsMapLayer>> above;
  for (int i = bottomUp.size() - 1; i >= 0; --i)
    above.append(bottomUp[i]);

  g_labelOrder.project = project;
  g_labelOrder.above = above;
  // 위에서 setLabeling·customProperty 로 상태가 바뀌었을 수 있으니 서명을 다시 딴다.
  g_labelOrder.sig = labelOrderSignature(root);
  g_labelOrder.valid = true;
  return changed;
}

}  // namespace

QList<QgsMapLayer*> LayerOps::layersDrawnAboveLabels(QgsProject* project) {
  QList<QgsMapLayer*> out;
  if (!project) return out;
  refreshLabelOrderCache(project);
  if (!g_labelOrder.valid || g_labelOrder.project != project) return out;
  for (const QPointer<QgsMapLayer>& p : g_labelOrder.above) {
    if (p) out.append(p.data());
  }
  return out;
}

void LayerOps::applyLayerOrderToLabels(QgsProject* project, QgsMapCanvas* canvas) {
  if (refreshLabelOrderCache(project) && canvas)
    refreshCanvasIfIdle(canvas);
}


bool LayerOps::hasToggleableLabels(const QgsMapLayer* layer) {
  return LayerLabelControls::describe(layer).supported;
}

bool LayerOps::labelsVisible(const QgsMapLayer* layer) {
  const auto* vl = qobject_cast<const QgsVectorLayer*>(layer);
  return vl && vl->isValid() && vl->labelsEnabled();
}

bool LayerOps::setLabelsVisible(QgsMapLayer* layer, bool on) {
  return LayerLabelControls::setVisible(layer, on);
}

