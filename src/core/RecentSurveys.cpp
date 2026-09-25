#include "RecentSurveys.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStringList>

static QString listKey() { return QStringLiteral("RecentSurveys/items"); }
static QString skipRestoreKey() { return QStringLiteral("RecentSurveys/skipAutoRestore"); }

QSettings RecentSurveys::userSettings() {
  return QSettings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("ka-hgis"),
                   QStringLiteral("ka-hgis"));
}

QVector<RecentSurveys::Item> RecentSurveys::load(QSettings& settings) {
  QVector<Item> out;
  const QStringList raw = settings.value(listKey()).toStringList();
  for (const QString& line : raw) {
    const QStringList parts = line.split(QLatin1Char('\t'));
    if (parts.size() < 2)
      continue;
    Item it;
    it.path = parts.at(0).trimmed();
    it.name = parts.at(1).trimmed();
    if (parts.size() >= 3)
      it.lastOpenedMs = parts.at(2).toLongLong();
    if (!it.path.isEmpty() && !QFileInfo::exists(it.path)) {
      // 포터블을 USB 로 들고 다니면 드라이브 글자가 바뀌어 예전 조사가 목록에서 사라졌다.
      QStringList drives;
      for (const QFileInfo& drive : QDir::drives()) drives << drive.absoluteFilePath();
      const QString moved = onAnotherDrive(it.path, drives);
      if (!moved.isEmpty()) it.path = moved;
    }
    if (it.path.isEmpty() || !QFileInfo::exists(it.path))
      continue;
    if (it.name.isEmpty())
      it.name = QFileInfo(it.path).completeBaseName();
    out.push_back(it);
    if (out.size() >= kMaxItems)
      break;
  }
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
  const QString shown = name.trimmed().isEmpty() ? QFileInfo(abs).completeBaseName() : name.trimmed();
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  QVector<Item> cur = load(settings);
  QVector<Item> next;
  next.push_back({shown, abs, now});
  for (const Item& it : cur) {
    if (QFileInfo(it.path).absoluteFilePath() == abs)
      continue;
    next.push_back(it);
    if (next.size() >= kMaxItems)
      break;
  }
  QStringList raw;
  for (const Item& it : next)
    raw << (it.path + QLatin1Char('\t') + it.name + QLatin1Char('\t') + QString::number(it.lastOpenedMs));
  settings.setValue(listKey(), raw);
  settings.sync();
}

void RecentSurveys::forget(QSettings& settings, const QString& path) {
  const QString abs = QFileInfo(path).absoluteFilePath();
  QVector<Item> cur = load(settings);
  QStringList raw;
  for (const Item& it : cur) {
    if (QFileInfo(it.path).absoluteFilePath() == abs)
      continue;
    raw << (it.path + QLatin1Char('\t') + it.name + QLatin1Char('\t') + QString::number(it.lastOpenedMs));
  }
  settings.setValue(listKey(), raw);
  settings.sync();
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
