#pragma once

#include <QColor>

class QgsVectorLayer;

// 선으로 된 지도(도면 선·면, 받은 지적도처럼 외곽선만 그린 면, 등고선 같은 선 레이어)의 선 색·굵기를 사용자가
// 바꾼다(2026-10-03 사용자 목표). 처음 바꿀 때 원래 모양(도면은 CAD 원래 색)을 레이어에 적어 두어
// 조사 작업공간에 같이 저장되고, 다시 열어도 「원래 색으로」 되돌릴 수 있다.
// 토양도·지질도처럼 면을 색으로 채운 지도와 점·글자 레이어는 바꾸지 않는다.
namespace LineMapStyle {

bool isLineMap(const QgsVectorLayer* layer);
// 레이어의 선을 모두 color·widthMm 로 그린다. 외곽선만 그린 면은 채우지 않고, 도면의 면은 같은 색을 옅게 채운다.
bool apply(QgsVectorLayer* layer, const QColor& color, double widthMm);
bool hasOriginal(const QgsVectorLayer* layer);
bool restoreOriginal(QgsVectorLayer* layer);
// 색 창의 처음 값. 도형마다 색이 다르면(도면) 진한 회색.
QColor currentColor(const QgsVectorLayer* layer);
double currentWidthMm(const QgsVectorLayer* layer);

}  // namespace LineMapStyle
