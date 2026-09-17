#pragma once

#include <QString>

class QgsProject;

// QgsProject::read can ACCESS_VIOLATE on a broken .qgz (addMapLayers).
// Returns false on failure or SEH; never lets the process die.
// crashedOut is set when the SEH handler fired. That case is NOT recoverable in
// process: __except skips the C++ destructors of every QgsScopedRuntimeProfile
// left on the stack, so QgsRuntimeProfiler keeps dangling parent pointers and the
// next QgsVectorLayer ctor dies inside QgsRuntimeProfiler::start. Callers must
// stop touching QGIS and ask for a restart instead of carrying on.
// Set loadLayouts only for an explicit GUI-thread workspace open. Validation
// and recovery reads keep the default lightweight path.
bool kaSafeReadQgisProject(QgsProject* project, const QString& path,
                           bool* crashedOut = nullptr, bool loadLayouts = false);

// One generation back: 안동시.qgz -> 안동시.bak.qgz. The suffix has to stay last —
// QgsZipUtils::isZipFile only accepts ".qgz", so a name like "안동시.qgz.bak" is
// written and read as plain XML and the project is silently unreadable.
QString kaProjectBackupPath(const QString& path);

// Writes the project so a half-written file can never be left behind: writes a
// temp file NEXT TO the target (same folder — QgsPathResolver resolves relative
// datasources against the project file's directory, and rename must stay on one
// volume), verifies it reads back, keeps one generation via kaProjectBackupPath,
// then atomically replaces the target with QSaveFile (no direct-write fallback).
// Failure restores the project's original filename and dirty state.
bool kaWriteQgisProjectAtomic(QgsProject* project, const QString& path, QString* errorOut = nullptr);

// QgsProject::clear after a half-finished read can ACCESS_VIOLATE in
// QgsRelationManager::layersRemoved. Returns false on SEH.
bool kaSafeClearQgisProject(QgsProject* project);

// Count <layername> / layer-tree names that look like satellite basemaps.
// .qgz is unzipped without QgsProject::read. Returns -1 if the file exists
// but cannot be inspected.
int kaCountSatelliteLayersInQgisProjectFile(const QString& path);

// True when the companion project would recreate duplicate VWorld/Google
// satellite XYZ layers (the crash that started after "위성 중복 생성").
bool kaQgisProjectFileHasDuplicateSatellites(const QString& path);

// Mark after QgsProject::read SEH/failure so the same .qgz is not opened again
// (half-loaded project + second read → RelationManager AV). Persisted to
// %LOCALAPPDATA%/ka-hgis/unsafe-projects.txt: an in-memory-only mark meant the
// AV was retriggered on every launch, so the app died every single time.
void kaMarkQgisProjectUnsafeToRead(const QString& path);
bool kaQgisProjectFileIsUnsafeToRead(const QString& path);
// 사용자가 고쳤거나 다시 저장했을 때 표시를 지운다.
void kaClearQgisProjectUnsafeMark(const QString& path);
