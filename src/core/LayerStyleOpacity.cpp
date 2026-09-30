// Layer transparency for every kind of layer (evaluation F018).
//
// Opacity is display only. It is decided apart from the edit rules
// (isReferenceOrBasemapLayer / isReferenceLayer): a survey polygon, an imported
// SHP and a background picture can all be made see-through, so a 유구면 can be
// read against the satellite picture without changing the data or its colours.
#include "LayerOps.h"

#include <qgsmapcanvas.h>
#include <qgsmaplayer.h>

QString LayerOps::opacityUnavailableReason(const QgsMapLayer* layer) {
  if (!layer) return QStringLiteral("레이어를 먼저 선택하세요.");
  if (!layer->isValid())
    return QStringLiteral("레이어 파일을 찾을 수 없습니다. 자료를 다시 열어 주세요.");
  return {};
}

bool LayerOps::canAdjustOpacity(const QgsMapLayer* layer) {
  return opacityUnavailableReason(layer).isEmpty();
}

bool LayerOps::applyLayerOpacity(QgsMapLayer* layer, double opacity, QgsMapCanvas* canvas) {
  if (!canAdjustOpacity(layer)) return false;
  layer->setOpacity(qBound(0.0, opacity, 1.0));
  layer->triggerRepaint();
  if (canvas) refreshCanvasIfIdle(canvas);
  return true;
}
