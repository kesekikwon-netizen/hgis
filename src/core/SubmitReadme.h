#pragma once
// README_submit.txt content for the submission package: what was submitted,
// from which survey file and program, and where the reference maps came from.
#include <QString>
#include <QStringList>

class QgsProject;

namespace SubmitReadme {

struct Inputs {
  QString encoding;
  QString checklistSummary;
  QStringList fieldNotes;      // "survey_area survey_name=surv_name"
  QStringList layerLines;      // one line per written SHP, after re-open verification
  QStringList referenceLines;  // reference-map provenance lines
  QString surveyName;
  QString surveyFile;          // file name only
  QString surveySha256;        // empty when the survey file is not on disk
  bool unsavedEditsIncluded = false;
  bool hasSheetPdf = false;
  bool hasSectionPdf = false;
};

QString build(const Inputs& in);

// Reference, basemap and cadastral layers drawn on the given layouts. Only the
// host of an online map is written (never a path or key); intranet heritage data
// is named by kind and date, never by file.
QStringList referenceLines(QgsProject* project, const QStringList& layoutNames);

// Hex SHA-256 of a file read in chunks; empty with error set on failure.
QString sha256OfFile(const QString& path, QString* error = nullptr);

// Folder-name-safe text (Windows reserved characters replaced), at most 40 chars.
QString folderNamePart(const QString& text);

}  // namespace SubmitReadme
