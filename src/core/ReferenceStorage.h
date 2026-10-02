#pragma once

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

class QgsProject;

// Housekeeping for the "ka-hgis-reference-XXXXXX" folders that downloads create
// next to the survey file. Only folders created by a download in this session and
// later replaced by a newer download of the same survey are ever removed, and only
// at a save point, when the saved survey equals the current project. Originals,
// survey data and folders from earlier sessions are never touched.
namespace ReferenceStorage {

// True for a folder (path or bare name) created by ReferenceMapPreparation::initializeStorage.
bool isGeneratedFolder(const QString& pathOrName);

// Survey folder used for downloads; empty when no saved survey file is open.
QString surveyFolder(const QString& surveyPath);

// Names of generated folders mentioned by any layer source (raw or percent-encoded,
// including local WMS capabilities and small VRT files that point at a raster).
QSet<QString> referencedFolderNames(const QgsProject* project);

// Deletes one generated folder. Refuses links, junctions and non-generated names.
bool removeGeneratedFolder(const QString& folder);

class FolderLedger {
public:
  // A download registered successfully and its files now live in `folder`.
  void adopt(const QString& folder, const QString& surveyPath);
  // Folders referenced before/after a registration: an adopted folder that is no
  // longer referenced was replaced by the new download.
  void supersede(const QSet<QString>& namesBefore, const QSet<QString>& namesAfter);
  // At a save point: replaced folders of this survey that nothing references now.
  QStringList removable(const QSet<QString>& referencedNames, const QString& surveyPath) const;
  void forget(const QString& folder);
  QStringList retired() const;

private:
  struct Entry {
    QString surveyPath;
    bool retired = false;
  };
  QHash<QString, Entry> m_folders;  // absolute folder path -> origin
};

// The application-wide ledger (one main window per process).
FolderLedger& sessionLedger();

}  // namespace ReferenceStorage
