#pragma once

#include <QString>
#include <QtGlobal>

#include <cmath>

// Elevations are stored as centimetre integers so labels never show binary residue.
inline int surveyMetersToCm(double meters) {
  return qRound(meters * 100.0);
}

inline double surveyCmToMeters(int cm) { return static_cast<double>(cm) / 100.0; }

inline QString surveyFormatMeters(int cm, int decimals) {
  return QString::number(surveyCmToMeters(cm), 'f', decimals);
}

inline QString surveyBandLabel(int minCm, int maxCm) {
  return QStringLiteral("%1 – %2 m").arg(surveyFormatMeters(minCm, 1), surveyFormatMeters(maxCm, 1));
}

inline int surveySnapCm(double meters, int baseCm, int stepCm) {
  const int cm = surveyMetersToCm(meters);
  if (stepCm <= 0) return cm;
  const int steps = qRound(static_cast<double>(cm - baseCm) / static_cast<double>(stepCm));
  return baseCm + steps * stepCm;
}

inline int surveyClassCount(int minCm, int maxCm, int stepCm) {
  if (stepCm <= 0 || maxCm <= minCm) return 0;
  return (maxCm - minCm + stepCm - 1) / stepCm;
}
