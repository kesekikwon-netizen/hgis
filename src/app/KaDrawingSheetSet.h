#pragma once
// Named copies of the drawing sheet (도면 보관). The studio always edits and
// submits the working sheet ("user_sheet"); stored copies are only snapshots the
// user can reopen into it. The submission rule stays: only the composed working
// sheet becomes 조사도면.pdf.
#include <QString>
#include <QVector>
#include <functional>

class QMenu;
class QgsPrintLayout;
class QgsProject;

namespace KaDrawingSheetSet {
struct Entry {
  QString layoutName;
  QString title;
};

// Layout name used for a stored copy. Never equal to the working sheet name.
QString layoutNameFor(const QString& title);
QVector<Entry> list(QgsProject* project);
bool contains(QgsProject* project, const QString& title);
// Copies the working sheet into a stored copy named title (replaces an older copy).
bool store(QgsProject* project, const QString& workingName, const QString& title, QString* error);
// Replaces the working sheet with a copy of the stored sheet. The caller must
// detach any view from the old working layout first; it is deleted here.
QgsPrintLayout* restore(QgsProject* project, const QString& title, const QString& workingName,
                        QString* error);
bool remove(QgsProject* project, const QString& title);

struct MenuActions {
  std::function<void()> store;
  std::function<void(const QString&)> restore;
  std::function<void(const QString&)> remove;
};
// Rebuilds menu: store current, reopen each stored sheet, delete submenu.
void fillMenu(QMenu* menu, QgsProject* project, const MenuActions& actions);
}  // namespace KaDrawingSheetSet
