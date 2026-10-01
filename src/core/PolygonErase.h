#pragma once

#include <QList>
#include <QPointer>
#include <QString>

#include <qgsfeatureid.h>
#include <qgsvectorlayer.h>

class QgsGeometry;
class QgsProject;

// 「겹친 곳 지우기」: 위에 그린 면 모양대로 아래 면에서 그 자리만 지운다. 위에 그린 면은 없앤다.
//
// 조사구역 안에서 빼야 하는 곳(제외 구역)을 그 위에 면으로 그린 뒤 쓰는 기능이다.
// 예전 「폴리곤 나누기」는 겹친 자리를 새 도형으로 더하기만 했고, 「구간 분리」는 겹친 자리를
// 새 레이어로 떼어 낼 뿐이어서 아래 면은 그대로 남았다.
namespace PolygonErase {

struct Shape {
  QPointer<QgsVectorLayer> layer;
  QgsFeatureId fid = FID_NULL;
};

struct Plan {
  QList<Shape> cutters;  // 모양을 빌려 쓰고 없앨 면
  QList<Shape> targets;  // 그 자리가 지워질 면
  QString hint;          // 할 수 없을 때 사용자에게 보여 줄 이유
  bool ready() const { return hint.isEmpty() && !cutters.isEmpty() && !targets.isEmpty(); }
};

struct Outcome {
  bool ok = false;
  QString error;
  int erased = 0;  // 자리가 지워진 면의 수
  int added = 0;   // 둘 이상으로 갈라져 새로 생긴 도형의 수
  // 편집 명령이 들어간 레이어. 둘 이상이면 되돌리기도 함께 해야 한다.
  QList<QPointer<QgsVectorLayer>> layers;
};

// 이 레이어의 면을 고쳐도 되는가. 지적도·참조 지도·읽기 전용은 안 된다.
bool editable(const QgsVectorLayer* layer);

// target 에서 cutter 자리를 뺀 면. 남는 면이 없으면 빈 도형. 둘은 같은 좌표계여야 한다.
QgsGeometry erased(const QgsGeometry& target, const QgsGeometry& cutter);

// 고른 도형으로 무엇을 어디서 지울지 정한다.
//   하나를 골랐으면 그 면이 자르는 면이고, 같은 레이어에서 그 아래 겹친 면이 대상이다.
//   같은 레이어에 없으면 다른 레이어에서 찾되, 겹친 면이 하나일 때만 대상으로 삼는다.
//   둘 이상 골랐으면 가장 큰 면이 대상이고 나머지가 자르는 면이다.
Plan plan(const QList<Shape>& selected, QgsProject* project);

// 계획대로 지운다. 레이어마다 편집 명령 하나를 남기고 커밋은 하지 않는다.
Outcome apply(const Plan& plan, QgsProject* project);

}  // namespace PolygonErase
