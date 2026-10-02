#include "RecentSurveys.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QSettings>
#include <QStringList>

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

QString listKey() { return QStringLiteral("RecentSurveys/items"); }
QString skipRestoreKey() { return QStringLiteral("RecentSurveys/skipAutoRestore"); }

constexpr Qt::CaseSensitivity kPathCase =
#ifdef Q_OS_WIN
    Qt::CaseInsensitive;
#else
    Qt::CaseSensitive;
#endif

QString normalizedPath(const QString& path) {
  const QString trimmed = path.trimmed();
  if (trimmed.isEmpty()) return {};
  return QDir::cleanPath(QDir::fromNativeSeparators(QFileInfo(trimmed).absoluteFilePath()));
}

// Tabs and line breaks would shift the tab-separated fields of the stored line.
QString storableName(QString name) {
  for (QChar& c : name)
    if (c == QLatin1Char('\t') || c == QLatin1Char('\r') || c == QLatin1Char('\n')) c = QLatin1Char(' ');
  return name.simplified();
}

bool hasDriveLetter(const QString& clean) {
  return clean.size() >= 3 && clean.at(1) == QLatin1Char(':') && clean.at(2) == QLatin1Char('/');
}

// Same path apart from the drive letter (a USB stick that came back as another letter).
bool sameRestOnOtherDrive(const QString& a, const QString& b) {
  const QString x = normalizedPath(a);
  const QString y = normalizedPath(b);
  if (!hasDriveLetter(x) || !hasDriveLetter(y)) return false;
  if (x.at(0).toUpper() == y.at(0).toUpper()) return false;
  return x.mid(2).compare(y.mid(2), kPathCase) == 0;
}

// ---- availability: local paths are checked directly; network paths are probed once per TTL so
// an unreachable server does not stall the home screen for every entry.
struct Probe {
  bool ok = false;
  qint64 atMs = 0;
};
QMutex g_probeMutex;
QHash<QString, Probe> g_probes;
constexpr qint64 kRemoteProbeTtlMs = 30 * 1000;

bool cachedExists(const QString& path) {
  const QString key = path.toCaseFolded();
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  {
    QMutexLocker lock(&g_probeMutex);
    const auto it = g_probes.constFind(key);
    if (it != g_probes.cend() && now - it->atMs < kRemoteProbeTtlMs) return it->ok;
  }
  const bool ok = QFileInfo::exists(path);
  QMutexLocker lock(&g_probeMutex);
  g_probes.insert(key, {ok, now});
  return ok;
}

// "//server/share" for UNC paths, "Z:/" for drive paths.
QString remoteRoot(const QString& clean) {
  if (hasDriveLetter(clean)) return clean.left(3);
  const QStringList parts = clean.mid(2).split(QLatin1Char('/'), Qt::SkipEmptyParts);
  if (parts.size() < 2) return clean;
  return QStringLiteral("//%1/%2").arg(parts.at(0), parts.at(1));
}

// A survey that was just opened is reachable: do not keep showing a stale "not found" probe.
void noteReachable(const QString& path) {
  const QString clean = normalizedPath(path);
  if (clean.isEmpty()) return;
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  QMutexLocker lock(&g_probeMutex);
  g_probes.insert(clean.toCaseFolded(), {true, now});
  g_probes.insert(remoteRoot(clean).toCaseFolded(), {true, now});
}

#ifdef Q_OS_WIN
UINT driveType(QChar letter) {
  const wchar_t root[] = {static_cast<wchar_t>(letter.toUpper().unicode()), L':', L'\\', 0};
  return GetDriveTypeW(root);
}

bool driveLetterPresent(QChar letter) {
  const int index = letter.toUpper().unicode() - 'A';
  return index >= 0 && index < 26 && (GetLogicalDrives() & (1u << index)) != 0;
}
#endif

bool pathAvailable(const QString& path) {
  const QString clean = normalizedPath(path);
  if (clean.isEmpty()) return false;
#ifdef Q_OS_WIN
  if (hasDriveLetter(clean) && !driveLetterPresent(clean.at(0))) return false;  // unplugged USB: no file-system call
#endif
  if (!RecentSurveys::isRemotePath(clean)) return QFileInfo::exists(clean);
  if (!cachedExists(remoteRoot(clean))) return false;
  return cachedExists(clean);
}

// Drives worth searching for a moved survey. Network and optical drives are skipped: a dead
// mapped drive can block for many seconds, and an empty optical drive can prompt for a disc.
QStringList localDriveRoots() {
  QStringList roots;
  for (const QFileInfo& drive : QDir::drives()) {
    const QString root = drive.absoluteFilePath();
#ifdef Q_OS_WIN
    if (root.size() >= 2 && root.at(1) == QLatin1Char(':')) {
      const UINT type = driveType(root.at(0));
      if (type == DRIVE_REMOTE || type == DRIVE_CDROM || type == DRIVE_NO_ROOT_DIR ||
          type == DRIVE_UNKNOWN)
        continue;
    }
#endif
    roots << root;
  }
  return roots;
}

// The stored list exactly as saved (deduplicated), without touching the file system.
QVector<RecentSurveys::Item> loadStored(QSettings& settings) {
  QVector<RecentSurveys::Item> out;
  const QStringList raw = settings.value(listKey()).toStringList();
  for (const QString& line : raw) {
    const QStringList parts = line.split(QLatin1Char('\t'));
    if (parts.size() < 2) continue;
    RecentSurveys::Item it;
    it.path = parts.at(0).trimmed();
    if (it.path.isEmpty()) continue;
    bool numeric = false;
    const qint64 ms = parts.size() >= 3 ? parts.last().toLongLong(&numeric) : 0;
    if (numeric) {
      it.lastOpenedMs = ms;
      it.name = storableName(parts.mid(1, parts.size() - 2).join(QLatin1Char(' ')));
    } else {
      it.name = storableName(parts.mid(1).join(QLatin1Char(' ')));
    }
    bool duplicate = false;
    for (const RecentSurveys::Item& seen : out)
      duplicate = duplicate || RecentSurveys::samePath(seen.path, it.path);
    if (duplicate) continue;
    out.push_back(it);
    if (out.size() >= RecentSurveys::kMaxItems) break;
  }
  return out;
}

