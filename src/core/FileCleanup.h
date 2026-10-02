#pragma once

#include <QString>

// 앱이 만든 파일(옛 도면 변환본 등)을 지운다. 앱 안의 읽는 쪽(지도가 그리는 동안 연 GPKG 손잡이, QGIS 연결 풀)이
// 아직 열고 있으면 지금은 지우지 못하므로 5초 뒤와 60초 뒤에 다시 지운다(같은 도면을 다시 불러오면 옛 변환본이
// 남던 일, 2026-10-02 R89). 앱이 만든 파일에만 쓴다: 사용자의 원본 자료에는 쓰지 않는다.
namespace FileCleanup {

// 지금 지웠으면 true. 못 지웠으면 다시 지우기를 예약하고 false.
bool removeWhenFree(const QString& path);

}  // namespace FileCleanup
