#pragma once

#include <QWidget>

class QLineEdit;
class QToolButton;

// Place search at the right end of the ribbon row: the 「지역」 drop-down with the
// province chips and one field for addresses and lot numbers. It used to be a navy
// bar of its own; that cost a whole row and shared it with the drawing tools. The
// survey name and its saved state are in the window title.
class KaAppBar final : public QWidget {
  Q_OBJECT
public:
  explicit KaAppBar(QWidget* parent = nullptr);

  // Hosts the province chips (KaRegionLocator) inside the 「지역」 drop-down.
  void setRegionWidget(QWidget* widget);
  void focusSearch();
  QLineEdit* searchField() const { return m_search; }
  // True when the last word is a lot number such as 1615, 1615-3 or 산12.
  static bool looksLikeLot(const QString& query);

signals:
  void searchRequested(const QString& query, bool lot);

private:
  QToolButton* m_region = nullptr;
  QLineEdit* m_search = nullptr;
};
