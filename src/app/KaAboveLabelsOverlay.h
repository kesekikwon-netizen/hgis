#pragma once

#include <qgsmapcanvasitem.h>

#include <QImage>
#include <QList>
#include <QPointer>
#include <QSize>
#include <QString>
#include <memory>

#include <qgsrectangle.h>

class QObject;
class QgsMapLayer;
class QgsMapCanvas;

// 라벨 위에 한 번 더 그리는 덧그림.
//
// QGIS 는 한 번의 렌더에서 도형을 모두 그린 다음 라벨을 맨 위에 한꺼번에 얹는다.
// 그래서 레이어 순서가 글자에는 먹지 않아, 아래 지적도의 지번이 위 경계선을 가렸다.
// 레이어의 rendering/renderAboveLabels 속성은 병렬 렌더러가 이미지를 합칠 때만
// (QgsMapRendererJob::composeImage) 쓰인다. 이 앱은 WMS 크래시를 피하려고 순차
// 렌더(QgsMapRendererCustomPainterJob)를 쓰므로 그 속성이 무시된다.
//
// 본 화면 목록에서는 이 대상이 빠져 있다. 이 항목이 그 도형을 한 번만 그리고,
// 글자가 있는 레이어는 심볼 없이 이름만 얹는다. 지적 지번은 본 화면에 남는다.
// 대상은 LayerOps::layersDrawnAboveLabels. 지질·지형도·위성은 넣지 않는다.
//
// The cached picture is drawn at the canvas device pixel ratio (125-200 % field
// screens stay sharp) and is rebuilt only when its inputs change: the layer list,
// a watched layer's style/data/edit signal, or the extent, size, DPR, CRS or
// rotation. A finished base-map render alone does not redraw it.
class KaAboveLabelsOverlay : public QgsMapCanvasItem {
public:
  explicit KaAboveLabelsOverlay(QgsMapCanvas* canvas);
  ~KaAboveLabelsOverlay() override;

  // 맨 위가 앞. 비면 아무것도 그리지 않는다.
  void setLayers(const QList<QgsMapLayer*>& layers);
  bool isEmpty() const { return m_layers.isEmpty(); }

  void updatePosition() override;

  // Tests and diagnostics: physical cache size, its DPR, and how often it was rebuilt.
  QSize cachePixelSize() const { return m_cache.size(); }
  qreal cacheDevicePixelRatio() const { return m_cache.isNull() ? 0. : m_cache.devicePixelRatio(); }
  int rebuildCount() const { return m_rebuilds; }

protected:
  void paint(QPainter* painter) override;

private:
  QList<QgsMapLayer*> currentLayers() const;
  void syncWithProject();
  void watchLayers();
  void markDirty();
  QString cacheKey() const;
  void rebuildCache(qreal dpr);

  QList<QPointer<QgsMapLayer>> m_layers;
  // 화면을 칠할 때마다 지도를 다시 그리면 팬·줌이 멎는다. 결과를 담아 두고
  // 입력이 바뀌었을 때만 새로 만든다.
  QImage m_cache;
  QString m_cacheKey;
  QSize m_cacheSize;
  bool m_dirty = true;
  bool m_painting = false;
  int m_rebuilds = 0;
  // Connection owners: destroyed with the item so no signal reaches a dead overlay.
  std::unique_ptr<QObject> m_canvasContext;
  std::unique_ptr<QObject> m_layerContext;
};
