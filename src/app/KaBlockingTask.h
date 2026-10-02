#pragma once

#include <QString>

#include <functional>

class QWidget;
class QgsFeedback;

// 무거운 일(DWG 변환·DXF 읽기·변환본 쓰기)을 QgsTask 로 돌리는 동안 창 모달 진행 창(「취소」)을 보이고
// 끝날 때까지 기다린다. work 안에서는 QgsProject 와 지도를 만지지 않는다.
namespace KaBlockingTask {

// work 가 true 로 끝나면 true. 「취소」를 누르면 feedback 이 취소되고, work 가 멈춘 뒤 false.
bool run(QWidget* parent, const QString& label, const std::function<bool(QgsFeedback*)>& work);

}  // namespace KaBlockingTask
