#include "TopographicSourceCrs.h"
#include "TopographicSheets.h"
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransform.h>
#include <qgsexception.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgsproviderregistry.h>
#include <qgsvectordataprovider.h>
#include <QFileInfo>
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>

namespace {
using TopographicCatalog::Category;
constexpr double BoundaryMarginMetres = 100.;
bool primary(Category category) {
  switch (category) {
  case Category::Contour: case Category::ElevationPoint: case Category::Water:
  case Category::Road: case Category::Building: case Category::Boundary: case Category::PlaceName:
    return true;
  default: return false;
  }
}
bool finiteBounds(const QgsRectangle& bounds) {
  return std::isfinite(bounds.xMinimum()) && std::isfinite(bounds.yMinimum())
      && std::isfinite(bounds.xMaximum()) && std::isfinite(bounds.yMaximum())
      && bounds.xMaximum() >= bounds.xMinimum() && bounds.yMaximum() >= bounds.yMinimum();
}
bool contains(const QgsRectangle& outer, const QgsRectangle& inner) {
  return inner.xMinimum() >= outer.xMinimum() && inner.yMinimum() >= outer.yMinimum()
      && inner.xMaximum() <= outer.xMaximum() && inner.yMaximum() <= outer.yMaximum();
}
QList<int> codes(TopographicSheets::Zone zone) {
  switch (zone) {
  case TopographicSheets::Zone::West: return {5180, 5185};
  case TopographicSheets::Zone::Central: return {5181, 5182, 5186};
  case TopographicSheets::Zone::East: return {5183, 5187};
  case TopographicSheets::Zone::EastSea: return {5184, 5188};
  default: return {};
  }
}
struct Candidate {
  QgsCoordinateReferenceSystem crs;
  QgsRectangle allowed;
};

// Read the formal frame, not the dataset extent (which includes marginal text).
// Provider ownership stays on the caller's worker thread; no project is touched.
QgsPolylineXY sourceFrame(const QList<TopographicCatalog::Record>& records,
                          const QgsCoordinateTransformContext& context) {
  QString source;
  QString layerName;
  for (const auto& record : records) {
    if (record.cadLayer != QLatin1String("H0017334")) continue;
    if (QFileInfo(record.source).suffix().compare(QLatin1String("dxf"), Qt::CaseInsensitive) != 0
        || (!source.isEmpty() && (source != record.source || layerName != record.layerName))) return {};
    source = record.source;
    layerName = record.layerName;
  }
  if (source.isEmpty()) return {};
  // A verified one-sheet download cannot borrow a frame from another file.
  for (const auto& record : records) if (record.source != source) return {};
  QgsDataProvider::ProviderOptions options;
  options.transformContext = context;
  const auto uri = QgsProviderRegistry::instance()->encodeUri(QStringLiteral("ogr"),
      {{QStringLiteral("path"), source}, {QStringLiteral("layerName"), layerName}});
  // A display layer also loads styles/relations from the GUI-owned project.
  // The worker only needs the read-only provider's geometry, with no GUI state.
  std::unique_ptr<QgsDataProvider> owner(QgsProviderRegistry::instance()->createProvider(
      QStringLiteral("ogr"), uri, options, Qgis::DataProviderReadFlag::ForceReadOnly
        | Qgis::DataProviderReadFlag::SkipFeatureCount | Qgis::DataProviderReadFlag::SkipGetExtent));
  auto* provider = qobject_cast<QgsVectorDataProvider*>(owner.get());
  if (!provider || !provider->isValid()
      || !provider->setSubsetString(QStringLiteral("\"Layer\" = 'H0017334'"), false)) return {};
  auto iterator = provider->getFeatures();
  QgsFeature feature;
  if (!iterator.nextFeature(feature)) return {};
  const auto geometry = feature.geometry();
  if (geometry.isMultipart() || geometry.type() != Qgis::GeometryType::Line) return {};
  const auto points = geometry.asPolyline();
  // Bound malformed inputs; genuine NGII frames contain only tens of vertices.
  if (points.size() < 5 || points.size() > 4096 || points.first() != points.last()
      || iterator.nextFeature(feature)) return {};
  return points;
}

struct FrameMatch {
  QgsRectangle allowed;
  double residual = 0.;
  double displacement = 0.;
};

std::optional<FrameMatch> matchSourceFrame(const QgsPolylineXY& points,
    const QgsCoordinateReferenceSystem& crs, const QgsRectangle& officialBounds,
    const QgsCoordinateTransformContext& context) {
  if (points.isEmpty()) return {};
  // 1:25,000 neatline: 7.5 arcmin each side. Jeju sheets such as 126°10' sit
  // 30 arcsec off a 0.025° lattice, about 773 m, so that lattice is not required.
  constexpr double SheetDegrees = .125;
  constexpr double SheetDegreeTolerance = 1e-5;
  constexpr double MaxResidualMetres = .25;
  const QgsCoordinateReferenceSystem geographic(QStringLiteral("EPSG:4737"));
  const QgsCoordinateReferenceSystem inventory(QStringLiteral("EPSG:5179"));
  QgsCoordinateTransform toGeographic(crs, geographic, context);
  QgsCoordinateTransform toSource(geographic, crs, context);
  QgsCoordinateTransform toInventory(geographic, inventory, context);
  for (auto* transform : {&toGeographic, &toSource, &toInventory}) {
    transform->setAllowFallbackTransforms(false);
    transform->setBallparkTransformsAreAppropriate(false);
    if (!transform->isValid()) return {};
  }
  QgsPolylineXY geographicPoints;
  for (const auto& point : points) {
    if (!std::isfinite(point.x()) || !std::isfinite(point.y())) return {};
    geographicPoints.append(toGeographic.transform(point));
  }
  const auto rectangle = QgsGeometry::fromPolylineXY(geographicPoints).boundingBox();
  if (!finiteBounds(rectangle)
      || std::abs(rectangle.width() - SheetDegrees) > SheetDegreeTolerance
      || std::abs(rectangle.height() - SheetDegrees) > SheetDegreeTolerance
      || !crs.bounds().contains(rectangle.center())) return {};
  const auto originalCenter = toInventory.transform(rectangle.center());
  const auto indexCenter = officialBounds.center();
  // The 0.25 m residual above is the CRS evidence. Jeju 2024 1:25,000 frames are
  // valid EPSG:5186 rectangles about 4 km from the NGII index, so a quarter of
  // the sheet (~3 km) rejected them. 50 km matches the official-footprint cap.
  // Do not enlarge the 100 m extent tolerance or move the source geometry.
  constexpr double MaxIndexSeparationMetres = 50000.;
  const double separation = originalCenter.distance(indexCenter);
  if (!std::isfinite(separation) || separation > MaxIndexSeparationMetres) return {};

  FrameMatch match;
  QList<int> edgeMasks;
  const QgsPolylineXY corners = {{rectangle.xMinimum(), rectangle.yMinimum()},
    {rectangle.xMaximum(), rectangle.yMinimum()}, {rectangle.xMaximum(), rectangle.yMaximum()},
    {rectangle.xMinimum(), rectangle.yMaximum()}};
  int foundCorners = 0;
  for (qsizetype i = 0; i < points.size(); ++i) {
    const auto& point = geographicPoints.at(i);
    const double x = std::clamp(point.x(), rectangle.xMinimum(), rectangle.xMaximum());
    const double y = std::clamp(point.y(), rectangle.yMinimum(), rectangle.yMaximum());
    const QgsPolylineXY nearest = {{rectangle.xMinimum(), y}, {rectangle.xMaximum(), y},
                                   {x, rectangle.yMinimum()}, {x, rectangle.yMaximum()}};
    int edges = 0;
    double residual = std::numeric_limits<double>::infinity();
    for (int edge = 0; edge < 4; ++edge) {
      const double distance = points.at(i).distance(toSource.transform(nearest.at(edge)));
      if (distance <= MaxResidualMetres) edges |= 1 << edge;
      residual = std::min(residual, distance);
      if (points.at(i).distance(toSource.transform(corners.at(edge))) <= MaxResidualMetres)
        foundCorners |= 1 << edge;
    }
    if (!edges || !std::isfinite(residual)) return {};
    match.residual = std::max(match.residual, residual);
    edgeMasks.append(edges);
  }
  if (foundCorners != 15) return {};
  // No diagonal shortcuts between different sides of the frame.
  for (qsizetype i = 1; i < edgeMasks.size(); ++i)
    if (!(edgeMasks.at(i - 1) & edgeMasks.at(i))) return {};
  if (!QgsGeometry::fromPolygonXY({geographicPoints}).isGeosValid()) return {};
  if (toGeographic.fallbackOperationOccurred() || toSource.fallbackOperationOccurred()
      || toInventory.fallbackOperationOccurred()) return {};
  match.allowed = QgsGeometry::fromPolylineXY(points).boundingBox();
  match.allowed.grow(BoundaryMarginMetres);
  match.displacement = originalCenter.distance(indexCenter);
  return match;
}
}

