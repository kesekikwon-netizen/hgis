#pragma once

#include <QHash>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <functional>
#include <memory>

class QObject;
class QTimer;
class QgsLayerTree;
class QgsLayerTreeView;
class QgsProject;

// Order and check marks of the layer list at one moment (evaluation F186).
// Keys: "L:<layer id>" for a layer row, "G:<parent path>/<name>" for a group row.
struct KaLayerTreeSnapshot {
  QHash<QString, QStringList> children;  // group key ("" = root) -> child keys in order
  QHash<QString, bool> checked;          // row key -> its own check mark
  QStringList rowKeys() const;           // every row key (layers and groups), sorted
  bool operator==(const KaLayerTreeSnapshot& other) const {
    return children == other.children && checked == other.checked;
  }
  bool operator!=(const KaLayerTreeSnapshot& other) const { return !(*this == other); }
};

// Records drag-reordering and check-mark changes the user makes in the layer
// list, so Ctrl+Z can put them back. Adding or removing a row is not recorded
// here, whether it is a layer or a group (an empty group deleted with Delete is
// restored by its own LayersRemoved step, so no step may sit on top of it);
// programmatic moves (satellite kept at the bottom, cadastral kept at the root)
// are folded into the baseline without an undo step. A group dragged into another
// parent changes its key and is treated like a removed row. The group menu is
// unchanged.
class KaLayerTreeUndo final {
public:
  using Push = std::function<void(std::shared_ptr<KaLayerTreeSnapshot> before)>;

  KaLayerTreeUndo(QgsProject* project, QgsLayerTreeView* view, Push push);
  ~KaLayerTreeUndo();
  KaLayerTreeUndo(const KaLayerTreeUndo&) = delete;
  KaLayerTreeUndo& operator=(const KaLayerTreeUndo&) = delete;

  static std::shared_ptr<KaLayerTreeSnapshot> capture(QgsLayerTree* root);
  // Puts rows back in the recorded order and check state. Rows added since stay
  // where they are. False with a Korean reason when nothing could be restored.
  static bool restoreInto(QgsLayerTree* root, const KaLayerTreeSnapshot& snapshot, QString* error = nullptr);
  bool restore(const KaLayerTreeSnapshot& snapshot, QString* error = nullptr);

  // Marks the next list change as the user's (for buttons outside the list view,
  // e.g. 전체 켜기/끄기). Clicks and drops inside the view are noticed automatically.
  // Every observed change becomes the new baseline by itself, so opening a survey
  // needs no reset.
  void noteUserGesture();

private:
  void scheduleCheck();
  void check();

  QPointer<QgsProject> m_project;
  QPointer<QgsLayerTreeView> m_view;
  QPointer<QTimer> m_timer;
  std::unique_ptr<QObject> m_context;
  Push m_push;
  std::shared_ptr<KaLayerTreeSnapshot> m_baseline;
  qint64 m_gestureUntilMs = 0;
  bool m_restoring = false;
};
