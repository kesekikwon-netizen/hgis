#pragma once

#include <QByteArray>
#include <QStringList>

class QgsProject;

// 2분 복구 사본을 "편집이 바뀐 때만" 만들기 위한 지문.
//
// 복구 사본은 늘 온전한 조사 단위(모든 대상 레이어 + 트리)로 만든다. 여기서는 그 사본을 다시
// 만들 필요가 있는지만 판단한다. 창·상태줄 알림은 없다(조용한 동작 유지).
namespace SurveyRecovery {

// layerIds 레이어들의 저장 안 된 편집 상태를 요약한 지문. 편집 버퍼(더한·고친·지운 도형,
// 속성, 필드), 편집 여부, 도형 수, 레이어 구성이 같으면 같은 값을 돌려준다. 대상은 편집
// 버퍼 크기에 비례해 읽고 레이어 전체를 읽지 않는다. 트리 순서·스타일만 바꾼 것은 반영하지 않는다.
QByteArray editSignature(QgsProject* project, const QStringList& layerIds);

// 지난번 사본의 지문과 같으면 새 사본이 필요 없다. lastSignature 가 비어 있으면 늘 필요하다.
bool snapshotNeeded(const QByteArray& lastSignature, const QByteArray& currentSignature);

}  // namespace SurveyRecovery
