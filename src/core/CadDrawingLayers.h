#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include "GeorefService.h"

class QgsMapLayer;
class QgsProject;
class QgsVectorLayer;

// 도면 변환본(CadDrawingStore 의 GPKG)을 참조 지도 아래 「<원본 이름> (도면)」 묶음으로 올린다.
// 묶음 안은 위에서부터 글자·점·선·면이고, CAD 원래 색과 글자 크기·각도를 그대로 보인다.
// 레이어마다 도면 id 를 kPropDrawing 에 적어 같은 도면의 레이어를 함께 찾는다.
namespace CadDrawingLayers {

constexpr const char* kPropDrawing = "ka_hgis/cad_drawing";  // 값 = 도면 id (QUuid, 괄호 없음)
constexpr const char* kPropUnsure = "ka_hgis/cad_unsure";    // true = 단서 없이 가장 그럴듯한 자리에 둔 도면

QString groupTitle(const QString& sourcePath);  // "<completeBaseName> (도면)"
// 같은 원본 파일에서 올린 묶음이 있으면 그 제목(다시 올리면 바꾼다), 없으면 비어 있는 「이름 (도면)」·「이름 (도면 2)」….
QString titleFor(const QgsProject* project, const QString& sourcePath);

// 새 레이어를 모두 연 뒤 같은 제목 묶음이 있으면 그 레이어와 묶음을 지우고 바꾼다.
// 실패하면 예전 묶음은 그대로 두고 빈 목록과 한국어 error.
QList<QgsVectorLayer*> addToProject(QgsProject* project, const QString& gpkgPath, const QString& title,
                                    const QString& drawingId, QString* error);
QList<QgsVectorLayer*> layersOf(const QgsProject* project, const QString& drawingId);
QgsVectorLayer* alignLayerOf(const QgsProject* project, const QString& drawingId);  // 선 → 면 → 점 → 글자
void removeFromProject(QgsProject* project, const QString& drawingId);
// 그 제목 도면 묶음(참조 지도 아래 어느 깊이든, 바로 아래 레이어에 도면 id 가 있는 묶음)의 도면 id, 없으면 ""
QString drawingIdOfGroup(const QgsProject* project, const QString& title);
QString drawingIdOf(const QgsMapLayer* layer);  // 도면 레이어면 그 id, 아니면 ""

// 정합: 맞추는 동안 같은 도면의 다른 레이어를 숨기고(숨긴 id), 끝나면 다시 보인다. 맞춘 변환은 그 도면의
// 변환본 모든 표에 같이 쓴다(CadDrawingStore::applyAffine). 성공하면 레이어마다 다시 그린다.
QStringList hideCompanions(QgsProject* project, const QString& drawingId, const QgsMapLayer* keep);
void showLayers(QgsProject* project, const QStringList& layerIds);
bool saveAlignment(QgsProject* project, const QString& drawingId, const GeorefService::Affine& a, QString* error);

}  // namespace CadDrawingLayers