void store(QSettings& settings, const QVector<RecentSurveys::Item>& items) {
  QStringList raw;
  for (const RecentSurveys::Item& it : items)
    raw << (it.path + QLatin1Char('\t') + storableName(it.name) + QLatin1Char('\t') +
            QString::number(it.lastOpenedMs));
  settings.setValue(listKey(), raw);
  settings.sync();
}

}  // namespace

QSettings RecentSurveys::userSettings() {
  return QSettings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("ka-hgis"),
                   QStringLiteral("ka-hgis"));
}

bool RecentSurveys::samePath(const QString& a, const QString& b) {
  const QString x = normalizedPath(a);
  return !x.isEmpty() && x.compare(normalizedPath(b), kPathCase) == 0;
}

bool RecentSurveys::isRemotePath(const QString& path) {
  const QString clean = normalizedPath(path);
  if (clean.isEmpty()) return false;
  if (clean.startsWith(QLatin1String("//"))) return true;
#ifdef Q_OS_WIN
  if (hasDriveLetter(clean)) return driveLetterPresent(clean.at(0)) && driveType(clean.at(0)) == DRIVE_REMOTE;
#endif
  return false;
}

QVector<RecentSurveys::Item> RecentSurveys::loadAll(QSettings& settings) {
  QVector<Item> stored = loadStored(settings);
  QVector<Item> out;
  QStringList drives;
  bool drivesReady = false;
  for (Item it : stored) {
    it.available = pathAvailable(it.path);
    if (!it.available) {
      // 포터블을 USB 로 들고 다니면 드라이브 글자가 바뀌어 예전 조사가 목록에서 사라졌다.
      if (!drivesReady) {
        drives = localDriveRoots();
        drivesReady = true;
      }
      const QString moved = onAnotherDrive(it.path, drives);
      if (!moved.isEmpty()) {
        it.path = moved;
        it.available = true;
      }
    }
    if (it.name.isEmpty()) it.name = QFileInfo(it.path).completeBaseName();
    bool duplicate = false;
    for (const Item& seen : out) duplicate = duplicate || samePath(seen.path, it.path);
    if (!duplicate) out.push_back(it);
  }
  return out;
}

QVector<RecentSurveys::Item> RecentSurveys::load(QSettings& settings) {
  QVector<Item> out;
  for (const Item& it : loadAll(settings))
    if (it.available) out.push_back(it);
  return out;
}

QString RecentSurveys::onAnotherDrive(const QString& path, const QStringList& driveRoots) {
  const QString clean = QDir::fromNativeSeparators(path);
  if (clean.size() < 4 || clean.at(1) != QLatin1Char(':') || clean.at(2) != QLatin1Char('/')) return {};
  const QString rest = clean.mid(2);
  for (const QString& root : driveRoots) {
    const QString drive = QDir::fromNativeSeparators(root);
    if (drive.size() < 2 || drive.at(1) != QLatin1Char(':')) continue;
    if (drive.at(0).toUpper() == clean.at(0).toUpper()) continue;
    const QString candidate = drive.left(2) + rest;
    if (QFileInfo::exists(candidate)) return candidate;
  }
  return {};
}

QString RecentSurveys::lastPath(QSettings& settings) {
  const QVector<Item> items = load(settings);
  return items.isEmpty() ? QString() : items.first().path;
}

void RecentSurveys::remember(QSettings& settings, const QString& path, const QString& name) {
  const QString abs = QFileInfo(path).absoluteFilePath();
  if (abs.isEmpty() || !QFileInfo::exists(abs))
    return;
  noteReachable(abs);
  const QString cleanName = storableName(name);
  const QString shown = cleanName.isEmpty() ? QFileInfo(abs).completeBaseName() : cleanName;
  QVector<Item> next;
  next.push_back({shown, abs, QDateTime::currentMSecsSinceEpoch(), true});
  // Offline entries stay: an unplugged USB or network drive must not erase them.
  for (const Item& it : loadStored(settings)) {
    if (samePath(it.path, abs)) continue;
    // The same survey seen earlier under another drive letter (USB letter changed).
    if (sameRestOnOtherDrive(it.path, abs) && !pathAvailable(it.path)) continue;
    next.push_back(it);
    if (next.size() >= kMaxItems) break;
  }
  store(settings, next);
}

void RecentSurveys::forget(QSettings& settings, const QString& path) {
  QVector<Item> next;
  for (const Item& it : loadStored(settings)) {
    if (samePath(it.path, path)) continue;
    // The home list shows a moved USB survey under its new letter; forget the stored old one too.
    if (sameRestOnOtherDrive(it.path, path) && !pathAvailable(it.path)) continue;
    next.push_back(it);
  }
  store(settings, next);
}

void RecentSurveys::setSkipAutoRestore(QSettings& settings, bool skip) {
  settings.setValue(skipRestoreKey(), skip);
  settings.sync();
}

bool RecentSurveys::takeSkipAutoRestore(QSettings& settings) {
  const bool skip = settings.value(skipRestoreKey(), false).toBool();
  if (skip) {
    settings.setValue(skipRestoreKey(), false);
    settings.sync();
  }
  return skip;
}
