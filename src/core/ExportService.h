#pragma once
#include <QString>
#include <QStringList>
#include <functional>
class QgsProject;

// Facts about the survey that go into README_submit.txt, plus a step callback.
struct SubmitPackageInfo {
  QString surveyName;
  QString surveyPath;  // on-disk survey file; its SHA-256 is recorded
  bool unsavedEditsIncluded = false;
  // Called before each step (done of total). Return false to cancel; the
  // staging folder is removed and no package is published.
  std::function<bool(int done, int total, const QString& step)> progress;
};

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
                                         QString* errorOut = nullptr,
                                         const SubmitPackageInfo& info = {});
  static QString writePdfViaLayout(QgsProject* project,
                                   const QString& layoutName,
                                   const QString& outPath,
                                   QString* errorOut = nullptr);
  static bool writeSha256Manifest(const QString& dir, QString* errorOut = nullptr);
};
