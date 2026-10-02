#pragma once

// Per-CRS cache of the Korea rectangles (evaluation F205).
// LayerOps::clampCanvasToKorea runs on every extentsChanged (wheel zoom, pan) and each
// answer used to build three CRS objects and two transforms, although the result depends
// only on the destination CRS. Answers are kept per auth id for the life of the process,
// so nothing has to be invalidated when the canvas CRS changes. USER: CRSs can be
// redefined and are not kept; neither is an empty/failed answer (PROJ may not be ready).
// Header-only; used by BasemapOps.cpp.

#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QString>

#include <qgscoordinatereferencesystem.h>
#include <qgsrectangle.h>

namespace BasemapExtentCache {

enum class Table { Korea = 0, SatelliteFill = 1 };

struct Store {
  QMutex mutex;
  QHash<QString, QgsRectangle> tables[2];
};

inline Store& store() {
  static Store instance;
  return instance;
}

inline bool cacheable(const QString& authId) {
  return !authId.isEmpty() && !authId.startsWith(QLatin1String("USER:"), Qt::CaseInsensitive);
}

// Cached answer for `authId`, or compute() (kept when it is a usable rectangle).
template <typename Compute>
QgsRectangle get(Table table, const QString& authId, Compute compute) {
  const QString key = authId.trimmed();
  const bool keep = cacheable(key);
  QHash<QString, QgsRectangle>& entries = store().tables[static_cast<int>(table)];
  if (keep) {
    const QMutexLocker lock(&store().mutex);
    const auto hit = entries.constFind(key);
    if (hit != entries.constEnd()) return hit.value();
  }
  const QgsRectangle computed = compute();
  if (keep && !computed.isEmpty() && computed.isFinite() &&
      QgsCoordinateReferenceSystem(key).isValid()) {
    const QMutexLocker lock(&store().mutex);
    entries.insert(key, computed);
  }
  return computed;
}

}  // namespace BasemapExtentCache
