#pragma once

#include <QString>

class QgsProject;

// 「다른 이름으로 저장」: 열린 조사(original)의 마지막 저장본을 target 으로 복사하고, original 을 가리키던 레이어를
// target 으로 옮긴다. 아직 저장하지 않은 편집은 레이어 편집 버퍼에 그대로 남아 다음 저장 때 target 에만 쓰인다
// (QGIS 는 데이터 경로를 바꿔도 편집 버퍼를 지킨다, 2026-10-02 잼). original 파일은 바꾸지 않는다.
namespace SurveySaveAs {

// 실패하면 한국어 error. 이미 옮긴 레이어는 그대로 둔다(호출한 쪽이 되돌린다).
bool moveToCopy(QgsProject* project, const QString& original, const QString& target, QString* error);

}  // namespace SurveySaveAs
