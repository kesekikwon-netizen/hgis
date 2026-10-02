#pragma once

#include <QString>

class QgsMapLayer;

// 앱 안에 남아 조사 파일을 연 채 두는 손잡이를 다룬다. 레이어 목록에서 지운 조사 레이어를 Ctrl+Z 를 위해 붙들어 두면
// 그 레이어가 원본 조사 파일을 계속 열어, 저장이 원본을 바꾸지 못하고 「이름-저장.gpkg」로 빠졌다(2026-10-02 R67).
namespace SurveyHandles {

// 붙들어 둘 조사 레이어의 데이터 경로를 비워 조사 파일 손잡이를 놓는다. 표 이름·편집 여부·모양은 레이어에 적어 두고
// 그대로 돌려준다. 조사 파일(surveyGpkg) 레이어가 아니거나 저장하지 않은 편집이 있으면(편집을 잃지 않게) 그대로 둔다.
QgsMapLayer* release(QgsMapLayer* layer, const QString& surveyGpkg);

// release 한 레이어를 지금 조사 파일(surveyGpkg)의 그 표로 다시 연다. 저장 뒤면 저장한 파일이다. 그 밖의 레이어는 그대로.
QgsMapLayer* reattach(QgsMapLayer* layer, const QString& surveyGpkg);

}  // namespace SurveyHandles
