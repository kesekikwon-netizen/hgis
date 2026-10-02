#include "SurveyFacts.h"

#include "RecentSurveys.h"
#include "SurveyFileFingerprint.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

#include <cpl_error.h>
#include <gdal.h>
#include <ogr_api.h>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

QString orderKey() { return QStringLiteral("RecentSurveys/factsOrder"); }
QString entryPrefix() { return QStringLiteral("RecentSurveys/facts/"); }

QString cleanPath(const QString& path) {
  const QString trimmed = path.trimmed();
  if (trimmed.isEmpty()) return {};
  return QDir::cleanPath(QDir::fromNativeSeparators(QFileInfo(trimmed).absoluteFilePath()));
}

// Windows paths that differ only in case or separators are the same survey.
QString keyFor(const QString& path) {
  QString clean = cleanPath(path);
  if (clean.isEmpty()) return {};
#ifdef Q_OS_WIN
  clean = clean.toCaseFolded();
#endif
  return QString::fromLatin1(
      QCryptographicHash::hash(clean.toUtf8(), QCryptographicHash::Sha1).toHex().left(16));
}

struct Stored {
  bool present = false;
  qint64 size = -1;
  qint64 modifiedMs = 0;
  SurveyFacts::Facts facts;
};

// size \t mtime \t areas \t features \t errors \t warnings \t checkedAt \t countedAt \t packagedAt
Stored read(QSettings& settings, const QString& key) {
  Stored out;
  if (key.isEmpty()) return out;
  const QStringList f = settings.value(entryPrefix() + key).toString().split(QLatin1Char('\t'));
  if (f.size() < 9) return out;
  out.present = true;
  out.size = f.at(0).toLongLong();
  out.modifiedMs = f.at(1).toLongLong();
  out.facts.areas = f.at(2).toInt();
  out.facts.features = f.at(3).toInt();
  out.facts.errors = f.at(4).toInt();
  out.facts.warnings = f.at(5).toInt();
  out.facts.checkedAtMs = f.at(6).toLongLong();
  out.facts.countedAtMs = f.at(7).toLongLong();
  out.facts.packagedAtMs = f.at(8).toLongLong();
  return out;
}

void write(QSettings& settings, const QString& key, const QString& path, const SurveyFacts::Facts& facts) {
  if (key.isEmpty()) return;
  const SurveyFileFingerprint::Fingerprint now = SurveyFileFingerprint::capture(path);
  const QStringList f{QString::number(now.exists ? now.size : -1), QString::number(now.exists ? now.modifiedMs : 0),
                      QString::number(facts.areas), QString::number(facts.features), QString::number(facts.errors),
                      QString::number(facts.warnings), QString::number(facts.checkedAtMs),
                      QString::number(facts.countedAtMs), QString::number(facts.packagedAtMs)};
  settings.setValue(entryPrefix() + key, f.join(QLatin1Char('\t')));
  QStringList order = settings.value(orderKey()).toStringList();
  order.removeAll(key);
  order.prepend(key);
  while (order.size() > SurveyFacts::kMaxEntries) settings.remove(entryPrefix() + order.takeLast());
  settings.setValue(orderKey(), order);
  settings.sync();
}

bool stillMatches(const Stored& stored, const QString& path) {
  const SurveyFileFingerprint::Fingerprint now = SurveyFileFingerprint::capture(path);
  return now.exists && now.size == stored.size && now.modifiedMs == stored.modifiedMs;
}

qint64 nowMs() { return QDateTime::currentMSecsSinceEpoch(); }

}  // namespace

