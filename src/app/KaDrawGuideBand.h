#pragma once

#include <QPointer>
#include <QString>
#include <QWidget>

class QAction;
class QFrame;
class QHideEvent;
class QLabel;
class QShowEvent;
class QToolButton;

// Floating guide over the map canvas (top-left, 12,12): the active tool's glyph, its title
// (「그리기 · 유구 면」) and a one-sentence hint in a pill, plus round undo/redo buttons on
// the right. Without a tool the pill folds away and only the two buttons remain, parked
// beside the opacity card so the card can sit back at its 10,10 spot. Display only: the
// window feeds setTool() from updateToolChip() and the buttons mirror m_actUndo/m_actRedo.
class KaDrawGuideBand : public QWidget {
  Q_OBJECT
 public:
  static constexpr int kMargin = 12;
  static constexpr int kHeight = 36;
  static constexpr int kButtonGap = 8;
  // The opacity card (KaLayerOpacityRail) is 260 wide at x 10; folded buttons sit past it.
  static constexpr int kFoldedLeft = 10 + 260 + 12;
  // How far the opacity card moves down while the pill shows: 12 + 36 + 8.
  static constexpr int kInsetWithTool = kMargin + kHeight + kButtonGap;

  explicit KaDrawGuideBand(QWidget* host);

  // The window's undo/redo actions; a null action leaves that button disabled.
  void setActions(QAction* undo, QAction* redo);
  // iconId: KaIcons id (empty or unknown: no glyph). Empty title folds the pill.
  void setTool(const QString& iconId, const QString& title, const QString& hint);
  bool hasTool() const { return !m_titleText.isEmpty(); }
  QString iconId() const { return m_iconId; }
  QString title() const { return m_titleText; }
  QString hint() const { return m_hintText; }
  // kInsetWithTool while the pill shows and the band is not hidden, else 0.
  int topInset() const;

  QFrame* pill() const { return m_pill; }
  QToolButton* undoButton() const { return m_undo; }
  QToolButton* redoButton() const { return m_redo; }

 signals:
  // Connect to KaLayerOpacityRail::setTopInset so the card never sits under the pill.
  void topInsetChanged(int px);

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;
  void showEvent(QShowEvent* event) override;
  void hideEvent(QHideEvent* event) override;

 private:
  void relayout();
  void emitInsetIfChanged();
  QToolButton* makeRoundButton(const QString& objectName, const QString& iconId, const QString& tip);
  static void bindAction(QToolButton* button, QAction* action, const QString& iconId);

  QPointer<QWidget> m_host;
  QFrame* m_pill = nullptr;
  QLabel* m_glyph = nullptr;
  QLabel* m_title = nullptr;
  QLabel* m_hint = nullptr;
  QToolButton* m_undo = nullptr;
  QToolButton* m_redo = nullptr;
  QString m_iconId;
  QString m_titleText;
  QString m_hintText;
  int m_lastInset = 0;
};
