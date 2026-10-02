#pragma once

#include "TopographicCatalog.h"

#include <QJsonObject>
#include <QList>
#include <QString>

// Verification cache for received topographic sheets. When a sheet was prepared
// once (ZIP hashed, CRS resolved, SHP converted), the survey index remembers the
// resulting SHP records together with the size and modification time of the
// original and of every SHP file, plus the CRS solver version. A later pan can
// reuse those records without hashing and converting again; any change to a
// file or to the solver falls back to the full, original verification.
namespace ReferenceSheetSnapshot {

QJsonObject capture(const QString& source, const QList<TopographicCatalog::Record>& records,
                    int solverVersion, const QString& crsName, const QString& evidence);

struct Restored {
  bool valid = false;
  QList<TopographicCatalog::Record> records;
  QString crsName;
  QString evidence;
};

// Valid only when the solver version, the original and every SHP dataset
// (shp/shx/dbf/prj) still have exactly the recorded size and time.
Restored restore(const QJsonObject& snapshot, const QString& source, int solverVersion);

// Sidecar files beside a survey index entry: index/<id>.json -> index/<id>.snapshot
// (not *.json, so index scans and their size limit never see it).
QString sidecarPath(const QString& indexFile);
// Writes the sidecar for a clean preparation (no warnings or review items);
// otherwise removes a stale one so the full verification runs next time.
void rememberBeside(const QString& indexFile, const QString& source,
                    const QList<TopographicCatalog::Record>& records, bool clean, int solverVersion,
                    const QString& crsName, const QString& evidence);
// Reads and validates the sidecar; every record must belong to `sheet`.
Restored restoreBeside(const QString& indexFile, const QString& source, const QString& sheet,
                       int solverVersion);

}  // namespace ReferenceSheetSnapshot
