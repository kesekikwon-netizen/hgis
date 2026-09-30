#pragma once

#include <QString>
#include <QStringList>
#include <QtGlobal>

class QSettings;

// Facts about a survey file for the home page: how many areas and features it holds, the
// last explicit check (검수) and whether a submit package was made from it.
//
// Every fact is a record in QSettings keyed by the file path and stamped with the file's
// size+mtime; a file that changed since the record was written reads back as unknown.
// Nothing here opens a project or a layer. Writers: rememberCounts after a successful save
// (MainWindow), rememberCheck from an explicit check with rules, markPackaged once the
// package exists. The read-side fallback probeCountsIfUnknown counts the GPKG tables with
// GDAL once for entries without a usable record; KA_HGIS_HOME_PROBE=0 turns it off.
namespace SurveyFacts {

struct Facts {
  bool known = false;   // a record exists and the file still matches its fingerprint
  int areas = -1;       // survey_area features; -1 = never counted
  int features = -1;    // feature_poly + feature_line; -1 = never counted
  int errors = -1;      // last explicit check; -1 = never checked
  int warnings = -1;
  qint64 checkedAtMs = 0;
  qint64 countedAtMs = 0;
  qint64 packagedAtMs = 0;
  bool hasCounts() const { return known && areas >= 0 && features >= 0; }
  bool hasCheck() const { return known && checkedAtMs > 0; }
};

constexpr int kMaxEntries = 24;  // records kept, newest first
constexpr int kMaxProbes = 12;   // GPKG files counted per home visit

// The record for path, or an all-unknown Facts when there is none or the file changed.
Facts lookup(QSettings& settings, const QString& path);
// Keeps the other fields of an existing record and refreshes the fingerprint.
void rememberCounts(QSettings& settings, const QString& path, int areas, int features);
void rememberCheck(QSettings& settings, const QString& path, int errors, int warnings);
void markPackaged(QSettings& settings, const QString& path);
void forget(QSettings& settings, const QString& path);
// rememberCheck / markPackaged against RecentSurveys::userSettings() (MainWindow callers).
void noteCheck(const QString& path, int errors, int warnings);
void notePackaged(const QString& path);
// Record keys, newest first (never more than kMaxEntries).
QStringList storedKeys(QSettings& settings);

// 「제출 준비: 오류 0 · 경고 2」, or the not-yet-checked sentence.
QString summaryLine(const Facts& facts);

// ---- read-side fallback ----
bool probeEnabled();  // KA_HGIS_HOME_PROBE is not "0"
enum class ProbeBlock { None, Disabled, EmptyPath, Remote, NotFixedDrive, OpenInApp, MissingOrEmpty };
// Why path is left alone (None when it may be counted): only local fixed drives, files this
// process has not opened, and non-empty files are read.
ProbeBlock probeBlockFor(const QString& path);
// Counts survey_area and feature_poly+feature_line with GDAL (read-only, GPKG driver only).
// False when the file is not a readable GeoPackage.
bool countLayers(const QString& path, int* areas, int* features);
// For each path without usable counts, counts and stores them; returns how many were stored.
int probeCountsIfUnknown(QSettings& settings, const QStringList& paths, int maxProbes = kMaxProbes);

}  // namespace SurveyFacts
