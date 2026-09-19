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
  QList<QToolButton*> tabButtons() const;
  void applyTabOrder();
  static QString twoLine(const QString& text);
  static void applyTwoLine(QToolButton* button);
  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

protected:
  void resizeEvent(QResizeEvent* event) override;
  void showEvent(QShowEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  QHBoxLayout* buttonRow(const QString& groupId) const;
  void updateOverflow();

  QHBoxLayout* m_row = nullptr;
  QHash<QString, QFrame*> m_groups;
  QHash<QString, QHBoxLayout*> m_btnRows;
  QStringList m_groupOrder;
  QHash<QString, QMenu*> m_groupMenus;
  QHash<QString, QScrollArea*> m_groupScrolls;
  QToolButton* m_overflow = nullptr;
  QMenu* m_overflowMenu = nullptr;
  bool m_updatingOverflow = false;
};
