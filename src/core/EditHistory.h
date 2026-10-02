#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QUndoStack>
#include <QVector>

class QUndoCommand;
class QgsVectorLayer;

// Puts edits on different layers on one time line.
//
// QGIS keeps one undo stack per layer and the app keeps a few actions outside those
// stacks (restoring removed layers, fallbacks that still work after 저장 cleared the
// stacks). Every command is stamped with a global sequence when it is pushed or redone,
// so Ctrl+Z can revert whatever happened last instead of the lowest layer in the tree.
class EditHistory : public QObject {
  Q_OBJECT
public:
  enum class State { Unknown, Applied, Undone, Committed, Discarded };
  struct Stamp {
    const QUndoCommand* command = nullptr;
    quint64 id = 0;      // unique per push; 0 = pushed before the stack was watched
    quint64 recent = 0;  // time of the last push or redo
    quint64 undone = 0;  // time of the last undo (orders redo)
  };

  explicit EditHistory(QObject* parent = nullptr);

  // Starts stamping the layer's undo stack. Safe to call repeatedly.
  void watch(QgsVectorLayer* layer);
  // Time for an action kept outside the layer stacks.
  quint64 next();
  // The command Ctrl+Z would revert on this layer, or an empty stamp.
  Stamp undoTop(QgsVectorLayer* layer) const;
  // The command Ctrl+Y would apply again on this layer, or an empty stamp.
  Stamp redoTop(QgsVectorLayer* layer) const;
  // The commands applied on this layer right now are being written to the file. 저장
  // drops the buffer afterwards (commitChanges, or rollBack + clear after a generation
  // write), which undoes them first; they still count as Committed when they leave the
  // stack, so their fallbacks work after the save.
  void commitApplied(QgsVectorLayer* layer);
  // Applied / Undone while the command is on a stack; Committed when it left the stack
  // while applied (저장), Discarded when it left while undone (a new edit, 되돌리기 후 저장).
  State state(quint64 id) const;
  // Last push/redo time of a command, also after it left its stack.
  quint64 lastRecent(quint64 id) const;

private:
  struct Track {
    QPointer<QUndoStack> stack;
    int index = 0;
    QVector<Stamp> cells;  // one per command, same order as the stack
    QSet<const QUndoCommand*> committing;  // applied commands 저장 is writing (commitApplied)
  };
  void sync(QUndoStack* stack);
  Stamp stampAt(const QUndoStack* stack, int pos) const;

  QHash<const QUndoStack*, Track> m_tracks;
  QHash<quint64, State> m_gone;
  QHash<quint64, quint64> m_goneRecent;
  quint64 m_seq = 0;
};
