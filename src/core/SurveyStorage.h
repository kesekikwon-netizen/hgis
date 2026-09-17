#pragma once

#include <QString>
#include <QStringList>

class QgsProject;

// 조사 파일 하나로 닫는 저장 구조.
//
// 예전 구조는 조사 데이터(.gpkg)와 작업공간(.qgz) 두 파일이었고, 외부 SHP는 .qgz가
// 상대경로로 가리키기만 했다. 그래서 .qgz 하나가 깨지거나 폴더를 옮기면 그 레이어들이
// 통째로 사라졌고, 그 상태에서 자동 저장이 원본을 덮어써 영구 손실이 났다.
//
// 여기서는 QGIS의 GeoPackage 프로젝트 저장소(geopackage: URI)를 써서 프로젝트를 .gpkg
// 안에 넣고, 외부 벡터 레이어도 .gpkg 안으로 들여온다. 벡터와 작업공간은 조사 파일에
// 보관하며, 사진·래스터는 외부 원본 파일을 함께 보관해야 한다.
namespace SurveyStorage {

// geopackage:<경로>?projectName=<이름>
QString projectUri(const QString& gpkgPath);

// 현재 작업을 바꾸기 전에 GPKG 형식과 SQLite 무결성을 읽기 전용으로 확인한다.
bool validateForOpen(const QString& gpkgPath, QString* errorOut = nullptr);

// .gpkg 안에 프로젝트가 들어 있는지(qgis_projects 테이블). 파일을 읽기만 한다.
bool hasEmbeddedProject(const QString& gpkgPath);

// 커밋된 WAL과 내장 작업공간까지 일관된 사본으로 만든 뒤 대상 파일을 원자적으로 교체한다.
// 실패하면 원본과 기존 대상 파일을 유지한다. 원본·대상이 같으면 아무것도 바꾸지 않는다.
bool copySurvey(const QString& sourceGpkg, const QString& targetGpkg, QString* errorOut = nullptr);

// 커밋하지 않은 현재 벡터 편집을 새 복구 GPKG에 보관한다. 원본 레이어/작업공간은 바꾸지
// 않으며 매번 별도 폴더를 만든다. 래스터는 외부 참조로 유지하고 조판은 포함하지 않는다.
// 성공 시 복구 파일 경로, 실패 시 빈 문자열을 반환한다.
QString writeRecoverySnapshot(QgsProject* project, const QString& recoveryDirectory,
                              QString* errorOut = nullptr);

struct AbsorbResult {
  QStringList imported;   // .gpkg 안으로 들여온 레이어 이름
  QStringList failed;     // 들여오지 못한 레이어 이름(원래 경로를 그대로 둔다)
  QStringList skippedRaster;  // 래스터는 아직 바깥에 남는다(스크린샷 등)
};

// 조사 .gpkg 바깥에 있는 파일 기반·메모리 벡터 레이어를 .gpkg 안으로 복사하고
// 레이어가 그 사본을 가리키게 바꾼다. 원본 파일은 지우지 않는다.
// 배경지도(xyz/wms)처럼 파일이 아닌 레이어는 건드리지 않는다.
AbsorbResult absorbExternalVectors(QgsProject* project, const QString& gpkgPath);

// 프로젝트를 .gpkg 안에 기록한다.
bool writeEmbedded(QgsProject* project, const QString& gpkgPath, QString* errorOut = nullptr);

// .gpkg 안의 프로젝트를 읽는다. crashedOut은 KaSafeQgis와 같은 의미다(SEH 발생).
// 사용자가 명시적으로 작업공간을 열 때만 loadLayouts=true로 저장 조판도 복원한다.
bool readEmbedded(QgsProject* project, const QString& gpkgPath, bool* crashedOut = nullptr,
                  QString* errorOut = nullptr, bool loadLayouts = false);

}  // namespace SurveyStorage