static TopographicSourceCrs::Result resolveImpl(
    const QString& sheetNo, const QString& projection,
    const QList<TopographicCatalog::Record>& records, const QgsCoordinateTransformContext& context,
    const QgsRectangle* officialBounds5179) {
  TopographicSourceCrs::Result result;
  if (projection.trimmed().compare(QStringLiteral("GRS80"), Qt::CaseInsensitive) != 0) {
    result.error = QStringLiteral("공식 도엽의 GRS80 측지계가 확인되지 않아 좌표계를 자동 지정하지 않았습니다.");
    return result;
  }
  const auto sheet = TopographicSheets::resolve(sheetNo);
  if (!sheet) {
    result.error = QStringLiteral("공식 1:25,000 도엽번호가 유효하지 않아 좌표계를 확인할 수 없습니다.");
    return result;
  }
  const QgsCoordinateReferenceSystem geographic(QStringLiteral("EPSG:4737"));
  QgsCoordinateReferenceSystem extentCrs = geographic;
  QgsRectangle referenceExtent = sheet->geographicExtent;
  auto geographicCenter = sheet->geographicExtent.center();
  auto zone = sheet->zone;
  QString boundsEvidence = QStringLiteral("도엽번호 명목 격자");
  if (officialBounds5179) {
    // These broad limits are input-association sanity checks, not claims about
    // official cartographic standards. The download must still be independently
    // associated with this inventory record by its official sheet identifier.
    constexpr double MaxCenterDisplacement = 50000.;
    constexpr double MinSheetDimension = 1000.;
    constexpr double MaxSheetDimension = 30000.;
    if (!finiteBounds(*officialBounds5179) || officialBounds5179->width() < MinSheetDimension
        || officialBounds5179->height() < MinSheetDimension || officialBounds5179->width() > MaxSheetDimension
        || officialBounds5179->height() > MaxSheetDimension) {
      result.error = QStringLiteral("실제 공식 도곽의 범위 또는 크기가 유효하지 않아 자동 적재하지 않았습니다."); return result;
    }
    extentCrs = QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5179"));
    referenceExtent = *officialBounds5179;
    try {
      QgsCoordinateTransform toOfficial(geographic, extentCrs, context);
      QgsCoordinateTransform toGeographic(extentCrs, geographic, context);
      toOfficial.setAllowFallbackTransforms(false); toOfficial.setBallparkTransformsAreAppropriate(false);
      toGeographic.setAllowFallbackTransforms(false); toGeographic.setBallparkTransformsAreAppropriate(false);
      if (!toOfficial.isValid() || !toGeographic.isValid()) {
        result.error = QStringLiteral("공식 도곽의 좌표 변환을 준비하지 못했습니다."); return result;
      }
      const auto nominalCenter = toOfficial.transform(sheet->geographicExtent.center());
      const auto actualCenter = referenceExtent.center();
      geographicCenter = toGeographic.transform(actualCenter);
      const double displacement = std::hypot(nominalCenter.x() - actualCenter.x(), nominalCenter.y() - actualCenter.y());
      if (!std::isfinite(displacement) || displacement > MaxCenterDisplacement
          || !std::isfinite(geographicCenter.x()) || !std::isfinite(geographicCenter.y())
          || geographicCenter.x() < 124. || geographicCenter.x() >= 132.
          || toOfficial.fallbackOperationOccurred() || toGeographic.fallbackOperationOccurred()) {
        result.error = QStringLiteral("실제 공식 도곽과 도엽번호의 위치가 지나치게 달라 자료 연결을 확인해야 합니다."); return result;
      }
      if (geographicCenter.x() < 126.) zone = TopographicSheets::Zone::West;
      else if (geographicCenter.x() < 128.) zone = TopographicSheets::Zone::Central;
      else if (geographicCenter.x() < 130.) zone = TopographicSheets::Zone::East;
      else zone = TopographicSheets::Zone::EastSea;
      boundsEvidence = QStringLiteral("실제 공식 도곽(EPSG:5179) · 명목 격자 중심 차이 %1m · 자료 연결 점검 한도 50000m")
                           .arg(displacement, 0, 'f', 1);
    } catch (const QgsCsException& exception) {
      result.error = QStringLiteral("공식 도곽의 위치 확인 실패: %1").arg(exception.what()); return result;
    }
  }
  QList<int> primaryIndexes;
  QList<QgsCoordinateReferenceSystem> declared;
  QgsRectangle combined;
  bool hasCombined = false;
  for (int i = 0; i < records.size(); ++i) {
    const auto& record = records.at(i);
    declared.append(record.crsWkt.isEmpty() ? QgsCoordinateReferenceSystem()
                                          : QgsCoordinateReferenceSystem(record.crsWkt));
    if (!record.crsWkt.isEmpty() && !declared.last().isValid()) {
      result.error = QStringLiteral("원본 레이어의 좌표계 정보가 손상되어 자동 지정하지 않았습니다."); return result;
    }
    if (!primary(record.category) || !record.hasExtent) continue;
    if (!finiteBounds(record.extent)) {
      result.error = QStringLiteral("수치지형도 기본 레이어의 좌표 범위가 유효하지 않습니다."); return result;
    }
    primaryIndexes.append(i);
    if (!hasCombined) { combined = record.extent; hasCombined = true; }
    else {
      combined.setXMinimum(std::min(combined.xMinimum(), record.extent.xMinimum()));
      combined.setYMinimum(std::min(combined.yMinimum(), record.extent.yMinimum()));
      combined.setXMaximum(std::max(combined.xMaximum(), record.extent.xMaximum()));
      combined.setYMaximum(std::max(combined.yMaximum(), record.extent.yMaximum()));
    }
  }
  if (primaryIndexes.isEmpty()) {
    result.error = QStringLiteral("등고선·표고점·도로·건물·수계·행정경계·지명 중 좌표 범위가 확인되는 레이어가 없습니다.");
    return result;
  }
  result.evidence = QStringLiteral("공식 도엽 %1 · %2 · 기본 레이어 %3개 · 원본 동거리 %4~%5m, 북거리 %6~%7m · 도곽 여유 %8m")
      .arg(sheetNo, TopographicSheets::zoneName(zone)).arg(primaryIndexes.size())
      .arg(combined.xMinimum(), 0, 'f', 3).arg(combined.xMaximum(), 0, 'f', 3)
      .arg(combined.yMinimum(), 0, 'f', 3).arg(combined.yMaximum(), 0, 'f', 3)
      .arg(BoundaryMarginMetres, 0, 'f', 0);
  result.evidence += QStringLiteral(" · %1").arg(boundsEvidence);
  QList<Candidate> matched;
  QStringList tested;
  for (const int code : codes(zone)) {
    const QgsCoordinateReferenceSystem crs(QStringLiteral("EPSG:%1").arg(code));
    if (!crs.isValid()) {
      result.error = QStringLiteral("설치된 좌표계 정의 EPSG:%1을 읽지 못해 후보를 안전하게 비교할 수 없습니다.").arg(code);
      return result;
    }
    // EPSG area-of-use prevents using the Jeju 550 km origin on mainland sheets.
    // Check the sheet center because coastal grid cells naturally extend offshore.
    if (!crs.bounds().contains(geographicCenter)) continue;
    tested.append(crs.authid());
    QgsRectangle allowed;
    try {
      QgsCoordinateTransform transform(extentCrs, crs, context);
      transform.setAllowFallbackTransforms(false);
      transform.setBallparkTransformsAreAppropriate(false);
      if (!transform.isValid()) {
        result.error = QStringLiteral("도곽 좌표 변환을 준비하지 못했습니다: %1").arg(crs.authid()); return result;
      }
      allowed = transform.transformBoundingBox(referenceExtent);
      if (!finiteBounds(allowed) || transform.fallbackOperationOccurred()) {
        result.error = QStringLiteral("도곽 좌표 변환 결과를 확인하지 못했습니다: %1").arg(crs.authid()); return result;
      }
      allowed.grow(BoundaryMarginMetres);
    } catch (const QgsCsException& exception) {
      result.error = QStringLiteral("도곽 좌표 변환 실패(%1): %2").arg(crs.authid(), exception.what()); return result;
    }
    bool fits = true;
    for (const int index : primaryIndexes) {
      if ((declared.at(index).isValid() && declared.at(index) != crs)
          || !contains(allowed, records.at(index).extent)) { fits = false; break; }
    }
    if (fits) matched.append(Candidate{crs, allowed});
  }
  result.evidence += QStringLiteral(" · 비교 후보: %1").arg(tested.join(QStringLiteral(", ")));
  if (matched.isEmpty() && officialBounds5179) {
    const auto frame = sourceFrame(records, context);
    QString frameEvidence;
    QString warning;
    for (const int code : codes(zone)) {
      const QgsCoordinateReferenceSystem crs(QStringLiteral("EPSG:%1").arg(code));
      try {
        const auto match = matchSourceFrame(frame, crs, *officialBounds5179, context);
        if (!match) continue;
        bool fits = true;
        for (const int index : primaryIndexes) {
          if ((declared.at(index).isValid() && declared.at(index) != crs)
              || !contains(match->allowed, records.at(index).extent)) { fits = false; break; }
        }
        if (!fits) continue;
        matched.append(Candidate{crs, match->allowed});
        frameEvidence = QStringLiteral(" · 원본 도곽 전체 변 검증: %1, 최대 잔차 %2m (한도 0.25m), "
          "공식 색인과 중심 차이 %3m · 좌표 이동 없이 원본 도곽 범위 적용")
          .arg(crs.authid()).arg(match->residual, 0, 'f', 4).arg(match->displacement, 0, 'f', 1);
        warning = QStringLiteral("도엽 %1: 공식 색인과 원본 도곽 위치가 %2m 다릅니다. "
          "원본 도곽의 좌표계를 별도로 검증해 적재했습니다.")
          .arg(sheetNo).arg(match->displacement, 0, 'f', 0);
      } catch (const QgsCsException& exception) {
        result.error = QStringLiteral("원본 도곽 좌표 검증 실패: %1").arg(exception.what());
        return result;
      }
    }
    if (matched.size() == 1) {
      result.evidence += frameEvidence;
      result.warning = warning;
    }
  }
  if (matched.size() != 1) {
    result.error = matched.isEmpty()
        ? QStringLiteral("원본 좌표가 공식 도엽 위치와 일치하지 않습니다. 잘못된 좌표계로 자동 적재하지 않았습니다.")
        : QStringLiteral("여러 좌표계가 공식 도엽 위치와 일치하여 하나로 판별할 수 없습니다. 자동 적재하지 않았습니다.");
    return result;
  }
  const auto& selected = matched.first();
  for (int i = 0; i < records.size(); ++i) {
    const auto& record = records.at(i);
    if (!record.hasExtent || !finiteBounds(record.extent) || !contains(selected.allowed, record.extent)
        || (declared.at(i).isValid() && declared.at(i) != selected.crs))
      result.unverifiedRecordIndexes.append(i);
  }
  result.authId = selected.crs.authid();
  result.crsWkt = selected.crs.toWkt();
  result.evidence += QStringLiteral(" · 유일한 도곽 일치 후보 %1 · 미확인 레이어 %2개. 도곽 범위 대조이며 위성영상 화소 정합 검증은 아닙니다.")
                         .arg(result.authId).arg(result.unverifiedRecordIndexes.size());
  return result;
}

TopographicSourceCrs::Result TopographicSourceCrs::resolve(
    const QString& sheetNo, const QString& projection,
    const QList<TopographicCatalog::Record>& records, const QgsCoordinateTransformContext& context) {
  return resolveImpl(sheetNo, projection, records, context, nullptr);
}

TopographicSourceCrs::Result TopographicSourceCrs::resolve(
    const QString& sheetNo, const QString& projection,
    const QList<TopographicCatalog::Record>& records, const QgsCoordinateTransformContext& context,
    const QgsRectangle& officialBounds5179) {
  return resolveImpl(sheetNo, projection, records, context, &officialBounds5179);
}
