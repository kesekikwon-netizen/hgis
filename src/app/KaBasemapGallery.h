#pragma once

#include <QIcon>
#include <QToolButton>

class QAction;
class QGridLayout;
class QMenu;

// One ribbon button for every reference map. It opens a panel of labelled
// previews; each preview is the existing control for that map, so its toggle,
// menu and tooltip keep working and only where the buttons live changes.
class KaBasemapGallery final : public QToolButton {
  Q_OBJECT
public:
  explicit KaBasemapGallery(QWidget* parent = nullptr);

  // Moves an existing map button into the panel and gives it a preview.
  void addButton(QToolButton* button, const QString& previewId);
  // Adds a button for action, for maps that are plain actions.
  QToolButton* addAction(QAction* action, const QString& previewId);
  QWidget* panel() const { return m_panel; }
  // Small picture of what the map looks like: terrain, contour, dem, soil,
  // paleo, cadastral, daedong, map1919, geology or river.
  static QIcon preview(const QString& id);

private:
  void place(QToolButton* button, const QString& previewId);

  QMenu* m_menu = nullptr;
  QWidget* m_panel = nullptr;
  QGridLayout* m_grid = nullptr;
  int m_count = 0;
};
