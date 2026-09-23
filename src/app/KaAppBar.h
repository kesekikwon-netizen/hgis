#pragma once

#include <QPixmap>
#include <QWidget>

class QLabel;
class QLineEdit;
class QToolButton;

// Navy bar across the top of the main window: the Strata name, the open survey
// and whether it is saved, and one search field for places and lot numbers.
// The province chips live in the 「지역」 drop-down so the ribbon stays short.
class KaAppBar final : public QWidget {
  Q_OBJECT
public:
  explicit KaAppBar(QWidget* parent = nullptr);

  // Survey shown next to the name; an empty name hides it (home screen).
  void setSurvey(const QString& name, bool unsaved);
  // Hosts the province chips (KaRegionLocator) inside the 「지역」 drop-down.
  void setRegionWidget(QWidget* widget);
  void focusSearch();
  QLineEdit* searchField() const { return m_search; }
  // True when the last word is a lot number such as 1615, 1615-3 or 산12.
  static bool looksLikeLot(const QString& query);

signals:
  void searchRequested(const QString& query, bool lot);
  void aboutRequested();

protected:
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

private:
  QLabel* m_survey = nullptr;
  QLabel* m_state = nullptr;
  QToolButton* m_region = nullptr;
  QLineEdit* m_search = nullptr;
  QPixmap m_texture;
};
