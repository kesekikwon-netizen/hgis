#include "KaAboveLabelsOverlay.h"

#include "core/LayerOps.h"

#include <QObject>
#include <QPainter>

#include <qgsmapcanvas.h>
#include <qgsmaplayer.h>
#include <qgsmaprenderercustompainterjob.h>
#include <qgsmapsettings.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

KaAboveLabelsOverlay::KaAboveLabelsOverlay(QgsMapCanvas* canvas)
    : QgsMapCanvasItem(canvas), m_canvasContext(std::make_unique<QObject>()),
      m_layerContext(std::make_unique<QObject>()) {
  // 좌표격자(1000)보다 아래, 지도보다는 위. 격자와 눈금은 계속 맨 위에 남는다.
  setZValue(900);
  setVisible(false);
  // 지도가 다시 그려지면 대상 목록만 다시 읽는다(조회만 하는 함수). 그림은 목록이나
  // 레이어가 실제로 바뀌었을 때만 다시 만든다.
  if (canvas)
    QObject::connect(canvas, &QgsMapCanvas::renderComplete, m_canvasContext.get(),
                     [this](QPainter*) { syncWithProject(); });
}

KaAboveLabelsOverlay::~KaAboveLabelsOverlay() = default;

void KaAboveLabelsOverlay::syncWithProject() {
  const QList<QgsMapLayer*> above = LayerOps::layersDrawnAboveLabels(QgsProject::instance());
  bool same = above.size() == m_layers.size();
  for (int i = 0; same && i < above.size(); ++i) same = m_layers.at(i).data() == above.at(i);
  if (same) {
    if (!above.isEmpty() && !isVisible()) setVisible(true);
    return;
  }
  // Layers handed in directly (not in the project) are the caller's to replace.
  bool fromProject = false;
  for (const QPointer<QgsMapLayer>& layer : std::as_const(m_layers))
    fromProject = fromProject || (layer && QgsProject::instance()->mapLayer(layer->id()) == layer.data());
  if (above.isEmpty() && !fromProject && !m_layers.isEmpty()) return;
  setLayers(above);
}

QList<QgsMapLayer*> KaAboveLabelsOverlay::currentLayers() const {
  QList<QgsMapLayer*> chosen;
  for (const QPointer<QgsMapLayer>& layer : m_layers) {
    if (layer) chosen.append(layer.data());
  }
  return chosen;
}

void KaAboveLabelsOverlay::watchLayers() {
  // A fresh context drops the previous layer connections at once.
  m_layerContext = std::make_unique<QObject>();
  QObject* context = m_layerContext.get();
  const auto dirty = [this] { markDirty(); };
  for (const QPointer<QgsMapLayer>& layer : std::as_const(m_layers)) {
    if (!layer) continue;
    QgsMapLayer* watched = layer.data();
    QObject::connect(watched, &QgsMapLayer::repaintRequested, context, dirty);
    QObject::connect(watched, &QgsMapLayer::rendererChanged, context, dirty);
    QObject::connect(watched, &QgsMapLayer::styleChanged, context, dirty);
    QObject::connect(watched, &QgsMapLayer::dataChanged, context, dirty);
    QObject::connect(watched, &QgsMapLayer::opacityChanged, context, dirty);
    QObject::connect(watched, &QgsMapLayer::blendModeChanged, context, dirty);
    QObject::connect(watched, &QgsMapLayer::crsChanged, context, dirty);
    QObject::connect(watched, &QObject::destroyed, context, dirty);
    if (auto* vector = qobject_cast<QgsVectorLayer*>(watched)) {
      // Edit buffer changes, undo/redo, commit and roll back.
      QObject::connect(vector, &QgsVectorLayer::layerModified, context, dirty);
      QObject::connect(vector, &QgsVectorLayer::afterRollBack, context, dirty);
      QObject::connect(vector, &QgsVectorLayer::afterCommitChanges, context, dirty);
    }
  }
}

void KaAboveLabelsOverlay::markDirty() {
  if (m_dirty) return;
  m_dirty = true;
  update();
}

void KaAboveLabelsOverlay::setLayers(const QList<QgsMapLayer*>& layers) {
  QList<QgsMapLayer*> wanted;
  for (QgsMapLayer* l : layers) {
    if (l) wanted.append(l);
  }
  if (wanted == currentLayers() && wanted.size() == m_layers.size()) {
    // Same list (the label stack is re-applied often): keep the picture.
    setVisible(!m_layers.isEmpty());
    return;
  }
  m_layers.clear();
  for (QgsMapLayer* l : layers) {
    if (l) m_layers.append(QPointer<QgsMapLayer>(l));
  }
  watchLayers();
  setVisible(!m_layers.isEmpty());
  m_dirty = true;
  update();
}

