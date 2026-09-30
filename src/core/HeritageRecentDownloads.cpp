#include "HeritageRecentDownloads.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace {

QJsonObject readManifest(const QString& root) {
  QFile file(HeritageRecentDownloads::manifestPath(root));
  if (!file.open(QIODevice::ReadOnly)) return {};
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  return doc.isObject() ? doc.object() : QJsonObject();
}

bool usableFiles(const QJsonArray& list, QStringList* out) {
  QStringList files;
  for (const QJsonValue& v : list) {
    const QFileInfo info(v.toString());
    if (!info.isFile() || info.size() <= 0) return false;
    files.append(info.absoluteFilePath());
  }
  if (files.isEmpty()) return false;
  if (out) *out = files;
  return true;
}

bool fresh(const QDateTime& when, int maxAgeDays, const QDateTime& now) {
  if (!when.isValid() || maxAgeDays < 0) return false;
  return when <= now.addSecs(60) && when.daysTo(now) <= maxAgeDays;
}

}  // namespace

QString HeritageRecentDownloads::defaultRoot(const HeritageCity& city) {
  const QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  if (base.isEmpty() || !city.ok()) return {};
  return QDir(base).filePath(
      QStringLiteral("주변유적/%1 %2/원본").arg(city.sido.trimmed(), city.city.trimmed()));
}

QString HeritageRecentDownloads::manifestPath(const QString& root) {
  return root.isEmpty() ? QString() : QDir(root).filePath(QStringLiteral("recent.json"));
}

bool HeritageRecentDownloads::record(const QString& root, HeritageDataset dataset,
                                     const QStringList& files, const QDateTime& when) {
  if (root.isEmpty() || files.isEmpty() || !QDir().mkpath(root)) return false;
  QJsonArray list;
  for (const QString& f : files) list.append(QFileInfo(f).absoluteFilePath());
  QJsonObject entry;
  entry.insert(QStringLiteral("when"), when.toString(Qt::ISODate));
  entry.insert(QStringLiteral("files"), list);
  QJsonObject manifest = readManifest(root);
  manifest.insert(HeritageStyle::layerName(dataset), entry);
  QSaveFile out(manifestPath(root));
  if (!out.open(QIODevice::WriteOnly)) return false;
  out.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
  return out.commit();
}

QStringList HeritageRecentDownloads::find(const QString& root, HeritageDataset dataset,
                                          int maxAgeDays, const QDateTime& now) {
  const QJsonObject entry = readManifest(root).value(HeritageStyle::layerName(dataset)).toObject();
  const QDateTime when =
      QDateTime::fromString(entry.value(QStringLiteral("when")).toString(), Qt::ISODate);
  QStringList files;
  if (!fresh(when, maxAgeDays, now)) return {};
  if (!usableFiles(entry.value(QStringLiteral("files")).toArray(), &files)) return {};
  return files;
}

QDateTime HeritageRecentDownloads::latest(const QString& root, int maxAgeDays, const QDateTime& now) {
  QDateTime newest;
  const QJsonObject manifest = readManifest(root);
  for (HeritageDataset dataset : HeritageStyle::allDatasets()) {
    const QJsonObject entry = manifest.value(HeritageStyle::layerName(dataset)).toObject();
    const QDateTime when =
        QDateTime::fromString(entry.value(QStringLiteral("when")).toString(), Qt::ISODate);
    if (!fresh(when, maxAgeDays, now) ||
        !usableFiles(entry.value(QStringLiteral("files")).toArray(), nullptr))
      continue;
    if (!newest.isValid() || when > newest) newest = when;
  }
  return newest;
}
