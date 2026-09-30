#include "EditHistory.h"

#include <algorithm>

#include <QUndoCommand>

#include <qgsvectorlayer.h>

EditHistory::EditHistory(QObject* parent) : QObject(parent) {}

void EditHistory::watch(QgsVectorLayer* layer) {
  QUndoStack* stack = layer ? layer->undoStack() : nullptr;
  if (!stack || m_tracks.contains(stack)) return;
  Track track;
  track.stack = stack;
  track.index = stack->index();
  track.cells.resize(stack->count());
  for (int pos = 0; pos < stack->count(); ++pos) track.cells[pos].command = stack->command(pos);
  m_tracks.insert(stack, track);
  connect(stack, &QUndoStack::indexChanged, this, [this, stack](int) { sync(stack); });
  connect(stack, &QObject::destroyed, this, [this, stack]() {
    const auto it = m_tracks.constFind(stack);
    if (it == m_tracks.constEnd()) return;
    for (const Stamp& cell : it->cells) {
      if (cell.id == 0) continue;
      m_gone.insert(cell.id, State::Discarded);
      m_goneRecent.insert(cell.id, cell.recent);
    }
    m_tracks.remove(stack);
  });
}

quint64 EditHistory::next() {
  return ++m_seq;
}

void EditHistory::sync(QUndoStack* stack) {
  const auto it = m_tracks.find(stack);
  if (it == m_tracks.end() || !stack) return;
  Track& track = it.value();
  const int count = stack->count();
  const int index = std::min(stack->index(), count);

  // Match commands by identity, not position: the undo limit drops the oldest ones.
  QHash<const QUndoCommand*, int> oldPos;
  for (int i = 0; i < track.cells.size(); ++i) oldPos.insert(track.cells.at(i).command, i);
  QVector<Stamp> cells(count);
  QVector<int> fromOld(count, -1);
  QVector<bool> kept(track.cells.size(), false);
  for (int pos = 0; pos < count; ++pos) {
    const QUndoCommand* command = stack->command(pos);
    const int old = oldPos.value(command, -1);
    if (old >= 0 && !kept.at(old)) {
      cells[pos] = track.cells.at(old);
      fromOld[pos] = old;
      kept[old] = true;
    } else {
      cells[pos].command = command;
    }
  }
  // A command that left while applied was written by 저장; one that left while undone
  // was thrown away by a new edit or by 저장 after 되돌리기. A save that has to undo the
  // buffer before it clears the stack names what it wrote through commitApplied().
  bool anyGone = false;
  for (int i = 0; i < track.cells.size(); ++i) {
    const Stamp& gone = track.cells.at(i);
    if (kept.at(i)) continue;
    anyGone = true;
    if (gone.id == 0) continue;
    const bool committed = i < track.index || track.committing.contains(gone.command);
    m_gone.insert(gone.id, committed ? State::Committed : State::Discarded);
    m_goneRecent.insert(gone.id, gone.recent);
  }
  // Pushed (new) or redone (was above the old index) now, oldest first.
  bool anyNew = false;
  for (int pos = 0; pos < index; ++pos) {
    const int old = fromOld.at(pos);
    if (old < 0) {
      anyNew = true;
      cells[pos].id = ++m_seq;
      cells[pos].recent = cells[pos].id;
    } else if (old >= track.index) {
      cells[pos].recent = ++m_seq;
    }
  }
  // The written commands are gone (or editing went on): freed pointers must not match
  // a later command that happens to reuse the address.
  if (anyGone || anyNew) track.committing.clear();
  // Undone now. The lowest one is redone first, so it gets the latest time.
  for (int pos = count - 1; pos >= index; --pos) {
    const int old = fromOld.at(pos);
    if (old >= 0 && old < track.index) cells[pos].undone = ++m_seq;
  }
  track.cells = cells;
  track.index = index;
}

void EditHistory::commitApplied(QgsVectorLayer* layer) {
  QUndoStack* stack = layer ? layer->undoStack() : nullptr;
  const auto it = stack ? m_tracks.find(stack) : m_tracks.end();
  if (it == m_tracks.end()) return;
  Track& track = it.value();
  track.committing.clear();
  const int index = std::min(stack->index(), stack->count());
  for (int pos = 0; pos < index; ++pos) track.committing.insert(stack->command(pos));
}

EditHistory::Stamp EditHistory::stampAt(const QUndoStack* stack, int pos) const {
  if (!stack || pos < 0 || pos >= stack->count()) return {};
  const QUndoCommand* command = stack->command(pos);
  const auto it = m_tracks.constFind(stack);
  if (it != m_tracks.constEnd() && pos < it->cells.size() && it->cells.at(pos).command == command)
    return it->cells.at(pos);
  Stamp unknown;
  unknown.command = command;
  return unknown;
}

EditHistory::Stamp EditHistory::undoTop(QgsVectorLayer* layer) const {
  const QUndoStack* stack = layer ? layer->undoStack() : nullptr;
  return stack ? stampAt(stack, stack->index() - 1) : Stamp();
}

EditHistory::Stamp EditHistory::redoTop(QgsVectorLayer* layer) const {
  const QUndoStack* stack = layer ? layer->undoStack() : nullptr;
  return stack ? stampAt(stack, stack->index()) : Stamp();
}

EditHistory::State EditHistory::state(quint64 id) const {
  if (id == 0) return State::Unknown;
  for (const Track& track : m_tracks) {
    for (int pos = 0; pos < track.cells.size(); ++pos) {
      if (track.cells.at(pos).id == id) return pos < track.index ? State::Applied : State::Undone;
    }
  }
  return m_gone.value(id, State::Unknown);
}

quint64 EditHistory::lastRecent(quint64 id) const {
  if (id == 0) return 0;
  for (const Track& track : m_tracks) {
    for (const Stamp& cell : track.cells) {
      if (cell.id == id) return cell.recent;
    }
  }
  return m_goneRecent.value(id, 0);
}
