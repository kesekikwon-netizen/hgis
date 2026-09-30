// 「레이어가 밑에 있으면 글자도 밑으로 간다」
//
// QGIS 는 레이어를 아래에서 위로 전부 그린 다음, 라벨만 한 번에 맨 위에 얹는다.
// 그래서 아래에 있는 지적도의 지번이 위에 있는 빨간 구역선 위로 올라와 보였다.
// 두 가지를 같이 걸어 순서를 지킨다.
//
//  1) 글자가 없는 레이어 중 아래에 글자 있는 레이어가 하나라도 있으면
//     rendering/renderAboveLabels 를 켠다. QGIS 가 그 레이어를 라벨 뒤에 그린다.
//  2) 라벨끼리는 zIndex 로 위 레이어가 이긴다.
//
// 예외는 글자를 가지면서 채움이 불투명한 면 레이어다. 켜면 자기 채움이 자기 글자를
// 덮는다. 테두리만 있는 면·선·점 레이어는 글자가 있어도 켠다.
//
// Query and apply are separate (evaluation F218): layersDrawnAboveLabels() and
// sheetBasePaintLayers() only read layer state; applyLayerOrderToLabels() is the
// one place that writes renderAboveLabels and label zIndex, and it writes once
// after each re-analysis, never on a cache hit (that is the per-refresh path).
// The analysis is cached by a signature of the cheap state that changes without a
// signal (tree order, node visibility, the labelsEnabled switch, whether a
// labeling object exists) plus a generation counter that watched layers bump from
// their own signals (style, repaint, opacity, custom property, name, data source,
// destruction). No object addresses are compared. setLabeling() emits nothing;
// every app path calls triggerRepaint() right after it, and that repaint is the
// invalidation.
#include "LayerOps.h"
#include "LayerLabelControls.h"

#include <QPointer>
#include <QSet>
#include <algorithm>
#include <vector>

#include <qgsfillsymbollayer.h>
#include <qgslayertree.h>
#include <qgslayertreelayer.h>
#include <qgspallabeling.h>
#include <qgsproject.h>
#include <qgsrendercontext.h>
#include <qgsrenderer.h>
#include <qgssymbol.h>
#include <qgssymbollayer.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayerlabeling.h>

namespace {

bool hidesOwnLabelsWhenDrawnLast(QgsVectorLayer* vl) {
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
      if (auto* fill = dynamic_cast<QgsSimpleFillSymbolLayer*>(sl)) {
        if (fill->brushStyle() == Qt::NoBrush) continue;
      }
      if (sl->color().alphaF() >= 0.9) return true;
    }
  }
  return false;
}

// 2차 패스로 다시 그려도 화면이 달라지지 않으려면 심볼이 완전히 불투명해야 한다.
bool paintsFullyOpaque(QgsVectorLayer* vl) {
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
        // 브러시가 없거나 채움이 완전 투명이면 속을 칠하지 않는다. 테두리만 본다.
        if (fill->brushStyle() == Qt::NoBrush || fill->color().alphaF() <= 0.02) {
          if (fill->strokeStyle() != Qt::NoPen && fill->strokeColor().alphaF() < 0.98 &&
              fill->strokeColor().alphaF() > 0.02)
            return false;
          continue;
        }
      }
      // 투명하거나 불투명해야 두 번 그려도 같다. 어중간한 반투명만 위험하다.
      const double a = sl->color().alphaF();
      if (a > 0.02 && a < 0.98) return false;
    }
  }
  return true;
}

// Bumped by any watched vector layer whose look may have changed, and when a
// watched layer dies: a reopened survey reuses layer ids, so without that bump an
// unchanged signature could keep the cache pointing at the old objects.
quint64 g_generation = 0;

// Watched layers by identity; the destroyed() hook removes them, so a new layer
// at a reused address is watched again. (A dynamic property was looked up per
// layer per refresh before; this is one hash probe.)
QSet<const QObject*>& watchedLayers() {
  // Never destroyed: a layer deleted during static teardown still runs its hook.
  static auto* const set = new QSet<const QObject*>();
  return *set;
}