void KaAboveLabelsOverlay::updatePosition() {
  if (!mMapCanvas) return;
  setRect(mMapCanvas->extent());
}

QString KaAboveLabelsOverlay::cacheKey() const {
  const QgsMapSettings& ms = mMapCanvas->mapSettings();
  const QgsRectangle ext = mMapCanvas->extent();
  return QStringLiteral("%1|%2|%3|%4|%5|%6")
      .arg(ext.toString(9), QString::number(mMapCanvas->width()), QString::number(mMapCanvas->height()),
           QString::number(ms.devicePixelRatio(), 'f', 3), ms.destinationCrs().authid(),
           QString::number(ms.rotation(), 'f', 4));
}

void KaAboveLabelsOverlay::paint(QPainter* painter) {
  if (!painter || !mMapCanvas || m_layers.isEmpty()) return;
  // 렌더 안에서 다시 이 항목이 그려지는 일이 없도록 막는다.
  if (m_painting) return;
  if (currentLayers().isEmpty()) return;

  const QSize logical = mMapCanvas->size();
  if (logical.width() < 2 || logical.height() < 2) return;
  // The base map image uses the canvas map settings' DPR; the overlay matches it.
  float dpr = mMapCanvas->mapSettings().devicePixelRatio();
  if (!(dpr > 0.05f)) dpr = static_cast<float>(mMapCanvas->devicePixelRatioF());
  if (!(dpr > 0.05f)) dpr = 1.0f;

  // 입력이 그대로면 담아 둔 그림을 쓴다. 마우스가 움직일 때마다 다시 그리면 화면이 멎는다.
  const QString key = cacheKey();
  if (m_dirty || m_cache.isNull() || m_cacheSize != logical || m_cacheKey != key) {
    m_cacheSize = logical;
    m_cacheKey = key;
    rebuildCache(dpr);
    m_dirty = false;
  }
  if (!m_cache.isNull())
    painter->drawImage(QPointF(0, 0), m_cache);
}

void KaAboveLabelsOverlay::rebuildCache(qreal dpr) {
  m_cache = QImage();
  if (!mMapCanvas || m_layers.isEmpty()) return;
  const QList<QgsMapLayer*> live = currentLayers();
  if (live.isEmpty()) return;
  ++m_rebuilds;

  QgsMapSettings ms = mMapCanvas->mapSettings();
  ms.setLayers(live);
  ms.setBackgroundColor(Qt::transparent);
  // 도형은 여기서 한 번만 그린다. 지번은 본 화면에 이미 있다.
  ms.setFlag(Qgis::MapSettingsFlag::DrawLabeling, false);
  ms.setOutputSize(m_cacheSize);
  ms.setDevicePixelRatio(static_cast<float>(dpr));

  // Same allocation as QgsMapRendererSequentialJob: physical pixels, logical painter.
  QImage img(ms.deviceOutputSize(), QImage::Format_ARGB32_Premultiplied);
  if (img.isNull()) return;
  img.setDevicePixelRatio(dpr);
  img.setDotsPerMeterX(static_cast<int>(1000 * ms.outputDpi() / 25.4));
  img.setDotsPerMeterY(static_cast<int>(1000 * ms.outputDpi() / 25.4));
  img.fill(Qt::transparent);
  QPainter p(&img);
  m_painting = true;
  // 페인트 안에서는 start()+waitForFinished() 로 배경 스레드를 기다리면 안 된다.
  // renderSynchronously() 가 이 자리(주 스레드)에서 그리라고 만들어진 API 다.
  // The vector-only pass stays sequential like the base map (WMS crash class rule).
  QgsMapRendererCustomPainterJob job(ms, &p);
  job.renderSynchronously();
  QList<QgsMapLayer*> labeled;
  for (QgsMapLayer* layer : live) {
    auto* vector = qobject_cast<QgsVectorLayer*>(layer);
    if (vector && vector->labelsEnabled() && vector->labeling())
      labeled.append(vector);
  }
  if (!labeled.isEmpty()) {
    QgsMapSettings labelSettings = ms;
    labelSettings.setLayers(labeled);
    labelSettings.setFlag(Qgis::MapSettingsFlag::DrawLabeling, true);
    labelSettings.setFlag(Qgis::MapSettingsFlag::SkipSymbolRendering, true);
    QgsMapRendererCustomPainterJob labelJob(labelSettings, &p);
    labelJob.renderSynchronously();
  }
  m_painting = false;
  p.end();
  m_cache = img;
}
