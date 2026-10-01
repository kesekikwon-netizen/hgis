#pragma once

#include <QList>

#include <qgsmapcanvas.h>
#include <qgsproject.h>

#include "core/LayerOps.h"

// Layers a click on the map can reach. The canvas list alone is not enough: survey and
// heritage layers that are drawn above the labels are taken out of it (LayerOps refresh)
// and painted once by KaAboveLabelsOverlay, so a tool reading only QgsMapCanvas::layers()
// cannot pick them at all. Lifted layers come first because they are painted on top.
inline QList<QgsMapLayer*> kaPickLayers(const QgsMapCanvas* canvas) {
  QList<QgsMapLayer*> layers = LayerOps::layersDrawnAboveLabels(QgsProject::instance());
  if (!canvas) return layers;
  const QList<QgsMapLayer*> painted = canvas->layers();
  for (QgsMapLayer* layer : painted)
    if (!layers.contains(layer)) layers.append(layer);
  return layers;
}
