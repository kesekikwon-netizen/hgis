#pragma once

#include <QString>

#include <functional>

class QMainWindow;
class QgsMapCanvas;
class QgsMapLayer;
class QgsMessageBar;

// 의뢰처 CAD 도면(DXF·DWG)을 좌표계를 알아내 작업 좌표계 GPKG 변환본으로 바꿔 참조 지도 제자리에 올린다.
// 파일함·끌어놓기·맞추기·「벡터·도면 불러오기」가 모두 MainWindow::addVectorFromPath 를 거쳐 여기로 온다.
// 원본 DXF·DWG 는 읽기만 한다. 좌표가 없는 도면은 그 도면의 선 레이어로 정합을 바로 시작한다.
namespace KaCadImport {

inline constexpr const char* kNoCrs = "none";  // forcedAuthId: 좌표 없는 도면으로 보기

struct Hooks {
  QMainWindow* window = nullptr;  // 진행 창·오류 창의 부모와 상태줄
  QgsMapCanvas* canvas = nullptr;
  QgsMessageBar* messageBar = nullptr;
  QString surveyPath;  // 열린 조사 GPKG, 없으면 ""
  QString workCrs;     // 프로젝트 좌표계가 무효일 때의 작업 좌표계
  std::function<void(QgsMapLayer*)> startAlign;                  // 그 레이어로 정합을 시작한다
  std::function<bool(const QString&, const QString&)> reimport;  // (원본, 좌표계)로 다시 불러온다
};

// forcedAuthId: "" = 도면 숫자로 판단, "EPSG:x" = 그 좌표계, kNoCrs = 좌표 없음. 지도에 올렸으면 true.
bool run(const Hooks& hooks, const QString& path, const QString& forcedAuthId = QString());

}  // namespace KaCadImport