void watchLayer(QgsVectorLayer* vl) {
  if (!vl || watchedLayers().contains(vl)) return;
  watchedLayers().insert(vl);
  const auto bump = [] { ++g_generation; };
  // The layer is the context: connections end with the layer.
  QObject::connect(vl, &QgsMapLayer::rendererChanged, vl, bump);
  QObject::connect(vl, &QgsMapLayer::styleChanged, vl, bump);
  QObject::connect(vl, &QgsMapLayer::repaintRequested, vl, bump);
  QObject::connect(vl, &QgsMapLayer::opacityChanged, vl, bump);
  QObject::connect(vl, &QgsMapLayer::customPropertyChanged, vl, bump);
  QObject::connect(vl, &QgsMapLayer::nameChanged, vl, bump);        // legacy title roles
  QObject::connect(vl, &QgsMapLayer::dataSourceChanged, vl, bump);  // validity
  QObject::connect(vl, &QObject::destroyed, vl, [](QObject* gone) {
    watchedLayers().remove(gone);
    ++g_generation;
  });
}

// Only the state that changes without a signal goes in here; everything a
// watched layer reports itself (style, opacity, role properties, name) comes in
// through the generation number at the end.
QString signatureOf(QgsLayerTree* root) {
  QString sig;
  if (!root) return sig;
  const QList<QgsLayerTreeLayer*> nodes = root->findLayers();
  sig.reserve(nodes.size() * 32 + 24);
  for (QgsLayerTreeLayer* n : nodes) {
    QgsMapLayer* ml = n ? n->layer() : nullptr;
    if (!ml) { sig += QLatin1Char('-'); continue; }
    sig += ml->id();
    sig += n->isVisible() ? QLatin1Char('1') : QLatin1Char('0');
    if (auto* vl = qobject_cast<QgsVectorLayer*>(ml)) {
      watchLayer(vl);
      sig += vl->labelsEnabled() ? QLatin1Char('L') : QLatin1Char('l');
      sig += vl->labeling() ? QLatin1Char('S') : QLatin1Char('s');
    }
    sig += QLatin1Char(';');
  }
  sig += QString::number(g_generation);
  return sig;
}

struct Want {
  QPointer<QgsMapLayer> layer;
  bool renderAbove = false;
  bool labeled = false;
  double zIndex = 0.;
};

struct LabelOrderCache {
  QPointer<QgsProject> project;
  QString sig;
  QList<QPointer<QgsMapLayer>> above;  // 맨 위가 앞
  QList<Want> wants;
  bool applied = false;  // wants were written to the layers since the last analysis
};

// One entry per live project (the map and a layout copy can alternate without
// thrashing one shared slot). GUI thread only, like the layer tree it reads.
LabelOrderCache* cacheFor(QgsProject* project) {
  static std::vector<LabelOrderCache> caches;
  caches.erase(std::remove_if(caches.begin(), caches.end(),
                              [](const LabelOrderCache& c) { return c.project.isNull(); }),
               caches.end());
  for (LabelOrderCache& cache : caches)
    if (cache.project == project) return &cache;
  if (caches.size() >= 8) caches.erase(caches.begin());
  caches.push_back(LabelOrderCache{QPointer<QgsProject>(project), QString(), {}, {}});
  return &caches.back();
}

// Pure analysis: reads layer state only. Returns the project's up-to-date entry.
LabelOrderCache* analyse(QgsProject* project) {
  QgsLayerTree* root = project ? project->layerTreeRoot() : nullptr;
  if (!root) return nullptr;
  const QString sig = signatureOf(root);
  LabelOrderCache* cache = cacheFor(project);
  if (!cache->sig.isEmpty() && cache->sig == sig) return cache;

  const QList<QgsLayerTreeLayer*> nodes = root->findLayers();  // 위 → 아래
  const int n = nodes.size();
  bool labeledBelow = false;
  QList<QPointer<QgsMapLayer>> bottomUp;
  QList<Want> wants;
  for (int i = n - 1; i >= 0; --i) {  // 아래 → 위
    QgsLayerTreeLayer* node = nodes[i];
    QgsMapLayer* ml = node ? node->layer() : nullptr;
    if (!ml || !ml->isValid()) continue;
    auto* vl = qobject_cast<QgsVectorLayer*>(ml);
    const bool hasLabels = vl && vl->labelsEnabled() && vl->labeling();
    const bool visible = node->isVisible();
    const bool hidesOwn = hasLabels && hidesOwnLabelsWhenDrawnLast(vl);
    wants.append({QPointer<QgsMapLayer>(ml), labeledBelow && !hidesOwn, hasLabels, double(n - i)});

    // 덧그림(2차 패스) 대상. 두 번 그려도 화면이 같은 벡터만. 참조 지도는 그림이라
    // 한 번만 그린다. 수치지형도는 선이 많아 다시 그리면 멎는다. 주변유적은 참조지만
    // 지적 덧그림 위에 다시 그린다. 지적도는 본 화면에 남는다.
    const bool isBackdrop =
        !ml->customProperty(QStringLiteral("ka_hgis/topographic_group")).toString().isEmpty();
    const bool keepHeritage = LayerLabelControls::isHeritage(vl);
    const bool cadastral = LayerOps::isCadastralLayer(vl);
    if (visible && vl && !hidesOwn && !isBackdrop && !cadastral && paintsFullyOpaque(vl) &&
        (!LayerOps::isReferenceLayer(vl) || keepHeritage) && (labeledBelow || keepHeritage))
      bottomUp.append(QPointer<QgsMapLayer>(ml));
    if (hasLabels && visible) labeledBelow = true;
  }
  QList<QPointer<QgsMapLayer>> above;
  for (int i = bottomUp.size() - 1; i >= 0; --i) above.append(bottomUp[i]);
  cache->sig = sig;
  cache->above = above;
  cache->wants = wants;
  cache->applied = false;
  return cache;
}

}  // namespace

