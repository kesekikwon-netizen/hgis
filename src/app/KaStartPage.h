#pragma once
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QWidget;

// Home page: a navy band with the Strata name, the two start actions and a
// card for the most recent survey, above the list of recent surveys. Nothing
// opens on its own; every survey opens from a click.
class KaStartPage : public QWidget {
  Q_OBJECT
public:
  explicit KaStartPage(QWidget* parent = nullptr);
  void reload();

signals:
  void newSurveyRequested();
  void openRequested();
  void recentOpened(const QString& path);
  void forgetRequested(const QString& path);

private:
  QWidget* buildHero();
  QWidget* buildRecentCard();
  QWidget* buildGuideCard();
  void openRow(int row);
  void showRecentMenu(const QPoint& pos);
  void applyFilter(const QString& text);

  QLabel* m_empty = nullptr;
  QLabel* m_count = nullptr;
  QLineEdit* m_filter = nullptr;
  QTableWidget* m_recent = nullptr;
  QWidget* m_continue = nullptr;
  QLabel* m_continueName = nullptr;
  QLabel* m_continuePath = nullptr;
  QLabel* m_continueWhen = nullptr;
  QString m_continueTarget;
};
