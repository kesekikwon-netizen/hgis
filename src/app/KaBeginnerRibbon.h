#pragma once

#include <QHash>
#include <QList>
#include <QWidget>

class QAction;
class QFrame;
class QHBoxLayout;
class QToolButton;
class QMenu;
class QScrollArea;

// 초보자용 위 리본. 기존 QAction/QToolButton을 단계 그룹에만 옮긴다.
class KaBeginnerRibbon : public QWidget {
  Q_OBJECT
public:
  explicit KaBeginnerRibbon(QWidget* parent = nullptr);

  QFrame* addGroup(const QString& id, const QString& caption);
  QToolButton* addAction(const QString& groupId, QAction* action);
  void addWidget(const QString& groupId, QWidget* widget);
  QFrame* group(const QString& id) const;
  // 앞쪽 id일수록 좁은 창에서도 리본에 남긴다. 화면 순서는 addGroup 순서를 유지한다.
  void setKeepPriority(const QStringList& ids);
  // 폭이 모자라면 기타·정합을 접어서라도 이 묶음은 리본에 둔다.
  void setPinned(const QStringList& ids);
  QList<QToolButton*> tabButtons() const;
  void applyTabOrder();
  // 이름은 예전 두 줄 맞춤. 지금은 줄바꿈을 없애 한 줄로 맞춘다.
  static QString twoLine(const QString& text);
  // One chip: 56 x 82 content box, one-line label shrunk to fit but never below
  // ButtonMetrics::ribbonMinFontSize; a label that still does not fit keeps its
  // full wording in the tooltip (screen readers already read the full text()).
  static void applyTwoLine(QToolButton* button);
  // Which groups stay on the one-row ribbon, in priority order. survey, out and
  // record come first and may fold any other group into 「더 많은 작업」; pinned
  // groups may fold unpinned ones. A group is placed only when folding others
  // actually makes it fit, and the overflow button always keeps its own room.
  static QStringList planGroups(const QStringList& priority, const QStringList& pinned,
                                const QHash<QString, int>& widths, int available, int overflowWidth);
  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

protected:
  void resizeEvent(QResizeEvent* event) override;
  void showEvent(QShowEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  QHBoxLayout* buttonRow(const QString& groupId) const;
  int groupInsertIndex() const;
  void updateOverflow();

  QHBoxLayout* m_row = nullptr;
  QHash<QString, QFrame*> m_groups;
  QHash<QString, QHBoxLayout*> m_btnRows;
  QStringList m_groupOrder;
  QStringList m_keepPriority;
  QStringList m_pinned;
  QHash<QString, QMenu*> m_groupMenus;
  QHash<QString, QScrollArea*> m_groupScrolls;
  QToolButton* m_overflow = nullptr;
  QMenu* m_overflowMenu = nullptr;
  bool m_updatingOverflow = false;
  QString m_lastFoldLog;  // last "[ribbon] 접힘" line, so the log records changes only
};
