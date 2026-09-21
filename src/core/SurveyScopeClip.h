#pragma once

#include <QString>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransformcontext.h>
#include <qgsgeometry.h>

class QgsProject;
class QgsVectorLayer;

// 조사구역 주변 5km. 지적도는 이미 이 범위로 자른다.
// 주변유적·수치지형도는 서버가 반경을 주지 않으므로 받은 뒤 여기서 자른다.
// 원본 ZIP/도엽은 건드리지 않는다.
namespace SurveyScopeClip {
constexpr double kRadiusMeters = 5000.;

QgsGeometry surveyUnion(QgsProject* project, const QgsCoordinateReferenceSystem& workCrs,
                        const QgsCoordinateTransformContext& context, QString* error = nullptr);
QgsGeometry bufferMeters(const QgsGeometry& survey, double meters = kRadiusMeters);

// 조사구역이 있으면 5km 밖 도형을 작업 레이어에서 지운다. 없으면 그대로 둔다.
// 원본 SHP/ZIP가 아닌 작업 사본에만 쓴다.
bool keepIntersectingIfSurvey(QgsProject* project, QgsVectorLayer* layer, QString* error = nullptr);

// 읽기 전용 원본은 복사본을 만든다. 조사구역이 없으면 source를 그대로 돌려준다.
// 조사구역이 있으면 호출자가 소유하는 새 레이어. source는 지우지 않는다.
QgsVectorLayer* intersectingCopyIfSurvey(QgsProject* project, QgsVectorLayer* source,
                                         const QString& name, QString* error = nullptr);
}