#pragma once

#include <QList>

#include <qgsfeatureid.h>

#include "PolygonPieces.h"

class QgsCoordinateReferenceSystem;
class QgsCoordinateTransformContext;
class QgsMapLayer;
class QgsPointXY;
class QgsVectorLayer;

// 지도에서 찍은 자리의 도형을 고른다(도형선택).
//
// 예전에는 위 레이어에서 처음 걸린 도형을 그대로 골랐다. 그래서 큰 면 위에 그린 작은 면과
// 조사구역 안의 유구는 찍어도 바깥 큰 면만 잡혔고, 그려 놓고 아직 저장하지 않은 도형
// (편집 버퍼의 음수 번호)은 아예 잡히지 않았다. 한 도형 안의 안쪽 조각(구멍, 다른 조각 안의
// 조각)도 따로 고를 수 없었다.
namespace FeaturePick {

struct Hit {
  QgsVectorLayer* layer = nullptr;
  QgsFeatureId fid = FID_NULL;
  // Valid when the click picked an inner piece of the shape (a hole, or a part lying inside
  // another part) rather than the whole shape.
  PolygonPieces::Piece piece;
  bool valid() const { return layer && !FID_IS_NULL(fid); }
};

// Survey data that 도형선택 edits. Reference maps, basemaps and cadastral layers are read-only:
// they are picked only where no survey shape lies under the click, and never get handles.
bool isSurveyLayer(const QgsVectorLayer* layer);

// Every shape under the click, best first. layers 는 위에 그려지는 순서. mapPoint·mapTolerance 는
// 지도 좌표계(mapCrs) 기준. 고르는 순서:
//   조사 도형이 참조·지적보다 먼저(조사 도형이 하나라도 걸리면 참조·지적은 목록에서 뺀다),
//   그 안에서는 찍은 자리 가까이의 점·선 → 찍은 자리를 품은 가장 작은 면이나 안쪽 조각 →
//   테두리가 가까운 면. 같은 자리를 다시 찍으면 다음 후보로 넘어가게 쓰는 목록이다.
// Unsaved shapes (negative edit-buffer ids) and shapes whose geometry was changed in the edit
// buffer count like saved ones, also when the click falls in one of their holes.
QList<Hit> candidates(const QList<QgsMapLayer*>& layers, const QgsPointXY& mapPoint, double mapTolerance,
                      const QgsCoordinateReferenceSystem& mapCrs, const QgsCoordinateTransformContext& context);

// The best candidate, or an invalid hit.
Hit at(const QList<QgsMapLayer*>& layers, const QgsPointXY& mapPoint, double mapTolerance,
       const QgsCoordinateReferenceSystem& mapCrs, const QgsCoordinateTransformContext& context);

}  // namespace FeaturePick
