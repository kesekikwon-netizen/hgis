#pragma once

#include "GeorefService.h"

#include <QString>
#include <QStringList>
#include <QVector>

// How trustworthy an alignment looks from its point pairs. Only informs the point list;
// the fit itself (2-point Helmert, 3+ point affine) and the reference-map contract stay.
namespace GeorefQuality {

// Map units of the work CRS (5186/5187 are metres).
constexpr double kResidualWarn = 1.0;
// Ratio between the two axis scales an affine applies (1.0 = same scale both ways).
constexpr double kAnisotropyWarn = 1.05;
// How far the two image axes end up from a right angle.
constexpr double kShearWarnDeg = 3.0;

struct Shape {
  bool valid = false;
  double scaleMajor = 0;
  double scaleMinor = 0;
  double anisotropy = 1.0;
  double shearDeg = 0;
};

struct Report {
  GeorefService::Affine affine;
  int pairCount = 0;
  // Residuals only mean something with more pairs than the fit needs (4+ for an affine).
  bool measurable = false;
  QVector<double> residuals;  // per pair, map units; empty unless measurable
  double rms = 0;
  double maxResidual = 0;
  int maxIndex = -1;
  Shape shape;
};

Shape shapeOf(const GeorefService::Affine& a);
Report assess(const QVector<GeorefService::Pair>& pairs, bool sourceYDown);

// "1번  왼쪽 → 오른쪽 · 어긋남 0.42 m" for the point list.
QString rowText(const Report& r, int index);
// Hints and warnings shown under the points (empty when nothing to say).
QStringList notes(const Report& r);
// Short summary for the status line, e.g. " · 평균 어긋남 0.31 m · 최대 0.52 m".
QString summary(const Report& r);

}  // namespace GeorefQuality
