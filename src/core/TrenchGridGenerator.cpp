#include "TrenchGridGenerator.h"

#include <cmath>
#include <cstring>
#include <memory>
#include <limits>
#include <unordered_set>

#include <QFileInfo>
#include <QScopeGuard>

#include <gdal.h>
#include <ogr_api.h>
#include <ogr_feature.h>
#include <ogr_geometry.h>
#include <ogr_spatialref.h>
#include <ogrsf_frmts.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace TrenchGridGenerator {
namespace {

constexpr double kMaxWidth = 2.0;
constexpr double kMaxLength = 20.0;
constexpr size_t kMaxCells = 200000;
constexpr double kGeometryTolerance = 1e-7;

struct GeometryDeleter {
  void operator()(OGRGeometry* geometry) const { OGRGeometryFactory::destroyGeometry(geometry); }
};
using Geometry = std::unique_ptr<OGRGeometry, GeometryDeleter>;
using Dataset = std::unique_ptr<GDALDataset, decltype(&GDALClose)>;

bool validDimensions(double width, double length) {
  return std::isfinite(width) && width > 0.0 && width <= kMaxWidth &&
         std::isfinite(length) && length > 0.0 && length <= kMaxLength;
}

bool validSpec(const Spec& spec) {
  return validDimensions(spec.trenchWidth, spec.trenchLength) &&
         std::isfinite(spec.balkWidth) && spec.balkWidth >= 0.0 &&
         std::isfinite(spec.azimuthDeg) && std::isfinite(spec.originX) && std::isfinite(spec.originY);
}

bool validCell(const Cell& cell) {
  if (!validDimensions(cell.width, cell.length)) return false;
  for (const auto& point : cell.ring)
    if (!std::isfinite(point.first) || !std::isfinite(point.second)) return false;
  const auto& p = cell.ring;
  const double ux = p[1].first - p[0].first, uy = p[1].second - p[0].second;
  const double vx = p[3].first - p[0].first, vy = p[3].second - p[0].second;
  const double width = std::hypot(ux, uy), length = std::hypot(vx, vy);
  if (!(width > 0.0) || !(length > 0.0) ||
      std::abs(width - cell.width) > kGeometryTolerance ||
      std::abs(length - cell.length) > kGeometryTolerance)
    return false;
  // Four finite vertices must form the same closed rectangle as its attributes.
  return std::hypot(p[4].first - p[0].first, p[4].second - p[0].second) <= kGeometryTolerance &&
         std::hypot((p[2].first - p[1].first) - vx,
                    (p[2].second - p[1].second) - vy) <= kGeometryTolerance &&
         std::abs((ux / width) * (vx / length) + (uy / width) * (vy / length)) <= kGeometryTolerance;
}

}  // namespace

std::vector<Cell> build(const Spec& spec) {
  std::vector<Cell> out;
  const int rows = spec.rows;
  const int cols = spec.cols;
  if (!validSpec(spec) || rows <= 0 || cols <= 0 ||
      static_cast<size_t>(rows) > kMaxCells / static_cast<size_t>(cols))
    return out;
  const double w = spec.trenchWidth;
  const double len = spec.trenchLength;
  const double balk = spec.balkWidth;

  // Azimuth clockwise from north. Local +X = east, +Y = north at azimuth 0.
  const double a = spec.azimuthDeg * M_PI / 180.0;
  const double c = std::cos(a);
  const double s = std::sin(a);
  auto world = [&](double lx, double ly) {
    // clockwise about origin: x' = x cos + y sin, y' = -x sin + y cos
    return std::pair<double, double>{lx * c + ly * s + spec.originX,
                                     -lx * s + ly * c + spec.originY};
  };

  out.reserve(static_cast<size_t>(rows) * cols);
  int n = 1;
  for (int r = 0; r < rows; ++r) {
    for (int col = 0; col < cols; ++col) {
      const double x0 = col * (w + balk);
      const double y0 = r * (len + balk);
      Cell cell;
      cell.name = spec.namePrefix + QString::number(n++);
      cell.width = w;
      cell.length = len;
      const auto p0 = world(x0, y0);
      const auto p1 = world(x0 + w, y0);
      const auto p2 = world(x0 + w, y0 + len);
      const auto p3 = world(x0, y0 + len);
      cell.ring[0] = p0;
      cell.ring[1] = p1;
      cell.ring[2] = p2;
      cell.ring[3] = p3;
      cell.ring[4] = p0;
      if (!validCell(cell)) return {};
      out.push_back(cell);
    }
  }
  return out;
}

