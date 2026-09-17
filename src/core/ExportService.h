#pragma once
#include <QString>
#include <QStringList>
class QgsProject;
class ExportService {
public:
  // outDir must be absent or empty. Publish only a fully generated package;
  // failure returns an empty string and never overwrites a previous package.
  static QString exportSubmissionPackage(QgsProject* project,
                                         const QString& outDir,
                                         const QString& encoding,
                                         const QString& checklistSummary,
                                         bool blockOnError,
                                         bool hasChecklistErrors,
                                         QString* errorOut = nullptr);
  static QString writePdfViaLayout(QgsProject* project,
                                   const QString& layoutName,
                                   const QString& outPath,
                                   QString* errorOut = nullptr);
  static bool writeSha256Manifest(const QString& dir, QString* errorOut = nullptr);
};
