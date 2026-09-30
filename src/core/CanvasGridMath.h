#pragma once

#include <algorithm>
#include <cmath>

// Adaptive map-grid intervals. Projected metres and geographic degrees.
namespace CanvasGridMath {

inline double niceStepMeters(double targetMeters) {
  if (!(targetMeters > 0.0) || !std::isfinite(targetMeters))
    return 10.0;
  const double mag = std::pow(10.0, std::floor(std::log10(targetMeters)));
  const double n = targetMeters / mag;
  if (n < 1.5)
    return mag;
  if (n < 3.5)
    return 2.0 * mag;
  if (n < 7.5)
    return 5.0 * mag;
  return 10.0 * mag;
}

// target in degrees. Returns a DMS-friendly step.
inline double niceStepDegrees(double targetDeg) {
  if (!(targetDeg > 0.0) || !std::isfinite(targetDeg))
    return 1.0 / 60.0;
  static const double kSteps[] = {
      0.1 / 3600.0, 1.0 / 3600.0, 10.0 / 3600.0, 30.0 / 3600.0,
      1.0 / 60.0,   10.0 / 60.0,  30.0 / 60.0,   1.0,
      2.0,          5.0,          10.0,          30.0, 45.0};
  for (double s : kSteps) {
    if (s >= targetDeg * 0.6)
      return s;
  }
  return 10.0;
}

// A projected grid: lines through origin, turned clockwise by rotationDeg from map +X (east).
// Clockwise is the sense of trench azimuths (TrenchGridGenerator), so the same angle lines up.
struct Frame {
  double originX = 0.0;
  double originY = 0.0;
  double rotationDeg = 0.0;
};

inline void frameAxes(const Frame& frame, double* c, double* s) {
  const double rad = frame.rotationDeg * 3.14159265358979323846 / 180.0;
  *c = std::cos(rad);
  *s = std::sin(rad);
}

// Map (x east, y north) to grid distances (u along the turned east axis, v along the turned north axis).
inline void toGrid(const Frame& frame, double x, double y, double* u, double* v) {
  double c = 1.0, s = 0.0;
  frameAxes(frame, &c, &s);
  const double dx = x - frame.originX;
  const double dy = y - frame.originY;
  *u = dx * c - dy * s;
  *v = dx * s + dy * c;
}

inline void toMap(const Frame& frame, double u, double v, double* x, double* y) {
  double c = 1.0, s = 0.0;
  frameAxes(frame, &c, &s);
  *x = frame.originX + u * c + v * s;
  *y = frame.originY - u * s + v * c;
}

// Moves a map point onto the nearest grid node.
inline void snapToNode(const Frame& frame, double step, double* x, double* y) {
  if (!(step > 1e-9) || !x || !y)
    return;
  double u = 0.0, v = 0.0;
  toGrid(frame, *x, *y, &u, &v);
  toMap(frame, std::round(u / step) * step, std::round(v / step) * step, x, y);
}

inline bool isTurned(const Frame& frame) {
  return std::abs(std::remainder(frame.rotationDeg, 360.0)) > 1e-9;
}

// Draw every n-th line so that no more than maxLines cover span; 1 draws them all.
inline int lineStride(double span, double step, int maxLines) {
  if (!(step > 0.0) || !(span >= 0.0) || maxLines < 1 || !std::isfinite(span / step))
    return 1;
  const double lines = std::floor(span / step) + 1.0;
  return lines <= maxLines ? 1 : static_cast<int>(std::ceil(lines / maxLines));
}

// Fewest decimals (0..3) that print every multiple of step, offset by origin, exactly.
inline int labelDecimals(double step, double origin = 0.0) {
  for (int decimals = 0; decimals < 3; ++decimals) {
    const double scale = std::pow(10.0, decimals);
    const bool stepExact = std::abs(step * scale - std::round(step * scale)) < 1e-4;
    const bool originExact = std::abs(origin * scale - std::round(origin * scale)) < 1e-4;
    if (stepExact && originExact)
      return decimals;
  }
  return 3;
}

}  // namespace CanvasGridMath