namespace {

// 가장자리에서 잘린 트렌치가 이보다 짧으면 조사에 쓸모가 없어 버린다.
constexpr double kMinTrimLen = 4.0;

OGRGeometry* geomFromWkb(const QByteArray& wkb) {
  if (wkb.isEmpty())
    return nullptr;
  OGRGeometry* g = nullptr;
  OGRGeometryFactory::createFromWkb(wkb.constData(), nullptr, &g, static_cast<size_t>(wkb.size()));
  return g;
}

// Why an area cannot hold trenches; empty when it can.
QString areaProblemOf(const OGRGeometry* area) {
  if (!area || area->IsEmpty())
    return QStringLiteral("조사구역 면을 읽지 못했습니다. 조사구역을 다시 그리거나 선택하세요.");
  const OGRwkbGeometryType gt = wkbFlatten(area->getGeometryType());
  if (gt != wkbPolygon && gt != wkbMultiPolygon)
    return QStringLiteral("조사구역이 면(폴리곤)이 아니어서 시굴격자를 놓을 수 없습니다.");
  if (!area->IsValid())
    return QStringLiteral("조사구역 경계가 스스로 겹치거나 꼬여 있어(유효하지 않은 도형) 시굴격자를 놓을 수 "
                          "없습니다. 꼭짓점을 고쳐 경계를 바로잡은 뒤 다시 적용하세요.");
  return {};
}

QByteArray wkbFromGeom(OGRGeometry* g) {
  QByteArray out;
  if (!g)
    return out;
  out.resize(static_cast<int>(g->WkbSize()));
  g->exportToWkb(wkbNDR, reinterpret_cast<unsigned char*>(out.data()));
  return out;
}

// 폴리곤/멀티폴리곤의 모든 꼭짓점을 훑는다(교차 결과의 국소 좌표 범위를 잴 때).
template <typename F>
void forEachVertex(const OGRGeometry* g, F&& fn) {
  if (!g) return;
  const OGRwkbGeometryType t = wkbFlatten(g->getGeometryType());
  if (t == wkbPolygon) {
    const auto* poly = g->toPolygon();
    for (const OGRLinearRing* ring : *poly)
      for (const OGRPoint& pt : *ring) fn(pt.getX(), pt.getY());
  } else if (t == wkbMultiPolygon || t == wkbGeometryCollection) {
    const auto* coll = g->toGeometryCollection();
    for (const OGRGeometry* part : *coll) forEachVertex(part, fn);
  }
}

}  // namespace

PickedArea pickAutoFillArea(const std::vector<SurveyPoly>& features,
                            const std::vector<qint64>& selectedFids, bool useAll) {
  PickedArea out;
  std::vector<const SurveyPoly*> valid;
  valid.reserve(features.size());
  for (const SurveyPoly& f : features) {
    if (f.wkb.isEmpty())
      continue;
    valid.push_back(&f);
  }
  out.totalCount = static_cast<int>(valid.size());
  if (valid.empty())
    return out;

  std::vector<const SurveyPoly*> use;
  const std::unordered_set<qint64> sel(selectedFids.begin(), selectedFids.end());
  if (!sel.empty()) {
    for (const SurveyPoly* f : valid) {
      if (sel.count(f->fid))
        use.push_back(f);
    }
    if (!use.empty())
      out.usedSelection = true;
  }
  if (use.empty()) {
    if (useAll) {
      // 사용자가 「조사구역 전체」를 고른 경우에만 전부 합친다.
      use = valid;
    } else {
      const SurveyPoly* newest = valid.front();
      for (const SurveyPoly* f : valid) {
        if (f->fid > newest->fid)
          newest = f;
      }
      use.push_back(newest);
    }
    out.usedSelection = false;
  }
  out.usedCount = static_cast<int>(use.size());

  OGRGeometry* acc = nullptr;
  for (const SurveyPoly* f : use) {
    OGRGeometry* g = geomFromWkb(f->wkb);
    if (!g)
      continue;
    if (!acc) {
      acc = g;
      continue;
    }
    OGRGeometry* uni = acc->Union(g);
    OGRGeometryFactory::destroyGeometry(acc);
    OGRGeometryFactory::destroyGeometry(g);
    acc = uni;
  }
  if (!acc)
    return out;
  out.areaM2 = OGR_G_Area(OGRGeometry::ToHandle(acc));
  out.wkb = wkbFromGeom(acc);
  OGRGeometryFactory::destroyGeometry(acc);
  return out;
}

