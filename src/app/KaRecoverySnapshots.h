#pragma once

#include <QString>
#include <QStringList>

class QgsProject;
class QSettings;

// Bookkeeping for the silent two-minute recovery copies.
//
// Copies stay in the survey's 복구사본 folder with no startup dialog, no status-bar nag and
// no automatic restore (user decision). The home page only shows a quiet, click-only entry
// for a survey whose last session left edits in a copy but never saved them.
namespace KaRecoverySnapshots {

// Layers worth copying: vectors with unsaved edits, plus memory-only survey vectors whose
// data exists nowhere else. Unchanged file layers are already on disk and are skipped.
QStringList layerIdsToCapture(QgsProject* project);

// Notes that snapshotPath holds edits of surveyPath that were not saved into it.
void rememberUnsaved(QSettings& settings, const QString& surveyPath, const QString& snapshotPath);
// The survey file now holds everything (successful save); drop its note.
void forgetUnsaved(QSettings& settings, const QString& surveyPath);
// Copy noted for surveyPath, or empty when there is none or the copy file is gone.
QString unsavedSnapshotFor(QSettings& settings, const QString& surveyPath);

}  // namespace KaRecoverySnapshots
