#include "KaRecoverySnapshots.h"

#include "core/LayerOps.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>

#include <qgsproject.h>
#include <qgsvectorlayer.h>

namespace {

// "survey|snapshot" pairs; '|' cannot appear in a Windows path.
const QString kKey = QStringLiteral("Survey/UnsavedRecovery");
constexpr int kMaxNotes = 12;

QString normalized(const QString& path) {
  return path.isEmpty() ? QString() : QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

bool samePath(const QString& a, const QString& b) {
  return !a.isEmpty() && normalized(a).compare(normalized(b), Qt::CaseInsensitive) == 0;
}

QStringList withoutSurvey(const QStringList& notes, const QString& surveyPath) {
  QStringList kept;
  for (const QString& note : notes)
    if (!samePath(note.section(QLatin1Char('|'), 0, 0), surveyPath)) kept << note;
  return kept;
}

}  // namespace

namespace KaRecoverySnapshots {

QStringList layerIdsToCapture(QgsProject* project) {
  QStringList ids;
  if (!project) return ids;
  for (QgsMapLayer* layer : project->mapLayers()) {
    auto* vector = qobject_cast<QgsVectorLayer*>(layer);
    if (!vector) continue;
    const bool unsavedEdit = vector->isEditable() && vector->isModified();
    const bool memoryOnly = vector->isValid() &&
                            vector->providerType() == QLatin1String("memory") &&
                            !LayerOps::isReferenceLayer(vector) && !LayerOps::isCadastralLayer(vector);
    if (unsavedEdit || memoryOnly) ids << vector->id();
  }
  return ids;
}

void rememberUnsaved(QSettings& settings, const QString& surveyPath, const QString& snapshotPath) {
  if (surveyPath.isEmpty() || snapshotPath.isEmpty()) return;
  QStringList notes = withoutSurvey(settings.value(kKey).toStringList(), surveyPath);
  notes.prepend(normalized(surveyPath) + QLatin1Char('|') + normalized(snapshotPath));
  while (notes.size() > kMaxNotes) notes.removeLast();
  settings.setValue(kKey, notes);
}

void forgetUnsaved(QSettings& settings, const QString& surveyPath) {
  if (surveyPath.isEmpty()) return;
  const QStringList notes = settings.value(kKey).toStringList();
  const QStringList kept = withoutSurvey(notes, surveyPath);
  if (kept.size() == notes.size()) return;
  if (kept.isEmpty())
    settings.remove(kKey);
  else
    settings.setValue(kKey, kept);
}

QString unsavedSnapshotFor(QSettings& settings, const QString& surveyPath) {
  if (surveyPath.isEmpty()) return {};
  for (const QString& note : settings.value(kKey).toStringList()) {
    if (!samePath(note.section(QLatin1Char('|'), 0, 0), surveyPath)) continue;
    const QString snapshot = note.section(QLatin1Char('|'), 1);
    return QFileInfo(snapshot).isFile() ? snapshot : QString();
  }
  return {};
}

}  // namespace KaRecoverySnapshots
