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

// 좌표계 정보가 없는 CAD 도면 숫자를 보고 그럴듯한 한국 좌표계 후보를 찾고, 단서로 하나를 정한다.
// 후보마다 도면 범위 중심을 경위도로 바꿔 시·도 개략 범위에 드는지 본다. 숫자가 원점 가까이(가로·세로 모두
// 50 km 안)인 로컬 도면은 투영 후보에서 뺀다: 동부원점(FN 500000)으로 읽으면 우연히 제주도에 떨어지기 때문이다.
// 사용자에게 묻지 않는다(2026-10-03 사용자 목표). 단서가 자리를 하나로 정하면 Certain, 끝까지 여럿이면
// 최근 좌표계·기본 순서로 가장 그럴듯한 자리를 앞에 둔 Likely 다.
struct CadCrsCandidate {
  QString authId;               // "EPSG:5174"
  QString label;                // label(authId)
  QString region;               // "경상북도"
  QStringList regions;          // 중심이 드는 시·도 개략 범위 모두(범위는 겹친다). 지명 단서가 쓴다
  QgsPointXY centreWork;        // robustExtent 중심을 작업 좌표계로
  double distanceToSiteM = -1;  // 조사 위치가 없으면 -1
};

enum class CadCrsVerdict { Certain, Likely, NoCrs };

struct CadCrsResult {
  CadCrsVerdict verdict = CadCrsVerdict::NoCrs;
  QVector<CadCrsCandidate> candidates;  // [0] 을 쓴다. 나머지는 「다른 위치로 바꾸기」. NoCrs 이면 비어 있다
  QString reason;                       // 정한 단서: 「도면 표시」·「조사 위치」·「지도 화면」·「지명」·「최근 좌표계」·「기본 순서」
};

// 판단 단서. 앞에서부터 쓰고, 한 단서가 자리를 여럿으로 줄이면 다음 단서가 그 안에서 고른다.
struct CadCrsClues {
  QStringList stated;               // 도면이 밝힌 좌표계(CadCrsHints::stated)
  std::optional<QgsPointXY> site;   // 조사 위치(작업 좌표계): 이만큼 10 km 안
  QgsRectangle view;                // 지금 지도 화면(작업 좌표계). 없으면 null
  QStringList provinces;            // 이름에 나온 시·도(CadCrsHints::provinces)
  QStringList recent;               // 최근에 맞았던 좌표계(최근 것이 앞)
};

namespace CadCrsGuess {

QStringList candidateAuthIds();  // 우선순위 순서
QString label(const QString& authId);
CadCrsResult guess(const QgsRectangle& robustExtent, const QgsCoordinateReferenceSystem& workCrs,
                   const CadCrsClues& clues, const QgsCoordinateTransformContext& context);
// 조사 레이어 범위의 중심, 없으면 지도에 이미 있는 좌표 도면(한국 안)의 중심, 둘 다 아니면 없음.
std::optional<QgsPointXY> siteLocation(const QgsProject* project);
// 지도 화면(작업 좌표계). 앱이 가장 그럴듯한 자리에 둔 도면이 화면에 있으면 사용자가 고른 위치가 아니므로 null.
QgsRectangle trustedView(const QgsProject* project, const QgsRectangle& viewWork);
// "경상북도 부근 · 베셀 중부원점 보정 (EPSG:5174) · 조사 지역에서 1.2 km"
QString describe(const CadCrsCandidate& candidate);

}  // namespace CadCrsGuess