std::vector<Cell> buildInArea(const Spec& spec, const QByteArray& areaWkb, QString* reasonOut) {
  std::vector<Cell> out;
  if (reasonOut) reasonOut->clear();
  const auto fail = [reasonOut](const QString& why) {
    if (reasonOut) *reasonOut = why;
    return std::vector<Cell>{};
  };
  if (areaWkb.isEmpty())
    return fail(QStringLiteral("조사구역이 없습니다. 조사구역을 먼저 그리거나 선택하세요."));
  if (!validSpec(spec))
    return fail(QStringLiteral("트렌치 규격이 맞지 않습니다. 폭은 2 m 이하, 길이는 20 m 이하, 둑은 0 m 이상이어야 합니다."));
  const double w = spec.trenchWidth;
  const double len = spec.trenchLength;
  const double balk = spec.balkWidth;
  Geometry area(geomFromWkb(areaWkb));
  if (const QString problem = areaProblemOf(area.get()); !problem.isEmpty())
    return fail(problem);

  OGREnvelope env;
  area->getEnvelope(&env);
  const double ax = (env.MinX + env.MaxX) * 0.5;
  const double ay = (env.MinY + env.MaxY) * 0.5;

  const double a = spec.azimuthDeg * M_PI / 180.0;
  const double c = std::cos(a);
  const double s = std::sin(a);
  auto world = [&](double lx, double ly) {
    return std::pair<double, double>{lx * c + ly * s + ax, -lx * s + ly * c + ay};
  };
  auto toLocal = [&](double wx, double wy) {
    const double dx = wx - ax;
    const double dy = wy - ay;
    return std::pair<double, double>{dx * c - dy * s, dx * s + dy * c};
  };

  // Local-frame bounds of the (rotated) envelope corners; superset of the area.
  double uMin = 0, uMax = 0, vMin = 0, vMax = 0;
  bool first = true;
  const double cxs[2] = {env.MinX, env.MaxX};
  const double cys[2] = {env.MinY, env.MaxY};
  for (double cx : cxs) {
    for (double cy : cys) {
      const auto l = toLocal(cx, cy);
      if (first) {
        uMin = uMax = l.first;
        vMin = vMax = l.second;
        first = false;
      } else {
        uMin = std::min(uMin, l.first);
        uMax = std::max(uMax, l.first);
        vMin = std::min(vMin, l.second);
        vMax = std::max(vMax, l.second);
      }
    }
  }

  const double stepU = w + balk;
  const double stepV = len + balk;
  const double countU = (uMax - uMin) / stepU;
  const double countV = (vMax - vMin) / stepV;
  const QString tooMany = QStringLiteral("구역에 비해 트렌치 간격이 좁아 칸 수가 한도를 넘습니다. 둑(간격)을 키우세요.");
  if (!std::isfinite(countU) || !std::isfinite(countV) || countU < 0.0 || countV < 0.0 ||
      countU > double(kMaxCells) - 2.0 || countV > double(kMaxCells) - 2.0)
    return fail(tooMany);
  const long long nu = static_cast<long long>(countU) + 2;
  const long long nv = static_cast<long long>(countV) + 2;
  if (nu * nv > static_cast<long long>(kMaxCells))
    return fail(tooMany);

  auto collect = [&]() {
    int n = 1;
    std::vector<Cell> got;
    for (long long j = 0; j < nv; ++j) {
      const double v0 = vMin + j * stepV;
      for (long long i = 0; i < nu; ++i) {
        const double u0 = uMin + i * stepU;
        const auto p0 = world(u0, v0);
        const auto p1 = world(u0 + w, v0);
        const auto p2 = world(u0 + w, v0 + len);
        const auto p3 = world(u0, v0 + len);
        OGRLinearRing ring;
        ring.addPoint(p0.first, p0.second);
        ring.addPoint(p1.first, p1.second);
        ring.addPoint(p2.first, p2.second);
        ring.addPoint(p3.first, p3.second);
        ring.addPoint(p0.first, p0.second);
        OGRPolygon poly;
        poly.addRing(&ring);
        double keepV0 = v0;
        double keepLen = len;
        if (!area->Contains(&poly)) {
          // 경계에 걸치면 통째로 버리지 않는다. 그러면 가장자리가 비어 배치가
          // 고르지 않다. 폭 2 m는 그대로 두고 길이만 잘라 구역 안에 넣는다.
          if (!area->Intersects(&poly))
            continue;
          Geometry inter(area->Intersection(&poly));
          if (!inter || inter->IsEmpty()) {
            continue;
          }
          double loV = 1e300, hiV = -1e300;
          forEachVertex(inter.get(), [&](double wx, double wy) {
            const double lv = toLocal(wx, wy).second;
            loV = std::min(loV, lv);
            hiV = std::max(hiV, lv);
          });
          loV = std::max(loV, v0);
          hiV = std::min(hiV, v0 + len);
          double cut = hiV - loV;
          if (!(cut >= kMinTrimLen) || cut > len)
            continue;
          // 교차 상자는 모서리에서 조금 넘칠 수 있다. 들어갈 때까지 양끝을 줄인다.
          bool fits = false;
          for (int attempt = 0; attempt < 4 && cut >= kMinTrimLen; ++attempt) {
            const auto q0 = world(u0, loV);
            const auto q1 = world(u0 + w, loV);
            const auto q2 = world(u0 + w, loV + cut);
            const auto q3 = world(u0, loV + cut);
            OGRLinearRing tr;
            tr.addPoint(q0.first, q0.second);
            tr.addPoint(q1.first, q1.second);
            tr.addPoint(q2.first, q2.second);
            tr.addPoint(q3.first, q3.second);
            tr.addPoint(q0.first, q0.second);
            OGRPolygon tp;
            tp.addRing(&tr);
            if (area->Contains(&tp)) {
              fits = true;
              break;
            }
            const double shrink = std::max(0.25, cut * 0.05);
            loV += shrink * 0.5;
            cut -= shrink;
          }
          if (!fits || cut < kMinTrimLen)
            continue;
          keepV0 = loV;
          keepLen = cut;
        }
        const auto k0 = world(u0, keepV0);
        const auto k1 = world(u0 + w, keepV0);
        const auto k2 = world(u0 + w, keepV0 + keepLen);
        const auto k3 = world(u0, keepV0 + keepLen);
        Cell cell;
        cell.name = spec.namePrefix + QString::number(n++);
        cell.width = w;
        cell.length = keepLen;
        cell.ring[0] = k0;
        cell.ring[1] = k1;
        cell.ring[2] = k2;
        cell.ring[3] = k3;
        cell.ring[4] = k0;
        if (!validCell(cell)) continue;
        got.push_back(cell);
      }
    }
    return got;
  };

  out = collect();
  if (out.empty())
    return fail(QStringLiteral("현재 규격과 방향으로는 구역 안에 들어가는 트렌치가 없습니다. 구역이 좁거나 "
                               "짧으면 회전이나 규격을 바꿔 보세요."));
  return out;
}

