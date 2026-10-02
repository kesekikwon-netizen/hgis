#include "SurveyFileHygiene.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <algorithm>

namespace {

QString normalized(const QString& path) {
  if (path.isEmpty()) return {};
  return QDir::cleanPath(QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath()));
}

// Returns the user-facing kind for an app staging entry, or an empty string when the entry is not
// one of ours. Only exact app prefixes/suffixes qualify; recovery copies never match.
QString kindOf(const QFileInfo& info) {
  const QString name = info.fileName();
  if (name.startsWith(QStringLiteral("조사복구_")) || name == QStringLiteral("복구사본")) return {};
  if (info.isDir()) {
    if (name.startsWith(QLatin1String(".ka-survey-gen-"))) return QStringLiteral("저장 중 끊긴 세대");
    if (name.startsWith(QLatin1String(".ka-survey-copy-"))) return QStringLiteral("저장 임시 사본");
    if (name.startsWith(QLatin1String(".ka-new-survey-"))) return QStringLiteral("새 조사 만들기 임시");
    if (name.startsWith(QLatin1String(".ka-hgis-export-"))) return QStringLiteral("제출 묶음 임시");
    if (name.startsWith(QLatin1String("ka-hgis-tilepack-"))) return QStringLiteral("타일 묶음 임시");
    return {};
  }
  if (!info.isFile()) return {};
  if (name.endsWith(QLatin1String(".ka-new"), Qt::CaseInsensitive)) return QStringLiteral("교체하지 못한 새 조사 파일");
  if (name.contains(QLatin1String(".ka-writing-"))) return QStringLiteral("작업공간 저장 임시");
  if (name.startsWith(QLatin1String(".ka-geotiff-")) && name.endsWith(QLatin1String(".tif"), Qt::CaseInsensitive))
    return QStringLiteral("GeoTIFF 내보내기 임시");
  return {};
}

bool isProtected(const QString& itemPath, bool isDir, const QStringList& protectPaths) {
  const QString item = normalized(itemPath);
  for (const QString& raw : protectPaths) {
    const QString protect = normalized(raw.section(QLatin1Char('|'), 0, 0));
    if (protect.isEmpty()) continue;
    if (protect.compare(item, Qt::CaseInsensitive) == 0) return true;
    if (isDir && protect.startsWith(item + QLatin1Char('/'), Qt::CaseInsensitive)) return true;
  }
  return false;
}

// Newest modification time and total size of an entry (recursive for folders).
void measure(const QFileInfo& info, QDateTime* newest, qint64* bytes) {
  *newest = info.lastModified();
  *bytes = info.isFile() ? info.size() : 0;
  if (!info.isDir()) return;
  QDirIterator it(info.absoluteFilePath(), QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    it.next();
    const QFileInfo entry = it.fileInfo();
    if (entry.lastModified() > *newest) *newest = entry.lastModified();
    if (entry.isFile()) *bytes += entry.size();
  }
}

bool qualifies(const QFileInfo& info, const QStringList& protectPaths, qint64 minAgeSecs,
               SurveyFileHygiene::StaleItem* out) {
  const QString kind = kindOf(info);
  if (kind.isEmpty()) return false;
  if (info.isDir() && info.fileName().startsWith(QLatin1String(".ka-survey-gen-")) &&
      SurveyFileHygiene::isPreservedGeneration(info.absoluteFilePath()))
    return false;
  if (isProtected(info.absoluteFilePath(), info.isDir(), protectPaths)) return false;
  QDateTime newest;
  qint64 bytes = 0;
  measure(info, &newest, &bytes);
  if (newest.isValid() && newest.secsTo(QDateTime::currentDateTime()) < minAgeSecs) return false;
  if (out) {
    out->path = normalized(info.absoluteFilePath());
    out->kind = kind;
    out->isDir = info.isDir();
    out->bytes = bytes;
    out->modified = newest;
  }
  return true;
}

}  // namespace

namespace SurveyFileHygiene {

QString preservedMarkerName() { return QStringLiteral("보존-저장실패.txt"); }

bool markPreservedGeneration(const QString& generationDir, const QString& reason) {
  if (generationDir.isEmpty() || !QFileInfo(generationDir).isDir()) return false;
  QSaveFile marker(QDir(generationDir).filePath(preservedMarkerName()));
  if (!marker.open(QIODevice::WriteOnly)) return false;
  const QByteArray text =
      (QStringLiteral("저장하지 못한 편집이 이 폴더의 survey.gpkg 에 남아 있어 지우지 않습니다.\n") +
       QDateTime::currentDateTime().toString(Qt::ISODate) + QLatin1Char('\n') + reason + QLatin1Char('\n'))
          .toUtf8();
  return marker.write(text) == text.size() && marker.commit();
}

bool isPreservedGeneration(const QString& generationDir) {
  return !generationDir.isEmpty() &&
         QFileInfo::exists(QDir(generationDir).filePath(preservedMarkerName()));
}

QList<StaleItem> staleStagingNear(const QString& surveyPath, const QStringList& protectPaths,
                                  qint64 minAgeSecs) {
  QList<StaleItem> items;
  if (surveyPath.isEmpty()) return items;
  const QDir dir = QFileInfo(surveyPath).absoluteDir();
  if (!dir.exists()) return items;
  const QFileInfoList entries =
      dir.entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
  for (const QFileInfo& entry : entries) {
    StaleItem item;
    if (qualifies(entry, protectPaths, minAgeSecs, &item)) items << item;
  }
  std::sort(items.begin(), items.end(),
            [](const StaleItem& a, const StaleItem& b) { return a.modified < b.modified; });
  return items;
}

int removeStaleStaging(const QList<StaleItem>& confirmed, const QStringList& protectPaths,
                       QStringList* failed, qint64 minAgeSecs) {
  int removed = 0;
  for (const StaleItem& item : confirmed) {
    const QFileInfo info(item.path);
    if (!info.exists()) continue;
    // Re-check at deletion time: the folder may have been marked or reused meanwhile.
    if (!qualifies(info, protectPaths, minAgeSecs, nullptr)) {
      if (failed) *failed << item.path;
      continue;
    }
    const bool ok = info.isDir() ? QDir(info.absoluteFilePath()).removeRecursively()
                                 : QFile::remove(info.absoluteFilePath());
    if (ok)
      ++removed;
    else if (failed)
      *failed << item.path;
  }
  return removed;
}

}  // namespace SurveyFileHygiene
