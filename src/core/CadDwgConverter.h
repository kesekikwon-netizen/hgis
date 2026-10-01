#pragma once

#include <QString>

#include <functional>

// DWG를 DXF로 바꾼다. LibreDWG 0.14의 dwg2dxf.exe 를 링크하지 않고 별도 프로그램으로만 실행한다
// (GPLv3 이상. 저장소 third_party/libredwg/0.14, 앱 폴더 tools/libredwg).
namespace CadDwgConverter {

// <appDir>/tools/libredwg/dwg2dxf.exe 가 있으면 그 경로, 없으면 빈 문자열.
QString bundledToolIn(const QString& appDir);
QString bundledTool();     // bundledToolIn(QCoreApplication::applicationDirPath())
QString converterLabel();  // "LibreDWG 0.14"

struct Result {
  bool ok = false;
  QString dxfPath;  // <outDir>/source.dxf
  QString error;    // 한국어 한 줄
  QString details;  // dwg2dxf stderr 마지막 비어 있지 않은 줄, 없으면 "종료 코드 N"
};

// dwgPath 를 <outDir>/source.dwg 로 복사하고, 작업 폴더 outDir 에서
// `<tool> --as r2000 -y -o source.dxf source.dwg` 를 창 없이 돌린다. canceled 는 100 ms마다 본다.
// 취소·시간 초과·실패이면 프로세스를 끝내고 source.dwg·source.dxf 를 지운다. 성공이면 source.dwg 만 지운다.
Result convert(const QString& dwgPath, const QString& outDir, const QString& tool, int timeoutMs = 120000,
               const std::function<bool()>& canceled = {});

}  // namespace CadDwgConverter