namespace SurveyFacts {

Facts lookup(QSettings& settings, const QString& path) {
  const Stored stored = read(settings, keyFor(path));
  if (!stored.present || !stillMatches(stored, path)) return {};
  Facts facts = stored.facts;
  facts.known = true;
  return facts;
}

void rememberCounts(QSettings& settings, const QString& path, int areas, int features) {
  const QString key = keyFor(path);
  // Our own save just changed the file: the check and package fields stay.
  Facts facts = read(settings, key).facts;
  facts.areas = qMax(0, areas);
  facts.features = qMax(0, features);
  facts.countedAtMs = nowMs();
  write(settings, key, path, facts);
}

void rememberCheck(QSettings& settings, const QString& path, int errors, int warnings) {
  const QString key = keyFor(path);
  Facts facts = read(settings, key).facts;
  facts.errors = qMax(0, errors);
  facts.warnings = qMax(0, warnings);
  facts.checkedAtMs = nowMs();
  write(settings, key, path, facts);
}

void markPackaged(QSettings& settings, const QString& path) {
  const QString key = keyFor(path);
  Facts facts = read(settings, key).facts;
  facts.packagedAtMs = nowMs();
  write(settings, key, path, facts);
}

void forget(QSettings& settings, const QString& path) {
  const QString key = keyFor(path);
  if (key.isEmpty()) return;
  settings.remove(entryPrefix() + key);
  QStringList order = settings.value(orderKey()).toStringList();
  order.removeAll(key);
  settings.setValue(orderKey(), order);
  settings.sync();
}

void noteCheck(const QString& path, int errors, int warnings) {
  if (path.isEmpty()) return;
  QSettings settings = RecentSurveys::userSettings();
  rememberCheck(settings, path, errors, warnings);
}

void notePackaged(const QString& path) {
  if (path.isEmpty()) return;
  QSettings settings = RecentSurveys::userSettings();
  markPackaged(settings, path);
}

QStringList storedKeys(QSettings& settings) { return settings.value(orderKey()).toStringList(); }

QString summaryLine(const Facts& facts) {
  if (!facts.hasCheck())
    return QStringLiteral("아직 검수하지 않았습니다 — 지도 탭 「검수·제출」에서 확인합니다.");
  return QStringLiteral("제출 준비: 오류 %1 · 경고 %2").arg(qMax(0, facts.errors)).arg(qMax(0, facts.warnings));
}

bool probeEnabled() { return qEnvironmentVariable("KA_HGIS_HOME_PROBE") != QLatin1String("0"); }

ProbeBlock probeBlockFor(const QString& path) {
  if (!probeEnabled()) return ProbeBlock::Disabled;
  const QString clean = cleanPath(path);
  if (clean.isEmpty()) return ProbeBlock::EmptyPath;
  if (clean.startsWith(QLatin1String("//"))) return ProbeBlock::Remote;  // UNC: never touched
#ifdef Q_OS_WIN
  if (clean.size() < 2 || clean.at(1) != QLatin1Char(':')) return ProbeBlock::NotFixedDrive;
  const wchar_t root[] = {static_cast<wchar_t>(clean.at(0).toUpper().unicode()), L':', L'\\', 0};
  const UINT type = GetDriveTypeW(root);
  if (type == DRIVE_REMOTE) return ProbeBlock::Remote;
  if (type != DRIVE_FIXED) return ProbeBlock::NotFixedDrive;  // USB, optical, unknown
#endif
  if (SurveyFileFingerprint::isRemembered(clean)) return ProbeBlock::OpenInApp;
  const QFileInfo info(clean);
  if (!info.isFile() || info.size() <= 0) return ProbeBlock::MissingOrEmpty;
  return ProbeBlock::None;
}

bool countLayers(const QString& path, int* areas, int* features) {
  static bool registered = false;
  if (!registered) {
    GDALAllRegister();  // idempotent; QgsApplication normally did this already
    registered = true;
  }
  const char* drivers[] = {"GPKG", nullptr};
  CPLPushErrorHandler(CPLQuietErrorHandler);  // a non-GPKG file is simply unknown, not an error
  GDALDatasetH dataset = GDALOpenEx(path.toUtf8().constData(), GDAL_OF_VECTOR | GDAL_OF_READONLY, drivers,
                                    nullptr, nullptr);
  CPLPopErrorHandler();
  if (!dataset) return false;
  const auto count = [dataset](const char* table) {
    OGRLayerH layer = GDALDatasetGetLayerByName(dataset, table);
    return layer ? static_cast<int>(OGR_L_GetFeatureCount(layer, TRUE)) : 0;
  };
  // Table names are the layer ids of data/schemas/ka_hgis_layers.yaml.
  if (areas) *areas = count("survey_area");
  if (features) *features = count("feature_poly") + count("feature_line");
  GDALClose(dataset);
  return true;
}

int probeCountsIfUnknown(QSettings& settings, const QStringList& paths, int maxProbes) {
  if (!probeEnabled()) return 0;
  int stored = 0;
  int tried = 0;
  for (const QString& path : paths) {
    if (tried >= maxProbes) break;
    Facts facts = lookup(settings, path);  // a valid record keeps its check and package fields
    if (facts.hasCounts()) continue;
    if (probeBlockFor(path) != ProbeBlock::None) continue;
    ++tried;
    int areas = 0;
    int features = 0;
    if (!countLayers(path, &areas, &features)) continue;
    facts.areas = areas;
    facts.features = features;
    facts.countedAtMs = nowMs();
    write(settings, keyFor(path), path, facts);
    ++stored;
  }
  return stored;
}

}  // namespace SurveyFacts
