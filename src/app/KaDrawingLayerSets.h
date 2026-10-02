#pragma once
// Named layer sets for drawing sheets (QGIS map themes). A set records which
// layers are checked. It is applied only when the user picks it from the menu;
// nothing restores a set automatically.
#include <QString>
#include <QStringList>
#include <functional>

class QMenu;
class QWidget;
class QgsProject;
class QgsLayerTreeModel;

namespace KaDrawingLayerSets {
QStringList names(QgsProject* project);
bool contains(QgsProject* project, const QString& name);
// Stores the current check state of the project layer tree under name (replaces it).
bool save(QgsProject* project, const QString& name, QgsLayerTreeModel* model);
bool apply(QgsProject* project, const QString& name, QgsLayerTreeModel* model);
bool remove(QgsProject* project, const QString& name);
// Rebuilds menu: save current, apply each set, delete submenu. status gets one line.
void fillMenu(QMenu* menu, QgsProject* project, QgsLayerTreeModel* model, QWidget* dialogParent,
              const std::function<void(const QString&)>& status);
}  // namespace KaDrawingLayerSets
