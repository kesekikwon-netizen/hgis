#pragma once

#include <QString>

// Shared by LayerOps.cpp and BasemapOps.cpp. Not a public API.
inline QString kaStripLegendCrsSuffix(QString name) {
  int bracket = name.lastIndexOf(QStringLiteral(" [EPSG:"));
  if (bracket < 0) bracket = name.lastIndexOf(QStringLiteral(" [CRS"));
  if (bracket >= 0) name = name.left(bracket).trimmed();
  return name;
}

inline QString kaFriendlyLegendName(const QString& name) {
  const QString base = kaStripLegendCrsSuffix(name);
  if (base.contains(QStringLiteral("지적"))) {
    const bool bon = base.contains(QStringLiteral("본번"));
    const bool bu = base.contains(QStringLiteral("부번"));
    if (bon && !bu) return QStringLiteral("지적 본번");
    if (bu && !bon) return QStringLiteral("지적 부번");
    return QStringLiteral("지적");
  }
  if (base.contains(QStringLiteral("위성")))
    return QStringLiteral("위성");
  return base;
}

inline bool legendTitlesMatch(const QString& a, const QString& b) {
  if (a == b) return true;
  if (a.startsWith(b + QLatin1String(" [")) || b.startsWith(a + QLatin1String(" [")))
    return true;
  const QString soil = QStringLiteral("토양도(흙토람)");
  if (a.startsWith(soil) && b.startsWith(soil))
    return true;
  return kaFriendlyLegendName(a) == kaFriendlyLegendName(b);
}