SlopeAspect upslopeAspect(const std::vector<ElevSample>& samples, double minSlopePct) {
  SlopeAspect out;
  if (samples.size() < 3)
    return out;

  // 표본에 z = a·x + b·y + c 평면을 최소제곱으로 맞춘다. 중심을 빼서 좌표가
  // 큰 5186/5187 미터값이어도 정규방정식이 수치적으로 무너지지 않게 한다.
  double mx = 0.0, my = 0.0, mz = 0.0;
  for (const ElevSample& p : samples) {
    mx += p.x;
    my += p.y;
    mz += p.z;
  }
  const double n = static_cast<double>(samples.size());
  mx /= n;
  my /= n;
  mz /= n;

  double sxx = 0.0, sxy = 0.0, syy = 0.0, sxz = 0.0, syz = 0.0;
  for (const ElevSample& p : samples) {
    const double dx = p.x - mx;
    const double dy = p.y - my;
    const double dz = p.z - mz;
    sxx += dx * dx;
    sxy += dx * dy;
    syy += dy * dy;
    sxz += dx * dz;
    syz += dy * dz;
  }
  const double det = sxx * syy - sxy * sxy;
  if (std::abs(det) < 1e-9)
    return out;  // 표본이 한 줄로 서 있으면 기울기를 못 정한다.

  const double a = (sxz * syy - syz * sxy) / det;  // dz/dx
  const double b = (syz * sxx - sxz * sxy) / det;  // dz/dy

  const double grad = std::hypot(a, b);
  out.slopePct = grad * 100.0;
  if (out.slopePct < minSlopePct)
    return out;  // 평지 — 방향을 정할 근거가 없다.

  // (a, b)는 오르막을 가리킨다. 방위는 북(+y) 기준 시계 방향이므로 atan2(동, 북).
  double az = std::atan2(a, b) * 180.0 / M_PI;
  if (az < 0.0)
    az += 360.0;
  out.azimuthDeg = az;
  out.valid = true;
  return out;
}

