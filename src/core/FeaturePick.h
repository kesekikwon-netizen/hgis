#pragma once

#include <QList>

#include <qgsfeatureid.h>

class QgsCoordinateReferenceSystem;
class QgsCoordinateTransformContext;
class QgsMapLayer;
class QgsPointXY;
class QgsVectorLayer;

// 지도에서 찍은 자리의 도형 하나를 고른다.
//
// 예전에는 위 레이어에서 처음 걸린 도형을 그대로 골랐다. 그래서 큰 면 위에 그린 작은 면과
// 조사구역 안의 유구는 찍어도 바깥 큰 면만 잡혔고, 그려 놓고 아직 저장하지 않은 도형
// (편집 버퍼의 음수 번호)은 아예 잡히지 않았다.
namespace FeaturePick {

struct Hit {
  QgsVectorLayer* layer = nullptr;
  QgsFeatureId fid = FID_NULL;
  bool valid() const { return layer && !FID_IS_NULL(fid); }
};

// layers 는 위에 그려지는 순서. mapPoint·mapTolerance 는 지도 좌표계(mapCrs) 기준.
// 고르는 순서: 조사 도형이 참조·지적보다 먼저, 그 안에서는
//   찍은 자리 가까이의 점·선 → 찍은 자리를 품은 가장 작은 면 → 테두리가 가까운 면.
Hit at(const QList<QgsMapLayer*>& layers, const QgsPointXY& mapPoint, double mapTolerance,
       const QgsCoordinateReferenceSystem& mapCrs, const QgsCoordinateTransformContext& context);

}  // namespace FeaturePick
