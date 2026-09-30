#include "ReferenceStorage.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUrl>
#include <qgsmaplayer.h>
#include <qgsogrproviderutils.h>
#include <qgsproject.h>

namespace ReferenceStorage {
namespace {

const QRegularExpression& folderNamePattern() {
  // QTemporaryDir replaces the six X characters with ASCII letters and digits.
  static const QRegularExpression pattern(QStringLiteral("ka-hgis-reference-[A-Za-z0-9]{6}"));
  return pattern;
}

QString normalized(const QString& path) {
  return path.isEmpty() ? QString() : QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

void collectNames(const QString& text, QSet<QString>* names) {
  auto matches = folderNamePattern().globalMatch(text);
  while (matches.hasNext()) names->insert(matches.next().captured(0));
}

}  // namespace

bool isGeneratedFolder(const QString& pathOrName) {
  const QString name = QFileInfo(QDir::cleanPath(pathOrName)).fileName();
  const auto match = folderNamePattern().match(name);
  return match.hasMatch() && match.capturedStart() == 0 && match.capturedLength() == name.size();
}

QString surveyFolder(const QString& surveyPath) {
  if (surveyPath.isEmpty()) return {};
  const QFileInfo survey(surveyPath);
  return survey.isFile() ? QDir::cleanPath(survey.absolutePath()) : QString();
}

QSet<QString> referencedFolderNames(const QgsProject* project) {
  QSet<QString> names;
  if (!project) return names;
  const auto layers = project->mapLayers();
  for (const QgsMapLayer* layer : layers) {
    if (!layer) continue;
    const QString source = layer->source();
    collectNames(source, &names);
    collectNames(QUrl::fromPercentEncoding(source.toUtf8()), &names);
    // A relief/warp VRT names its raster inside the file, not in the layer source.
    const QFileInfo file(source.section(QLatin1Char('|'), 0, 0));
    if (file.suffix().compare(QLatin1String("vrt"), Qt::CaseInsensitive) == 0 && file.isFile() &&
        file.size() <= 1024 * 1024) {
      QFile vrt(file.absoluteFilePath());
      if (vrt.open(QIODevice::ReadOnly)) collectNames(QString::fromUtf8(vrt.readAll()), &names);
    }
  }
  return names;
}

bool removeGeneratedFolder(const QString& folder) {
  const QFileInfo info(folder);
  if (!isGeneratedFolder(info.fileName()) || !info.isDir() || info.isSymLink() || info.isJunction())
    return false;
  const QString path = info.absoluteFilePath();
  // Release OGR's cached handles first; Windows cannot delete an open GeoPackage.
  QDirIterator packages(path, {QStringLiteral("*.gpkg")}, QDir::Files | QDir::NoSymLinks,
                        QDirIterator::Subdirectories);
  while (packages.hasNext()) QgsOgrProviderUtils::invalidateCachedDatasets(packages.next());
  return QDir(path).removeRecursively();
}

void FolderLedger::adopt(const QString& folder, const QString& surveyPath) {
  const QString key = normalized(folder);
  if (key.isEmpty() || !isGeneratedFolder(key) || surveyPath.isEmpty()) return;
  m_folders.insert(key, Entry{normalized(surveyPath), false});
}

void FolderLedger::supersede(const QSet<QString>& namesBefore, const QSet<QString>& namesAfter) {
  for (auto it = m_folders.begin(); it != m_folders.end(); ++it) {
    const QString name = QFileInfo(it.key()).fileName();
    if (!it->retired && namesBefore.contains(name) && !namesAfter.contains(name)) it->retired = true;
  }
}

QStringList FolderLedger::removable(const QSet<QString>& referencedNames,
                                    const QString& surveyPath) const {
  QStringList out;
  const QString survey = normalized(surveyPath);
  if (survey.isEmpty()) return out;
  for (auto it = m_folders.cbegin(); it != m_folders.cend(); ++it) {
    if (!it->retired || it->surveyPath.compare(survey, Qt::CaseInsensitive) != 0) continue;
    if (referencedNames.contains(QFileInfo(it.key()).fileName())) continue;
    out.append(it.key());
  }
  out.sort();
  return out;
}

void FolderLedger::forget(const QString& folder) { m_folders.remove(normalized(folder)); }

QStringList FolderLedger::retired() const {
  QStringList out;
  for (auto it = m_folders.cbegin(); it != m_folders.cend(); ++it)
    if (it->retired) out.append(it.key());
  out.sort();
  return out;
}

FolderLedger& sessionLedger() {
  static FolderLedger ledger;
  return ledger;
}

}  // namespace ReferenceStorage
