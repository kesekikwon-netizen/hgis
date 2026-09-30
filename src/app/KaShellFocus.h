#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

class QSplitter;
class QWidget;

// Opt-in ways to give the map more room on a small laptop screen. Nothing here runs by
// itself and nothing is remembered between sessions. The ribbon keeps its one row and its
// fixed chips; 지도 넓게 보기 only hides it until the user presses the key again.
// The splitter may hold two panes (left panel | map) or three (| inspector on the right);
// every width change goes through sizesWithLeft() so the third pane keeps its width.
class KaShellFocus final : public QObject {
  Q_OBJECT
public:
  static constexpr int kMinLeftWidth = 200;
  static constexpr int kMaxLeftWidth = 348;  // the previous fixed default
  // Under kCompactSplitWidth (a 1366 window) the cap is 260: with the 272 inspector and the
  // two handles the map keeps >= 760 px, which 22 % (297) would not leave.
  static constexpr int kCompactSplitWidth = 1500;
  static constexpr int kCompactLeftWidth = 260;
  // Inspector width when the third pane has none yet (KaInspectorPanel::preferredWidth at 1366).
  static constexpr int kDefaultRightWidth = 272;
  static QString mapFocusKey() { return QStringLiteral("Ctrl+F11"); }
  static QString leftPanelKey() { return QStringLiteral("F9"); }
  static QString rightPanelKey() { return QStringLiteral("F10"); }

  // split: horizontal splitter with the left panel at index 0, the map at index 1 and,
  // when present, the inspector at index 2.
  explicit KaShellFocus(QSplitter* split, QObject* parent = nullptr);

  // Widgets hidden while the map is focused (the ribbon toolbar).
  void setChrome(const QList<QWidget*>& chrome);

  bool leftPanelCollapsed() const { return m_leftCollapsed; }
  bool rightPanelCollapsed() const { return m_rightCollapsed; }
  bool mapFocused() const { return m_focused; }

  // Left-panel width for a first start without a saved layout: 22 % of the splitter, capped
  // at kCompactLeftWidth under 1500 px and at the old 348 px above, so large screens look
  // the same and a 1366 laptop keeps a >= 760 px map beside the inspector.
  static int defaultLeftPanelWidth(int totalWidth);
  // Sizes for setSizes() that put the left pane at left px. Two panes: the map takes the
  // rest. Three panes: the right pane keeps its current width (kDefaultRightWidth when it
  // has none yet, 0 while it is hidden) and the map takes what remains.
  static QList<int> sizesWithLeft(const QSplitter* split, int left);
  // A restored splitter state is the user's choice; the ratio default then stays off.
  void markUserWidthRestored() { m_widthDecided = true; }
  // Applies the ratio default once, when the splitter has its real width.
  void applyDefaultWidthOnce();

public slots:
  void setLeftPanelCollapsed(bool collapsed);
  void toggleLeftPanel();
  void setRightPanelCollapsed(bool collapsed);
  void toggleRightPanel();
  void setMapFocused(bool focused);
  void toggleMapFocus();
  // Brings the ribbon and both panels back, e.g. before the window saves its state.
  void restoreAll();

signals:
  void changed(const QString& message);

private:
  void applyLeftCollapsed(bool collapsed);
  void applyRightCollapsed(bool collapsed);
  void foldInto(int foldedIndex, const QList<int>& before);
  void restoreSizes(const QList<int>& saved);

  QPointer<QSplitter> m_split;
  QList<QPointer<QWidget>> m_chrome;
  QList<QPointer<QWidget>> m_hiddenChrome;
  QList<int> m_savedSizes;
  QList<int> m_savedRightSizes;
  bool m_leftCollapsed = false;
  bool m_rightCollapsed = false;
  bool m_focused = false;
  bool m_leftCollapsedBeforeFocus = false;
  bool m_widthDecided = false;
};
