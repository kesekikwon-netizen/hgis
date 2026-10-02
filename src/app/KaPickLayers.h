#pragma once

#include <QList>

#include <qgsmapcanvas.h>
#include <qgsproject.h>

#include "core/LayerOps.h"

// Layers a click on the map can reach. The canvas list alone is not enough: survey and
// heritage layers that are drawn above the labels are taken out of it (LayerOps refresh)
// and painted once by KaAboveLabelsOverlay, so a tool reading only QgsMapCanvas::layers()
// cannot pick them at all. Lifted layers come first because they are painted on top, then
// every layer visible in the legend in paint order, then anything else on the canvas.
inline QList<QgsMapLayer*> kaPickLayers(const QgsMapCanvas* canvas) {
  QgsProject* project = QgsProject::instance();
  QList<QgsMapLayer*> layers = LayerOps::layersDrawnAboveLabels(project);
  const auto addMissing = [&layers](const QList<QgsMapLayer*>& more) {
    for (QgsMapLayer* layer : more)
      if (layer && !layers.contains(layer)) layers.append(layer);
  };
  addMissing(LayerOps::visibleLayersPaintOrder(project));
  if (canvas) addMissing(canvas->layers());
  return layers;
}