QList<QgsMapLayer*> LayerOps::layersDrawnAboveLabels(QgsProject* project) {
  QList<QgsMapLayer*> out;
  const LabelOrderCache* cache = analyse(project);
  if (!cache) return out;
  for (const QPointer<QgsMapLayer>& p : cache->above) {
    if (p) out.append(p.data());
  }
  return out;
}

QList<QgsMapLayer*> LayerOps::sheetBasePaintLayers(QgsProject* project) {
  QList<QgsMapLayer*> out;
  if (!project) return out;
  const QList<QgsMapLayer*> visible = visibleLayersPaintOrder(project);
  const QList<QgsMapLayer*> above = layersDrawnAboveLabels(project);
  const QSet<QgsMapLayer*> skip(above.begin(), above.end());
  for (QgsMapLayer* layer : visible) {
    // 덧그림에 올라간 조사·유적은 본지도에서 한 번 더 그리지 않는다.
    if (layer && !skip.contains(layer)) out.append(layer);
  }
  return out;
}

void LayerOps::applyLayerOrderToLabels(QgsProject* project, QgsMapCanvas* canvas) {
  LabelOrderCache* cache = analyse(project);
  // Nothing changed since the last apply: the layers already carry this order.
  // This is the per-refresh path, so it must stay at the cost of the signature;
  // re-checking every label setting here copies a QgsPalLayerSettings (callout
  // and all) per labelled layer and doubled the refresh time on 40 layers.
  if (!cache || cache->applied) return;
  cache->applied = true;
  bool changed = false;
  const QList<Want> wants = cache->wants;  // writes below may re-enter the analysis
  for (const Want& want : wants) {
    QgsMapLayer* ml = want.layer.data();
    if (!ml) continue;
    // 1) Used by the parallel renderer (composeImage) and QGIS desktop.
    const QVariant cur = ml->customProperty(QStringLiteral("rendering/renderAboveLabels"));
    if (!cur.isValid() || cur.toBool() != want.renderAbove) {
      ml->setCustomProperty(QStringLiteral("rendering/renderAboveLabels"), want.renderAbove);
      ml->triggerRepaint();
      changed = true;
    }
    // 2) 라벨끼리는 위 레이어가 이긴다. A labeling replaced without a signal is
    // caught by the repaint every app path issues after setLabeling().
    auto* vl = qobject_cast<QgsVectorLayer*>(ml);
    if (!want.labeled || !vl || !vl->labeling() || vl->labeling()->type() != QLatin1String("simple"))
      continue;
    QgsPalLayerSettings settings = vl->labeling()->settings();
    if (!qFuzzyCompare(settings.zIndex + 1.0, want.zIndex + 1.0)) {
      settings.zIndex = want.zIndex;
      vl->setLabeling(new QgsVectorLayerSimpleLabeling(settings));
      vl->triggerRepaint();
      changed = true;
    }
  }
  // The writes above bumped the generation; re-sign so the next query is a cache hit.
  if (changed) {
    if (LabelOrderCache* fresh = cacheFor(project)) {
      fresh->sig = signatureOf(project->layerTreeRoot());
      fresh->applied = true;
    }
  }
  if (changed && canvas) refreshCanvasIfIdle(canvas);
}
