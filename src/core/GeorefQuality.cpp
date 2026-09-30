#include "GeorefQuality.h"

#include <algorithm>
#include <cmath>

namespace GeorefQuality {

Shape shapeOf(const GeorefService::Affine& a) {
  Shape s;
  if (!a.valid) return s;
  // Closed-form singular values of the 2x2 linear part [[a b] [d e]].
  const double E = 0.5 * (a.a + a.e);
  const double F = 0.5 * (a.a - a.e);
  const double G = 0.5 * (a.d + a.b);
  const double H = 0.5 * (a.d - a.b);
  const double Q = std::hypot(E, H);
  const double R = std::hypot(F, G);
  s.scaleMajor = Q + R;
  s.scaleMinor = std::abs(Q - R);
  if (!(s.scaleMinor > 0.0) || !std::isfinite(s.scaleMajor)) return s;
  s.anisotropy = s.scaleMajor / s.scaleMinor;
  // Angle between the images of the source x and y axes; a similarity keeps 90 degrees
  // (with or without the Y-down flip).
  const double lx = std::hypot(a.a, a.d);
  const double ly = std::hypot(a.b, a.e);
  if (lx > 0.0 && ly > 0.0) {
    const double c = std::min(1.0, std::abs(a.a * a.b + a.d * a.e) / (lx * ly));
    s.shearDeg = std::asin(c) * 180.0 / 3.14159265358979323846;
  }
  s.valid = true;
  return s;
}

Report assess(const QVector<GeorefService::Pair>& pairs, bool sourceYDown) {
  Report r;
  r.pairCount = pairs.size();
  r.affine = GeorefService::fromPairs(pairs, sourceYDown);
  if (!r.affine.valid) return r;
  r.shape = shapeOf(r.affine);
  r.measurable = pairs.size() >= 4;
  if (!r.measurable) return r;
  double acc = 0;
  for (int i = 0; i < pairs.size(); ++i) {
    double mx = 0, my = 0;
    GeorefService::transform(r.affine, pairs[i].srcX, pairs[i].srcY, &mx, &my);
    const double d = std::hypot(mx - pairs[i].mapX, my - pairs[i].mapY);
    r.residuals.append(d);
    acc += d * d;
    if (d > r.maxResidual || r.maxIndex < 0) {
      r.maxResidual = d;
      r.maxIndex = i;
    }
  }
  r.rms = std::sqrt(acc / pairs.size());
  return r;
}

QString rowText(const Report& r, int index) {
  QString s = QStringLiteral("%1번  왼쪽 → 오른쪽").arg(index + 1);
  if (r.measurable && index >= 0 && index < r.residuals.size()) {
    s += QStringLiteral(" · 어긋남 %1 m").arg(r.residuals[index], 0, 'f', 2);
    if (index == r.maxIndex && r.maxResidual > kResidualWarn) s += QStringLiteral(" (가장 큼)");
  }
  return s;
}

QStringList notes(const Report& r) {
  QStringList out;
  if (!r.affine.valid) return out;
  if (r.pairCount == 3) out << QStringLiteral("점 4개부터 점마다 어긋남을 잽니다");
  if (r.measurable && r.maxResidual > kResidualWarn) {
    out << QStringLiteral("주의: %1번 점이 %2 m 어긋납니다. 두 쪽 점을 다시 확인하세요.")
               .arg(r.maxIndex + 1)
               .arg(r.maxResidual, 0, 'f', 2);
  }
  if (r.pairCount >= 3 && r.shape.valid && r.shape.anisotropy > kAnisotropyWarn) {
    out << QStringLiteral("주의: 가로·세로 배율이 %1% 다릅니다. 잘못 찍은 점이 있으면 그림이 늘어납니다.")
               .arg((r.shape.anisotropy - 1.0) * 100.0, 0, 'f', 0);
  }
  if (r.pairCount >= 3 && r.shape.valid && r.shape.shearDeg > kShearWarnDeg) {
    out << QStringLiteral("주의: 그림 축이 %1° 비뚤어집니다. 잘못 찍은 점이 있는지 보세요.")
               .arg(r.shape.shearDeg, 0, 'f', 1);
  }
  return out;
}

QString summary(const Report& r) {
  if (r.pairCount == 2) return QStringLiteral(" · 한 점 더 찍으면 기울기도 맞습니다");
  if (r.pairCount == 3) return QStringLiteral(" · 한 점 더 찍으면 어긋남을 잽니다");
  if (!r.measurable) return {};
  return QStringLiteral(" · 평균 어긋남 %1 m · 최대 %2 m")
      .arg(r.rms, 0, 'f', 2)
      .arg(r.maxResidual, 0, 'f', 2);
}

}  // namespace GeorefQuality