RatioFill buildForTargetRatio(const QByteArray& areaWkb, double targetPct, double width,
                              double azimuthDeg) {
  RatioFill best;
  if (areaWkb.isEmpty() || !validDimensions(width, kMaxLength) ||
      !std::isfinite(targetPct) || targetPct <= 0.0 || targetPct > 100.0 ||
      !std::isfinite(azimuthDeg)) {
    best.error = QStringLiteral("폭은 0 초과 2 m 이하, 목표 비율은 0 초과 100% 이하의 유효한 값이어야 합니다.");
    return best;
  }

  Geometry area(geomFromWkb(areaWkb));
  if (const QString problem = areaProblemOf(area.get()); !problem.isEmpty()) {
    best.error = problem;
    return best;
  }
  const double areaM2 = OGR_G_Area(OGRGeometry::ToHandle(area.get()));
  const double targetArea = areaM2 * (targetPct / 100.0);
  if (!std::isfinite(areaM2) || !(areaM2 > 1e-6) || !(targetArea > 0.0)) {
    best.error = QStringLiteral("조사구역 면적과 목표 면적을 계산하지 못했습니다.");
    return best;
  }
  best.areaM2 = areaM2;

  Spec s;
  s.trenchWidth = width;
  s.azimuthDeg = azimuthDeg;
  s.namePrefix = targetPct <= 3.5 ? QStringLiteral("Sp-") : QStringLiteral("Tr-");

  double bestArea = std::numeric_limits<double>::infinity();
  const double lengths[] = {8.0, 10.0, 12.0, 16.0, 20.0};
  for (double len : lengths) {
    s.trenchLength = len;
    for (double balk = 2.0; balk <= 120.0; balk += 2.0) {
      s.balkWidth = balk;
      auto cells = buildInArea(s, areaWkb);
      if (cells.empty())
        continue;
      const double candidateArea = totalArea(cells);
      if (candidateArea >= targetArea && candidateArea < bestArea) {
        bestArea = candidateArea;
        best.cells = std::move(cells);
        best.balk = balk;
        best.azimuthDeg = azimuthDeg;
      }
      if (bestArea - targetArea < std::max(1e-9, targetArea * 1e-10)) break;
    }
    if (bestArea - targetArea < std::max(1e-9, targetArea * 1e-10)) break;
  }
  if (best.cells.empty()) {
    best.error = QStringLiteral("현재 구역과 방향에서 폭 %1 m·길이 20 m 이하로 목표 %2%를 배치할 수 없습니다. 기존 격자는 유지됩니다.")
                     .arg(width).arg(targetPct);
    return best;
  }

  // Shrinking only the length keeps each result inside its original rectangle:
  // containment and separation survive, including holes and concave boundaries.
  const double factor = targetArea / bestArea;
  for (Cell& cell : best.cells) {
    const auto original = cell.ring;
    const double dx = (original[3].first - original[0].first) * ((1.0 - factor) * 0.5);
    const double dy = (original[3].second - original[0].second) * ((1.0 - factor) * 0.5);
    cell.ring[0] = {original[0].first + dx, original[0].second + dy};
    cell.ring[1] = {original[1].first + dx, original[1].second + dy};
    cell.ring[2] = {original[2].first - dx, original[2].second - dy};
    cell.ring[3] = {original[3].first - dx, original[3].second - dy};
    cell.ring[4] = cell.ring[0];
    cell.length *= factor;
    if (!validCell(cell)) {
      best.cells.clear();
      best.length = 0.0;
      best.error = QStringLiteral("목표 면적에 맞는 유효한 시굴격자를 계산하지 못했습니다.");
      return best;
    }
    best.length = std::max(best.length, cell.length);
  }
  best.ratioPct = totalArea(best.cells) / areaM2 * 100.0;
  return best;
}

