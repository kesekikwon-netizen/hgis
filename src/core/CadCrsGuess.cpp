#include "CadCrsGuess.h"

#include "KoreaRegionCatalog.h"
#include "LayerRole.h"

#include <QHash>

#include <cmath>

#include <qgscoordinatetransform.h>
#include <qgsexception.h>
#include <qgsmaplayer.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

namespace CadCrsGuess {
namespace {

constexpr double kClusterM = 2000;        // 같은 무리: 첫 구성원에서 이만큼 안
constexpr double kNearSiteM = 10000;      // 조사 위치에서 이만큼 안이면 「가까운 무리」
constexpr double kMaxSideProjected = 100000;
constexpr double kMaxSideDegrees = 1.0;
constexpr double kNearOriginM = 50000;    // 가로·세로 모두 이 안이면 로컬 도면(투영 후보 아님)
constexpr double kMaxSiteViewM = 50000;   // 화면 폭이 이 이하일 때만 화면 중심을 조사 위치로 쓴다

QString regionOf(const QgsPointXY& lonLat) {
  for (const QString& sido : KoreaRegionCatalog::sidoNames()) {
    const std::optional<KoreaRegionBounds> box = KoreaRegionCatalog::overviewBounds(sido);
    if (box && lonLat.x() >= box->west && lonLat.x() <= box->east && lonLat.y() >= box->south &&
        lonLat.y() <= box->north)
      return sido;
  }
  return {};
}

double distance(const QgsPointXY& a, const QgsPointXY& b) {
  return std::hypot(a.x() - b.x(), a.y() - b.y());
}

}  // namespace

QStringList candidateAuthIds() {
  return {QStringLiteral("EPSG:5186"),  QStringLiteral("EPSG:5187"),  QStringLiteral("EPSG:5185"),
          QStringLiteral("EPSG:5188"),  QStringLiteral("EPSG:5174"),  QStringLiteral("EPSG:5176"),
          QStringLiteral("EPSG:5173"),  QStringLiteral("EPSG:5177"),  QStringLiteral("EPSG:5175"),
          QStringLiteral("EPSG:5181"),  QStringLiteral("EPSG:5183"),  QStringLiteral("EPSG:5180"),
          QStringLiteral("EPSG:5184"),  QStringLiteral("EPSG:5182"),  QStringLiteral("EPSG:2097"),
          QStringLiteral("EPSG:2096"),  QStringLiteral("EPSG:2098"),  QStringLiteral("EPSG:5179"),
          QStringLiteral("EPSG:32652"), QStringLiteral("EPSG:32651"), QStringLiteral("EPSG:4326")};
}

QString label(const QString& authId) {
  static const QHash<QString, QString> names = {
      {QStringLiteral("EPSG:5186"), QStringLiteral("GRS80 중부원점(2010)")},
      {QStringLiteral("EPSG:5187"), QStringLiteral("GRS80 동부원점(2010)")},
      {QStringLiteral("EPSG:5185"), QStringLiteral("GRS80 서부원점(2010)")},
      {QStringLiteral("EPSG:5188"), QStringLiteral("GRS80 동해원점(2010)")},
      {QStringLiteral("EPSG:5174"), QStringLiteral("베셀 중부원점 보정")},
      {QStringLiteral("EPSG:5176"), QStringLiteral("베셀 동부원점 보정")},
      {QStringLiteral("EPSG:5173"), QStringLiteral("베셀 서부원점 보정")},
      {QStringLiteral("EPSG:5177"), QStringLiteral("베셀 동해원점 보정")},
      {QStringLiteral("EPSG:5175"), QStringLiteral("베셀 제주원점 보정")},
      {QStringLiteral("EPSG:5181"), QStringLiteral("GRS80 중부원점(2002)")},
      {QStringLiteral("EPSG:5183"), QStringLiteral("GRS80 동부원점(2002)")},
      {QStringLiteral("EPSG:5180"), QStringLiteral("GRS80 서부원점(2002)")},
      {QStringLiteral("EPSG:5184"), QStringLiteral("GRS80 동해원점(2002)")},
      {QStringLiteral("EPSG:5182"), QStringLiteral("GRS80 제주원점(2002)")},
      {QStringLiteral("EPSG:2097"), QStringLiteral("베셀 중부원점")},
      {QStringLiteral("EPSG:2096"), QStringLiteral("베셀 동부원점")},
      {QStringLiteral("EPSG:2098"), QStringLiteral("베셀 서부원점")},
      {QStringLiteral("EPSG:5179"), QStringLiteral("UTM-K")},
      {QStringLiteral("EPSG:32652"), QStringLiteral("UTM 52N")},
      {QStringLiteral("EPSG:32651"), QStringLiteral("UTM 51N")},
      {QStringLiteral("EPSG:4326"), QStringLiteral("경위도(WGS84)")},
  };
  return names.value(authId, authId);
}

CadCrsResult guess(const QgsRectangle& robustExtent, const QgsCoordinateReferenceSystem& workCrs,
                   const std::optional<QgsPointXY>& siteWork, const QgsCoordinateTransformContext& context) {
  CadCrsResult result;
  if (robustExtent.isNull() || !workCrs.isValid()) return result;
  const QgsPointXY centre = robustExtent.center();
  const double side = std::max(robustExtent.width(), robustExtent.height());
  const bool nearOrigin = std::abs(centre.x()) < kNearOriginM && std::abs(centre.y()) < kNearOriginM;
  const QgsCoordinateReferenceSystem wgs84(QStringLiteral("EPSG:4326"));

  // 그럴듯한 후보(우선순위 순서).
  QVector<CadCrsCandidate> plausible;
  for (const QString& authId : candidateAuthIds()) {
    const QgsCoordinateReferenceSystem crs(authId);
    if (!crs.isValid()) continue;
    if (crs.isGeographic()) {
      if (side > kMaxSideDegrees) continue;
    } else if (side > kMaxSideProjected || nearOrigin) {
      continue;
    }
    CadCrsCandidate candidate;
    candidate.authId = authId;
    candidate.label = label(authId);
    try {
      const QgsPointXY lonLat = QgsCoordinateTransform(crs, wgs84, context).transform(centre);
      candidate.region = regionOf(lonLat);
      if (candidate.region.isEmpty()) continue;
      candidate.centreWork = QgsCoordinateTransform(crs, workCrs, context).transform(centre);
    } catch (const QgsCsException&) {
      continue;
    }
    if (siteWork) candidate.distanceToSiteM = distance(candidate.centreWork, *siteWork);
    plausible << candidate;
  }
  if (plausible.isEmpty()) return result;

  // 무리: 우선순위 순서로, 첫 구성원에서 2 km 안이면 그 무리.
  QVector<QVector<CadCrsCandidate>> clusters;
  for (const CadCrsCandidate& c : plausible) {
    bool placed = false;
    for (QVector<CadCrsCandidate>& cluster : clusters) {
      if (distance(cluster.first().centreWork, c.centreWork) <= kClusterM) {
        cluster << c;
        placed = true;
        break;
      }
    }
    if (!placed) clusters << QVector<CadCrsCandidate>{c};
  }
  const auto clusterDistance = [](const QVector<CadCrsCandidate>& cluster) {
    double best = -1;
    for (const CadCrsCandidate& c : cluster)
      if (best < 0 || c.distanceToSiteM < best) best = c.distanceToSiteM;
    return best;
  };

  if (siteWork) {
    // 무리 거리순(같으면 원래 순서). 무리 안은 우선순위 그대로다.
    std::stable_sort(clusters.begin(), clusters.end(),
                     [&](const auto& a, const auto& b) { return clusterDistance(a) < clusterDistance(b); });
    int near = 0;
    for (const auto& cluster : clusters) near += clusterDistance(cluster) <= kNearSiteM ? 1 : 0;
    result.verdict = near == 1 && clusterDistance(clusters.first()) <= kNearSiteM ? CadCrsVerdict::Certain
                                                                                  : CadCrsVerdict::Choose;
    for (const auto& cluster : clusters) result.candidates += cluster;
  } else {
    result.verdict = CadCrsVerdict::Choose;
    result.candidates = plausible;
  }
  return result;
}

std::optional<QgsPointXY> siteLocation(const QgsProject* project, const QgsRectangle& canvasExtentWork) {
  if (project) {
    QgsRectangle surveyed;
    surveyed.setNull();
    for (QgsMapLayer* layer : project->mapLayers()) {
      auto* vector = qobject_cast<QgsVectorLayer*>(layer);
      if (!vector || LayerRole::resolve(vector) != LayerRole::Kind::Survey || vector->featureCount() <= 0) continue;
      QgsRectangle extent = vector->extent();
      if (extent.isNull()) continue;
      if (vector->crs() != project->crs()) {
        try {
          extent = QgsCoordinateTransform(vector->crs(), project->crs(), project->transformContext())
                       .transformBoundingBox(extent);
        } catch (const QgsCsException&) {
          continue;
        }
      }
      surveyed.combineExtentWith(extent);
    }
    if (!surveyed.isNull()) return surveyed.center();
  }
  if (!canvasExtentWork.isNull() && !canvasExtentWork.isEmpty() && canvasExtentWork.width() <= kMaxSiteViewM)
    return canvasExtentWork.center();
  return std::nullopt;
}

QString describe(const CadCrsCandidate& candidate) {
  QString text = QStringLiteral("%1 부근 · %2 (%3)").arg(candidate.region, candidate.label, candidate.authId);
  if (candidate.distanceToSiteM >= 0)
    text += QStringLiteral(" · 조사 지역에서 %1 km").arg(candidate.distanceToSiteM / 1000.0, 0, 'f', 1);
  return text;
}

}  // namespace CadCrsGuess
