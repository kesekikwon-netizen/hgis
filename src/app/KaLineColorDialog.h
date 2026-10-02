#pragma once

#include <functional>

class QWidget;
class QgsLayerTreeView;
class QgsVectorLayer;

// 참조 지도·지적도 가운데 선으로 된 지도(도면 선·면, 받은 지적도, 등고선)의 「선 색」 창(2026-10-03 사용자 목표:
// 「선으로 된지도는 사용자가 색을 바꿀수있게하라」). 색·굵기를 고르고 「적용」, 처음 모양은 「원래 색으로」.
// 면을 색으로 채운 지도(토양도·지질도)는 공식 범례라 이유를 알리고 바꾸지 않는다.
namespace KaLineColorDialog {

// 고른 레이어. 도면 묶음(「이름 (도면)」)을 골랐으면 그 도면의 선 레이어.
QgsVectorLayer* layerFor(QgsLayerTreeView* view);
// changed 는 색을 바꾸거나 되돌린 뒤 한 번 부른다(지도·도면 화면 다시 그리기).
void edit(QWidget* parent, QgsVectorLayer* layer, const std::function<void()>& changed);

}  // namespace KaLineColorDialog