double totalArea(const std::vector<Cell>& cells) {
  double sum = 0.0;
  for (const Cell& c : cells)
    sum += c.width * c.length;
  return sum;
}

bool cellsValid(const std::vector<Cell>& cells, QString* errorOut) {
  if (errorOut) errorOut->clear();
  const auto fail = [errorOut](const QString& message) {
    if (errorOut) *errorOut = message;
    return false;
  };
  if (cells.empty() || cells.size() > kMaxCells)
    return fail(QStringLiteral("기록할 시굴격자가 없거나 개수가 너무 많습니다."));
  for (const Cell& cell : cells)
    if (!validCell(cell))
      return fail(QStringLiteral("시굴격자 규격과 도형이 일치하지 않습니다. 폭은 최대 2 m, 길이는 최대 20 m입니다."));
  return true;
}

bool writeGpkg(const QString& gpkgPath, const QString& layerName, const std::vector<Cell>& cells,
               const QString& authid, QString* errorOut) {
  if (errorOut) errorOut->clear();
  const auto fail = [errorOut](const QString& message) {
    if (errorOut) *errorOut = message;
    return false;
  };
  // Validate everything before opening a write connection or deleting anything.
  if (gpkgPath.isEmpty() || layerName.isEmpty())
    return fail(QStringLiteral("GPKG 경로와 레이어 이름이 필요합니다."));
  if (!cellsValid(cells, errorOut))
    return false;
  OGRSpatialReference srs;
  if (srs.SetFromUserInput(authid.toUtf8().constData()) != OGRERR_NONE)
    return fail(QStringLiteral("시굴격자 작업 좌표계를 읽지 못했습니다."));
  // Cell coordinates and GeoPackage layers use x/y GIS order, even when the
  // EPSG definition declares northing before easting (5186/5187).
  srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
  if (!srs.IsProjected() ||
      std::abs(srs.GetLinearUnits() - 1.0) > 1e-9)
    return fail(QStringLiteral("시굴격자는 미터 단위의 작업 좌표계로 저장해야 합니다."));

  GDALAllRegister();
  const char* allowedDrivers[] = {"GPKG", nullptr};
  Dataset ds(static_cast<GDALDataset*>(GDALOpenEx(
                 gpkgPath.toUtf8().constData(), GDAL_OF_VECTOR | GDAL_OF_UPDATE,
                 allowedDrivers, nullptr, nullptr)), &GDALClose);
  if (!ds)
    return fail(QStringLiteral("GPKG를 열 수 없습니다. 먼저 새 조사를 만드세요."));
  if (ds->StartTransaction() != OGRERR_NONE)
    return fail(QStringLiteral("시굴격자 교체 작업을 시작하지 못했습니다. 기존 격자는 유지됩니다."));
  bool transactionActive = true;
  const auto rollback = qScopeGuard([&]() {
    if (transactionActive && ds->RollbackTransaction() != OGRERR_NONE && errorOut)
      *errorOut += QStringLiteral(" 기존 격자 복구 결과를 확인하지 못했습니다.");
  });

  OGRLayer* lyr = ds->GetLayerByName(layerName.toUtf8().constData());
  if (!lyr) {
    lyr = ds->CreateLayer(layerName.toUtf8().constData(), &srs, wkbPolygon, nullptr);
    if (lyr) {
      OGRFieldDefn fName("name", OFTString);
      fName.SetWidth(64);
      OGRFieldDefn fW("width", OFTReal);
      OGRFieldDefn fL("length", OFTReal);
      if (lyr->CreateField(&fName) != OGRERR_NONE || lyr->CreateField(&fW) != OGRERR_NONE ||
          lyr->CreateField(&fL) != OGRERR_NONE)
        return fail(QStringLiteral("시굴격자 속성 필드를 만들지 못했습니다."));
    }
  }
  if (!lyr)
    return fail(QStringLiteral("시굴격자 레이어를 만들 수 없습니다."));
  if (wkbFlatten(lyr->GetGeomType()) != wkbPolygon || !lyr->GetSpatialRef() ||
      !srs.IsSame(lyr->GetSpatialRef()))
    return fail(QStringLiteral("기존 시굴격자의 도형 형식 또는 좌표계가 작업 좌표계와 다릅니다."));

  const int iName = lyr->FindFieldIndex("name", TRUE);
  const int iW = lyr->FindFieldIndex("width", TRUE);
  const int iL = lyr->FindFieldIndex("length", TRUE);
  if (iName < 0 || iW < 0 || iL < 0 ||
      lyr->GetLayerDefn()->GetFieldDefn(iName)->GetType() != OFTString ||
      lyr->GetLayerDefn()->GetFieldDefn(iW)->GetType() != OFTReal ||
      lyr->GetLayerDefn()->GetFieldDefn(iL)->GetType() != OFTReal)
    return fail(QStringLiteral("기존 시굴격자의 속성 필드가 올바르지 않습니다."));

  std::vector<GIntBig> oldFids;
  lyr->ResetReading();
  CPLErrorReset();
  while (OGRFeature* raw = lyr->GetNextFeature()) {
    std::unique_ptr<OGRFeature, decltype(&OGRFeature::DestroyFeature)> feature(raw, &OGRFeature::DestroyFeature);
    oldFids.push_back(feature->GetFID());
  }
  if (CPLGetLastErrorType() >= CE_Failure)
    return fail(QStringLiteral("기존 시굴격자를 읽지 못했습니다."));
  for (GIntBig fid : oldFids)
    if (lyr->DeleteFeature(fid) != OGRERR_NONE)
      return fail(QStringLiteral("기존 시굴격자를 교체하지 못했습니다."));

  for (const Cell& cell : cells) {
    OGRLinearRing ring;
    for (const auto& p : cell.ring)
      ring.addPoint(p.first, p.second);
    OGRPolygon poly;
    poly.addRing(&ring);
    OGRFeature feat(lyr->GetLayerDefn());
    if (feat.SetGeometry(&poly) != OGRERR_NONE)
      return fail(QStringLiteral("시굴격자 도형을 기록하지 못했습니다: %1").arg(cell.name));
    feat.SetField(iName, cell.name.toUtf8().constData());
    feat.SetField(iW, cell.width);
    feat.SetField(iL, cell.length);
    if (lyr->CreateFeature(&feat) != OGRERR_NONE)
      return fail(QStringLiteral("트렌치를 쓰지 못했습니다: %1").arg(cell.name));
  }
  if (lyr->SyncToDisk() != OGRERR_NONE || ds->FlushCache() != CE_None)
    return fail(QStringLiteral("시굴격자 기록을 마치지 못했습니다."));
  if (ds->CommitTransaction() != OGRERR_NONE)
    return fail(QStringLiteral("시굴격자 교체를 완료하지 못했습니다."));
  transactionActive = false;
  return true;
}

