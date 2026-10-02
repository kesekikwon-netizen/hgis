#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransformcontext.h>

#include "CadDrawingReader.h"
#include "GeorefService.h"

class QgsVectorLayer;

// 도면 한 장을 작업 좌표계 GPKG 변환본으로 쓰고, 정합 결과를 변환본의 모든 표에 같은 변환으로 적용한다.
// 원본 DXF·DWG 는 건드리지 않는다. 표는 도형이 있는 lines·fills·points·texts 와 정보 표 ka_cad_drawing 이다.
struct CadStoreInfo {
  QString sourcePath;
  QString sourceSha256;
  QString sourceCrs;  // "EPSG:5174" 또는 "" (좌표 없음)
  QString converter;  // "LibreDWG 0.14" 또는 ""
};

namespace CadDrawingStore {

// <조사 폴더>/가져온자료/도면/<원본 이름>.gpkg. 조사가 없으면 AppLocalData/cad-drawings 아래다.
// 그 이름이 이미 있으면 " (2)", " (3)" 을 붙인다.
QString outputPathFor(const QString& sourcePath, const QString& surveyDir);

// 같은 폴더의 임시 폴더에 다 쓴 뒤 outPath 로 옮긴다. outPath 가 이미 있으면 쓰지 않는다.
// 실패하거나 취소하면 false 와 한국어 error 이고, 아무 파일도 남기지 않는다.
bool write(const CadDrawing& drawing, const CadStoreInfo& info, const QgsCoordinateReferenceSystem& workCrs,
           const QgsCoordinateTransformContext& context, const QString& outPath, QString* error,
           const std::function<bool()>& canceled = {});

CadStoreInfo readInfo(const QString& gpkgPath);
QStringList tableNames(const QString& gpkgPath);  // "lines","fills","points","texts" 가운데 있는 것, 이 순서

// 레이어마다 도형을 옮기고 글자의 각도에 회전각을 더하고 높이에 배율을 곱한다(데이터 제공자로 바로 쓴다).
// 한 레이어라도 실패하면 이미 바꾼 레이어를 원래 도형·속성으로 되돌리고 false.
bool applyAffine(const QList<QgsVectorLayer*>& layers, const GeorefService::Affine& a, QString* error);

}  // namespace CadDrawingStore
