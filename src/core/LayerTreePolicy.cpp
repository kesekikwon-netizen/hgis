#include "LayerTreePolicy.h"

#include "LayerOps.h"

#include <qgsmaplayer.h>
#include <qgsproject.h>

QString LayerTreePolicy::forcedOrderNote(const QgsMapLayer* layer, QgsProject* project) {
  if (!layer) return {};
  // Same rules as LayerOps::visibleLayersPaintOrder and layersDrawnAboveLabels.
  if (layer->name().contains(QStringLiteral("위성")))
    return QStringLiteral("그리기 순서 고정: 위성 사진은 목록 순서와 관계없이 맨 아래에 그립니다.");
  if (LayerOps::isBasemapLayer(layer))
    return QStringLiteral("그리기 순서 고정: 배경 지도는 목록 순서와 관계없이 조사·참조 자료 아래에 그립니다.");
  if (LayerOps::isCadastralLayer(layer))
    return QStringLiteral("그리기 순서 고정: 지적 선과 지번은 본 지도에 그리고, 조사 도형은 그 위에 한 번 더 그립니다.");
  if (project && LayerOps::layersDrawnAboveLabels(project).contains(const_cast<QgsMapLayer*>(layer)))
    return QStringLiteral("그리기 순서 고정: 글자를 가리지 않도록 지도 위에 한 번 더 그립니다. "
                          "목록에서 아래로 내려도 그림(래스터)보다 위에 보입니다.");
  return {};
}