bool clearLayer(const QString& gpkgPath, const QString& layerName, QString* errorOut) {
  if (errorOut) errorOut->clear();
  if (gpkgPath.isEmpty() || layerName.isEmpty() || !QFileInfo::exists(gpkgPath))
    return true;  // no file yet -> nothing to clear
  GDALAllRegister();
  GDALDataset* ds = static_cast<GDALDataset*>(GDALOpenEx(
      gpkgPath.toUtf8().constData(), GDAL_OF_VECTOR | GDAL_OF_UPDATE, nullptr, nullptr, nullptr));
  if (!ds) {
    // The file is there but cannot be written: nothing was cleared.
    if (errorOut)
      *errorOut = QStringLiteral("조사 파일을 열 수 없어 기존 시굴격자를 지우지 못했습니다.");
    return false;
  }
  OGRLayer* lyr = ds->GetLayerByName(layerName.toUtf8().constData());
  bool ok = true;
  if (lyr) {
    std::vector<GIntBig> fids;
    lyr->ResetReading();
    while (OGRFeature* f = lyr->GetNextFeature()) {
      fids.push_back(f->GetFID());
      OGRFeature::DestroyFeature(f);
    }
    for (GIntBig fid : fids) {
      if (lyr->DeleteFeature(fid) != OGRERR_NONE) {
        ok = false;
        if (errorOut)
          *errorOut = QStringLiteral("기존 시굴격자를 지우지 못했습니다.");
        break;
      }
    }
    if (lyr->SyncToDisk() != OGRERR_NONE && ok) {
      ok = false;
      if (errorOut)
        *errorOut = QStringLiteral("기존 시굴격자 지우기를 파일에 기록하지 못했습니다.");
    }
  }
  GDALClose(ds);
  return ok;
}

}  // namespace TrenchGridGenerator
