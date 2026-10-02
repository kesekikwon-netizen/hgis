#pragma once

#include <QList>
#include <QString>

class QgsProject;
class QgsVectorLayer;

// 도면 변환본(CadDrawingStore 의 GPKG)을 참조 지도 아래 「<원본 이름> (도면)」 묶음으로 올린다.
// 묶음 안은 위에서부터 글자·점·선·면이고, CAD 원래 색과 글자 크기·각도를 그대로 보인다.
// 레이어마다 도면 id 를 kPropDrawing 에 적어 같은 도면의 레이어를 함께 찾는다.
namespace CadDrawingLayers {

constexpr const char* kPropDrawing = "ka_hgis/cad_drawing";  // 값 = 도면 id (QUuid, 괄호 없음)

QString groupTitle(const QString& sourcePath);  // "<completeBaseName> (도면)"

// 같은 제목 묶음이 있으면 그 레이어와 묶음을 먼저 지운다. 실패하면 빈 목록과 한국어 error.
QList<QgsVectorLayer*> addToProject(QgsProject* project, const QString& gpkgPath, const QString& title,
                                    const QString& drawingId, QString* error);
QList<QgsVectorLayer*> layersOf(const QgsProject* project, const QString& drawingId);
QgsVectorLayer* alignLayerOf(const QgsProject* project, const QString& drawingId);  // 선 → 면 → 점 → 글자
void removeFromProject(QgsProject* project, const QString& drawingId);
QString drawingIdOfGroup(const QgsProject* project, const QString& title);  // 그 제목 묶음의 도면 id, 없으면 ""

}  // namespace CadDrawingLayers
