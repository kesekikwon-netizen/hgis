// MainWindow's Ctrl+Z ordering: layer undo stacks and m_undoActions on one time line
// (core/EditHistory). Kept apart from MainWindowUndo.cpp, which applies the actions.
#include "MainWindow.h"
#include "core/EditHistory.h"
#include "core/LayerOps.h"

#include <QUndoStack>
#include <algorithm>

#include <qgslayertreeview.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

namespace {
QgsVectorLayer* preferredMapLayer(QgsLayerTreeView* tree, QgsVectorLayer* editLayer) {
  if (tree) {
    if (auto* current = qobject_cast<QgsVectorLayer*>(tree->currentLayer())) return current;
  }
  return editLayer;
}

// Removed layers stay alive (with unsaved edits) until Ctrl+Z; keep only the latest ones.
constexpr int kMaxRemovedLayerUndo = 20;
constexpr int kMaxUndoActions = 300;

bool undoableSurveyLayer(QgsVectorLayer* layer, bool redo) {
  if (!layer || !layer->isValid() || !layer->undoStack()) return false;
  if (LayerOps::isReferenceLayer(layer) || LayerOps::isCadastralLayer(layer)) return false;
  return redo ? layer->undoStack()->canRedo() : layer->undoStack()->canUndo();
}
}

EditHistory* MainWindow::editHistory() {
  if (m_editHistory) return m_editHistory;
  m_editHistory = new EditHistory(this);
  // Layers that were already open before the first edit get a time line too.
  if (auto* project = QgsProject::instance()) {
    for (QgsMapLayer* layer : project->mapLayers()) {
      if (auto* vector = qobject_cast<QgsVectorLayer*>(layer)) m_editHistory->watch(vector);
    }
  }
  return m_editHistory;
}

void MainWindow::pushUndoAction(KaUndoAction action, QgsVectorLayer* stackLayer) {
  EditHistory* history = editHistory();
  const EditHistory::Stamp top = stackLayer ? history->undoTop(stackLayer) : EditHistory::Stamp();
  if (top.id != 0) {
    // The edit was just pushed on the layer stack: this entry is its fallback for after
    // 저장 clears the stack, and shares its time.
    action.commandId = top.id;
    action.seq = top.recent;
  } else if (top.command) {
    // A command from before the stack was watched: let the stack undo it first.
    action.seq = 0;
  } else {
    // Nothing on the stack (the change was written at once): a step of its own.
    action.seq = history->next();
  }
  m_undoActions.append(std::move(action));
  int removedLayerActions = 0;
  for (const KaUndoAction& entry : m_undoActions)
    removedLayerActions += entry.type == KaUndoAction::LayersRemoved ? 1 : 0;
  for (int i = 0; removedLayerActions > kMaxRemovedLayerUndo && i < m_undoActions.size();) {
    if (m_undoActions.at(i).type == KaUndoAction::LayersRemoved) {
      m_undoActions.removeAt(i);
      --removedLayerActions;
    } else {
      ++i;
    }
  }
  while (m_undoActions.size() > kMaxUndoActions) m_undoActions.removeFirst();
}

void MainWindow::pruneUndoActions() {
  if (!m_editHistory) return;
  m_undoActions.removeIf([this](const KaUndoAction& entry) {
    return entry.commandId != 0 &&
           m_editHistory->state(entry.commandId) == EditHistory::State::Discarded;
  });
}

int MainWindow::newestFallbackUndoIndex() {
  EditHistory* history = editHistory();
  int best = -1;
  quint64 bestTime = 0;
  for (int i = 0; i < m_undoActions.size(); ++i) {
    const KaUndoAction& entry = m_undoActions.at(i);
    quint64 time = entry.seq;
    if (entry.commandId != 0) {
      const EditHistory::State state = history->state(entry.commandId);
      // While its command is still on a layer stack, that stack undoes and redoes it.
      if (state == EditHistory::State::Applied || state == EditHistory::State::Undone) continue;
      if (state == EditHistory::State::Discarded) continue;
      time = std::max(time, history->lastRecent(entry.commandId));
    }
    if (best < 0 || time >= bestTime) {
      best = i;
      bestTime = time;
    }
  }
  return best;
}

QgsVectorLayer* MainWindow::newestUndoLayer() {
  auto* project = QgsProject::instance();
  if (!project) return nullptr;
  EditHistory* history = editHistory();
  QgsVectorLayer* best = nullptr;
  quint64 bestTime = 0;
  for (QgsMapLayer* layer : project->mapLayers()) {
    auto* vector = qobject_cast<QgsVectorLayer*>(layer);
    if (!undoableSurveyLayer(vector, false)) continue;
    const quint64 time = history->undoTop(vector).recent;
    if (time > bestTime) {
      best = vector;
      bestTime = time;
    }
  }
  // Edits made before the layer was watched have no time: keep the old preference.
  return best ? best : LayerOps::preferredUndoLayer(project, preferredMapLayer(m_layerTree, m_editLayer));
}

QgsVectorLayer* MainWindow::newestRedoLayer() {
  auto* project = QgsProject::instance();
  if (!project) return nullptr;
  EditHistory* history = editHistory();
  QgsVectorLayer* best = nullptr;
  quint64 bestTime = 0;
  for (QgsMapLayer* layer : project->mapLayers()) {
    auto* vector = qobject_cast<QgsVectorLayer*>(layer);
    if (!undoableSurveyLayer(vector, true)) continue;
    const quint64 time = history->redoTop(vector).undone;
    if (time > bestTime) {
      best = vector;
      bestTime = time;
    }
  }
  return best ? best : LayerOps::preferredRedoLayer(project, preferredMapLayer(m_layerTree, m_editLayer));
}
