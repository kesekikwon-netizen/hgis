#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransformcontext.h>
#include <qgspointxy.h>
#include <qgsrectangle.h>

class QgsProject;

// 좌표계 정보가 없는 CAD 도면 숫자를 보고 그럴듯한 한국 좌표계 후보를 찾는다.
// 후보마다 도면 범위 중심을 경위도로 바꿔 시·도 개략 범위에 드는지 보고, 조사 지역 가까이 놓이는 무리가
// 하나뿐이면 바로 고른다(Certain). 숫자가 원점 가까이(가로·세로 모두 50 km 안)인 로컬 도면은 투영 후보에서 뺀다:
// 동부원점(FN 500000)으로 읽으면 우연히 제주도에 떨어지기 때문이다.
struct CadCrsCandidate {
  QString authId;               // "EPSG:5174"
  QString label;                // label(authId)
  QString region;               // "경상북도"
  QgsPointXY centreWork;        // robustExtent 중심을 작업 좌표계로
  double distanceToSiteM = -1;  // 조사 위치가 없으면 -1
};

enum class CadCrsVerdict { Certain, Choose, NoCrs };

struct CadCrsResult {
  CadCrsVerdict verdict = CadCrsVerdict::NoCrs;
  QVector<CadCrsCandidate> candidates;  // [0] 이 제안. NoCrs 이면 비어 있다
};

namespace CadCrsGuess {

QStringList candidateAuthIds();  // 우선순위 순서
QString label(const QString& authId);
CadCrsResult guess(const QgsRectangle& robustExtent, const QgsCoordinateReferenceSystem& workCrs,
                   const std::optional<QgsPointXY>& siteWork, const QgsCoordinateTransformContext& context);
// 조사 레이어 범위의 중심, 없으면 폭 50 km 이하인 지도 화면의 중심, 둘 다 아니면 없음.
std::optional<QgsPointXY> siteLocation(const QgsProject* project, const QgsRectangle& canvasExtentWork);
// "경상북도 부근 · 베셀 중부원점 보정 (EPSG:5174) · 조사 지역에서 1.2 km"
QString describe(const CadCrsCandidate& candidate);

}  // namespace CadCrsGuess
