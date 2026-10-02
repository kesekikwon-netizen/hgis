#include "CadCrsGuess.h"

#include "CadDrawingLayers.h"
#include "KoreaRegionCatalog.h"
#include "LayerRole.h"

#include <QHash>

#include <algorithm>
#include <cmath>
#include <functional>

#include <qgscoordinatetransform.h>
#include <qgsexception.h>
#include <qgsmaplayer.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

namespace CadCrsGuess {
namespace {

constexpr double kClusterM = 2000;        // 같은 무리: 첫 구성원에서 이만큼 안
constexpr double kNearSiteM = 10000;      // 조사 위치에서 이만큼 안이면 「가까운 무리」
constexpr double kFarFromSiteM = 50000;   // 조사 위치를 아는데 이만큼 안에 자리가 없으면 로컬 도면
constexpr double kMaxSideProjected = 100000;
constexpr double kMaxSideDegrees = 1.0;
constexpr double kNearOriginM = 50000;    // 가로·세로 모두 이 안이면 로컬 도면(투영 후보 아님)

QStringList regionsOf(const QgsPointXY& lonLat) {
  QStringList out;
  for (const QString& sido : KoreaRegionCatalog::sidoNames()) {
    const std::optional<KoreaRegionBounds> box = KoreaRegionCatalog::overviewBounds(sido);
    if (box && lonLat.x() >= box->west && lonLat.x() <= box->east && lonLat.y() >= box->south &&
        lonLat.y() <= box->north)
      out << sido;
  }
  return out;
}

QString regionOf(const QgsPointXY& lonLat) { return regionsOf(lonLat).value(0); }

bool isUnsure(const QgsMapLayer* layer) {
  return layer->customProperty(QString::fromLatin1(CadDrawingLayers::kPropUnsure)).toBool();
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
                   const CadCrsClues& clues, const QgsCoordinateTransformContext& context) {
  const std::optional<QgsPointXY>& siteWork = clues.site;
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
      candidate.regions = regionsOf(lonLat);
      candidate.region = candidate.regions.value(0);
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
  if (siteWork) {  // 무리 거리순(같으면 원래 순서). 「다른 위치로 바꾸기」 목록도 이 순서다.
    std::stable_sort(clusters.begin(), clusters.end(),
                     [&](const auto& a, const auto& b) { return clusterDistance(a) < clusterDistance(b); });
    // 조사 위치를 아는데 어느 자리도 가깝지 않으면 실제 좌표가 아니라 업체 로컬 좌표다: 정합으로 간다.
    if (clusterDistance(clusters.first()) > kFarFromSiteM) return result;
  }

  // 단서마다 남은 무리를 줄인다. 하나만 남으면 그 단서로 정해진 것이고, 하나도 안 남는 단서는 건너뛴다.
  QVector<int> pool;
  for (int i = 0; i < clusters.size(); ++i) pool << i;
  const auto narrow = [&](const QString& why, const std::function<bool(const QVector<CadCrsCandidate>&)>& keep) {
    QVector<int> kept;
    for (int i : pool)
      if (keep(clusters[i])) kept << i;
    if (!kept.isEmpty()) pool = kept;
    if (pool.size() == 1 && result.reason.isEmpty() && !kept.isEmpty()) result.reason = why;
  };
  const auto has = [](const QVector<CadCrsCandidate>& cluster, const QStringList& ids) {
    return std::any_of(cluster.cbegin(), cluster.cend(), [&](const CadCrsCandidate& c) { return ids.contains(c.authId); });
  };
  QgsRectangle view = clues.view;
  if (!view.isNull()) view.grow(kNearSiteM);
  narrow(QStringLiteral("도면 표시"), [&](const auto& cluster) { return has(cluster, clues.stated); });
  if (result.reason.isEmpty() && siteWork)
    narrow(QStringLiteral("조사 위치"), [&](const auto& cluster) { return clusterDistance(cluster) <= kNearSiteM; });
  if (result.reason.isEmpty() && !view.isNull())
    narrow(QStringLiteral("지도 화면"), [&](const auto& cluster) {
      return std::any_of(cluster.cbegin(), cluster.cend(), [&](const auto& c) { return view.contains(c.centreWork); });
    });
  if (result.reason.isEmpty())
    narrow(QStringLiteral("지명"), [&](const auto& cluster) {
      return std::any_of(cluster.first().regions.cbegin(), cluster.first().regions.cend(),
                         [&](const QString& sido) { return clues.provinces.contains(sido); });
    });

  // 무리 안과 남은 무리는 도면 표시 → 최근 좌표계 → 기본 순서로 놓는다.
  const auto rank = [&clues](const CadCrsCandidate& c) {
    const int recent = static_cast<int>(clues.recent.indexOf(c.authId));
    return (clues.stated.contains(c.authId) ? 0 : 1000) + (recent < 0 ? 100 : recent);
  };
  const auto best = [&](const QVector<CadCrsCandidate>& cluster) {
    int r = 1000 * 2;
    for (const CadCrsCandidate& c : cluster) r = std::min(r, rank(c));
    return r;
  };
  for (QVector<CadCrsCandidate>& cluster : clusters)
    std::stable_sort(cluster.begin(), cluster.end(), [&](const auto& a, const auto& b) { return rank(a) < rank(b); });
  // 정하지 못했을 때: 조사 위치를 알면 가까운 순, 모르면 최근에 맞았던 좌표계 순.
  if (!siteWork)
    std::stable_sort(pool.begin(), pool.end(), [&](int a, int b) { return best(clusters[a]) < best(clusters[b]); });
  result.verdict = result.reason.isEmpty() ? CadCrsVerdict::Likely : CadCrsVerdict::Certain;
  if (result.reason.isEmpty())
    result.reason = siteWork ? QStringLiteral("가까운 순")
                    : rank(clusters[pool.first()].first()) % 1000 < 100 ? QStringLiteral("최근 좌표계")
                                                                         : QStringLiteral("기본 순서");
  for (int i : pool) result.candidates += clusters[i];
  for (int i = 0; i < clusters.size(); ++i)
    if (!pool.contains(i)) result.candidates += clusters[i];
  return result;
}

std::optional<QgsPointXY> siteLocation(const QgsProject* project) {
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
    // 도면을 먼저 올리고 조사구역을 그리는 순서라, 조사구역이 없으면 이미 올린 좌표 도면·받은 지적도를 본다.
    const QgsCoordinateReferenceSystem wgs84(QStringLiteral("EPSG:4326"));
    QgsRectangle placed;
    placed.setNull();
    for (QgsMapLayer* layer : project->mapLayers()) {
      auto* vector = qobject_cast<QgsVectorLayer*>(layer);
      const bool drawing = vector && !vector->customProperty(QString::fromLatin1(CadDrawingLayers::kPropDrawing)).toString().isEmpty();
      if (vector && isUnsure(vector)) continue;  // 가장 그럴듯한 자리에 둔 도면은 위치가 아니다
      if (!vector || !(drawing || LayerRole::resolve(vector) == LayerRole::Kind::Cadastral) || vector->featureCount() <= 0)
        continue;
      const QgsPointXY centre = vector->extent().center();
      if (std::abs(centre.x()) < kNearOriginM && std::abs(centre.y()) < kNearOriginM) continue;  // 로컬 숫자 도면
      try {
        if (regionOf(QgsCoordinateTransform(vector->crs(), wgs84, project->transformContext()).transform(centre)).isEmpty())
          continue;
        placed.combineExtentWith(
            QgsCoordinateTransform(vector->crs(), project->crs(), project->transformContext()).transformBoundingBox(vector->extent()));
      } catch (const QgsCsException&) {
      }
    }
    if (!placed.isNull()) return placed.center();
  }
  return std::nullopt;
}

QgsRectangle trustedView(const QgsProject* project, const QgsRectangle& viewWork) {
  if (!project || viewWork.isNull()) return viewWork;
  for (QgsMapLayer* layer : project->mapLayers()) {
    if (!isUnsure(layer)) continue;
    try {
      const QgsRectangle extent = QgsCoordinateTransform(layer->crs(), project->crs(), project->transformContext())
                                      .transformBoundingBox(layer->extent());
      if (extent.intersects(viewWork)) return QgsRectangle();
    } catch (const QgsCsException&) {
    }
  }
  return viewWork;
}

QString describe(const CadCrsCandidate& candidate) {
  QString text = QStringLiteral("%1 부근 · %2 (%3)").arg(candidate.region, candidate.label, candidate.authId);
  if (candidate.distanceToSiteM >= 0)
    text += QStringLiteral(" · 조사 지역에서 %1 km").arg(candidate.distanceToSiteM / 1000.0, 0, 'f', 1);
  return text;
}

}  // namespace CadCrsGuess
