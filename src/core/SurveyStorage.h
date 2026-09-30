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

// 요청 경로의 상위 폴더가 없으면 fallbackDir 아래 같은 파일명으로 옮긴다.
QString writableSurveyPath(const QString& requestedPath, const QString& fallbackDir);

// 커밋된 WAL과 내장 작업공간까지 일관된 사본으로 만든 뒤 대상 파일을 원자적으로 교체한다.
// 실패하면 원본과 기존 대상 파일을 유지한다. 원본·대상이 같으면 아무것도 바꾸지 않는다.
// 대상이 열려 이름 교체를 계속 거부당하면 대상을 제자리에서 덮어쓰지 않고 옆의 -저장.gpkg 로
// 두고 writtenPath에 그 경로를 넣는다. 그것도 못 하면 새 사본(<대상>.ka-new)을 남기고 알린다.
// 새 사본은 디스크까지 기록(FlushFileBuffers)하고 교체는 MOVEFILE_WRITE_THROUGH 로 한다.
bool copySurvey(const QString& sourceGpkg, const QString& targetGpkg, QString* errorOut = nullptr,
                QString* writtenPath = nullptr);

// 검증된 다음 세대 GPKG로 원본을 원자적으로 교체한다. 실패하면 원본 바이트를 유지하고
// writtenPath 는 바꾸지 않는다. 성공하면 SurveyFileFingerprint 에 새 상태를 기억한다.
bool publishSurveyGeneration(const QString& generationGpkg, const QString& targetGpkg,
                             QString* errorOut = nullptr, QString* writtenPath = nullptr);

// 커밋하지 않은 현재 벡터 편집을 새 복구 GPKG에 보관한다. 원본 레이어/작업공간은 바꾸지
// 않으며 매번 별도 폴더를 만든다. 래스터는 외부 참조로 유지하고 조판은 포함하지 않는다.
// onlyLayerIds가 비어 있지 않으면 그 레이어만 담는다. 성공 시 복구 파일 경로, 실패 시 빈 문자열.
QString writeRecoverySnapshot(QgsProject* project, const QString& recoveryDirectory,
                              QString* errorOut = nullptr,
                              const QStringList& onlyLayerIds = {});

// 복구사본/pending.txt에 최신 사본 경로를 남긴다. 조사 파일은 쓰지 않는다.
bool noteRecoveryPending(const QString& recoveryDirectory, const QString& snapshotPath,
                         QString* errorOut = nullptr);
// pending.txt가 가리키는 파일이 있으면 그 절대 경로, 없으면 빈 문자열.
QString pendingRecoverySnapshot(const QString& recoveryDirectory);
void clearRecoveryPending(const QString& recoveryDirectory);
// 조사복구_* 폴더를 최신 keep개만 남긴다. protectPath가 들어 있는 폴더는 지우지 않는다.
int pruneRecoverySnapshots(const QString& recoveryDirectory, int keep, const QString& protectPath);

struct AbsorbResult {
  QStringList imported;   // .gpkg 안으로 들여온 레이어 이름
  QStringList failed;     // 들여오지 못한 레이어 이름(원래 경로를 그대로 둔다)
  QStringList skippedRaster;  // 래스터는 아직 바깥에 남는다(스크린샷 등)
  QStringList skippedReference;  // 참조 지도 벡터는 조사 파일에 복사하지 않는다
};

// 조사 .gpkg 바깥에 있는 파일 기반·메모리 벡터 레이어를 .gpkg 안으로 복사하고
// 레이어가 그 사본을 가리키게 바꾼다. 원본 파일은 지우지 않는다.
// 배경지도(xyz/wms)처럼 파일이 아닌 레이어는 건드리지 않는다.
AbsorbResult absorbExternalVectors(QgsProject* project, const QString& gpkgPath,
                                   const QString& alsoSurveyGpkg = {});

// 다음 세대 GPKG에 편집·흡수·내장 쓰기를 한 뒤 검증하고 원본을 교체한다.
// 한 단계라도 실패하면 saved=false 이고 원본 바이트와 미저장 편집을 유지한다. 조사 파일
// 레이어의 편집은 세대 파일에만 쓰고, 원본에는 교체가 성공할 때만 반영된다.
// 예외 하나(사용자 결정 2026-09-20): 커밋이 막힌 레이어가 있으면 저장할 수 있는 조사 파일
// 레이어는 원본에 먼저 커밋한다. 그 레이어 이름은 originalCommittedLayers 에 담기고 error 에도
// 적힌다(원본 일부 갱신). 이때도 세대 교체는 하지 않고 복구 사본을 남긴다.
struct PersistAttempt {
    bool saved = false;
    QString surveyPath;
    QString recoveryPath;
    QString error;
    QStringList committedLayers;
    QStringList failedLayers;
    QStringList skippedRaster;
    QStringList skippedReference;
    QStringList collectedLayers;  // 조사 폴더로 모아 온 바깥 자료
    QStringList originalCommittedLayers;  // 실패했지만 원본 조사 파일에 먼저 커밋한 레이어
  };
PersistAttempt persistWorkspace(QgsProject* project, const QString& gpkgPath,
                                const QString& recoveryDirectory,
                                const QString& fallbackDirectory = {});

// 조사 GPKG 안에 들어 있는 참조 벡터 레이어 이름. 도메인 키는 제외한다.
QStringList embeddedReferenceVectorNames(QgsProject* project, const QString& gpkgPath);

struct ExtractAttempt {
  bool extracted = false;
  QString error;
  QStringList moved;
  QStringList failed;
  QString outputDirectory;
  qint64 bytesBefore = 0;
  qint64 bytesAfter = 0;
};

// 조사 파일 안 참조 벡터를 바깥 GPKG로 옮기고 원본 테이블을 지운 뒤 검증·교체한다.
// 호출자가 사용자 확인을 마친 뒤에만 부른다. 도메인 레이어는 건드리지 않는다.
ExtractAttempt extractEmbeddedReferenceVectors(QgsProject* project, const QString& gpkgPath,
                                               const QString& outputDirectory);

// 프로젝트를 .gpkg 안에 기록한다.
// publishedGpkg: gpkgPath 가 저장용 세대 파일이면 나중에 바뀔 진짜 조사 파일 경로. 세대 파일을
// 가리키는 레이어를 그 경로로 적는다. 세대 폴더는 저장 뒤 지워지기 때문이다.
bool writeEmbedded(QgsProject* project, const QString& gpkgPath, QString* errorOut = nullptr,
                   const QString& publishedGpkg = QString());

// .gpkg 안의 프로젝트를 읽는다. crashedOut은 KaSafeQgis와 같은 의미다(SEH 발생).
// 사용자가 명시적으로 작업공간을 열 때만 loadLayouts=true로 저장 조판도 복원한다.
bool readEmbedded(QgsProject* project, const QString& gpkgPath, bool* crashedOut = nullptr,
                  QString* errorOut = nullptr, bool loadLayouts = false);

}  // namespace SurveyStorage
