#include "KaDrawingLayerSets.h"

#include <QAction>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>

#include <qgslayertree.h>
#include <qgslayertreemodel.h>
#include <qgsmapthemecollection.h>
#include <qgsproject.h>

namespace KaDrawingLayerSets {

QStringList names(QgsProject* project) {
  if (!project || !project->mapThemeCollection()) return {};
  QStringList out = project->mapThemeCollection()->mapThemes();
  out.sort(Qt::CaseInsensitive);
  return out;
}

bool contains(QgsProject* project, const QString& name) {
  return project && project->mapThemeCollection() && project->mapThemeCollection()->hasMapTheme(name);
}

bool save(QgsProject* project, const QString& name, QgsLayerTreeModel* model) {
  const QString key = name.trimmed();
  if (!project || !model || key.isEmpty() || !project->layerTreeRoot() || !project->mapThemeCollection())
    return false;
  const auto record =
      QgsMapThemeCollection::createThemeFromCurrentState(project->layerTreeRoot(), model);
  QgsMapThemeCollection* themes = project->mapThemeCollection();
  if (themes->hasMapTheme(key))
    themes->update(key, record);
  else
    themes->insert(key, record);
  project->setDirty(true);
  return true;
}

bool apply(QgsProject* project, const QString& name, QgsLayerTreeModel* model) {
  if (!model || !contains(project, name) || !project->layerTreeRoot()) return false;
  project->mapThemeCollection()->applyTheme(name, project->layerTreeRoot(), model);
  return true;
}

bool remove(QgsProject* project, const QString& name) {
  if (!contains(project, name)) return false;
  project->mapThemeCollection()->removeMapTheme(name);
  project->setDirty(true);
  return true;
}

void fillMenu(QMenu* menu, QgsProject* project, QgsLayerTreeModel* model, QWidget* dialogParent,
              const std::function<void(const QString&)>& status) {
  if (!menu) return;
  menu->clear();
  menu->setToolTipsVisible(true);
  auto say = [status](const QString& text) {
    if (status) status(text);
  };
  QAction* store = menu->addAction(QStringLiteral("지금 켜진 레이어를 세트로 저장…"));
  store->setEnabled(project && model);
  QObject::connect(store, &QAction::triggered, menu, [project, model, dialogParent, say]() {
    bool ok = false;
    const QString name = QInputDialog::getText(
        dialogParent, QStringLiteral("레이어 세트"),
        QStringLiteral("세트 이름 (예: 유구 배치, 지형·지적 중첩)"), QLineEdit::Normal,
        QStringLiteral("세트 %1").arg(names(project).size() + 1), &ok).trimmed();
    if (!ok || name.isEmpty()) return;
    if (contains(project, name) &&
        QMessageBox::question(dialogParent, QStringLiteral("레이어 세트"),
                              QStringLiteral("「%1」 세트가 이미 있습니다. 지금 켜짐 상태로 바꿀까요?").arg(name),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
      return;
    if (save(project, name, model))
      say(QStringLiteral("레이어 세트 「%1」을 저장했습니다. 고를 때만 적용됩니다.").arg(name));
  });
  const QStringList sets = names(project);
  menu->addSeparator();
  if (sets.isEmpty()) {
    QAction* none = menu->addAction(QStringLiteral("저장한 세트가 없습니다"));
    none->setEnabled(false);
    return;
  }
  for (const QString& name : sets) {
    QAction* pick = menu->addAction(QStringLiteral("적용: %1").arg(name));
    pick->setToolTip(QStringLiteral("이 세트의 켜짐 상태로 레이어를 켜고 끕니다. 지도 화면에도 같이 적용됩니다."));
    QObject::connect(pick, &QAction::triggered, menu, [project, model, name, say]() {
      if (apply(project, name, model))
        say(QStringLiteral("레이어 세트 「%1」을 적용했습니다.").arg(name));
    });
  }
  QMenu* drop = menu->addMenu(QStringLiteral("세트 지우기"));
  for (const QString& name : sets) {
    QAction* del = drop->addAction(name);
    QObject::connect(del, &QAction::triggered, menu, [project, dialogParent, name, say]() {
      if (QMessageBox::question(dialogParent, QStringLiteral("레이어 세트"),
                                QStringLiteral("「%1」 세트를 지울까요? 레이어는 그대로입니다.").arg(name),
                                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
      if (remove(project, name))
        say(QStringLiteral("레이어 세트 「%1」을 지웠습니다.").arg(name));
    });
  }
}

}  // namespace KaDrawingLayerSets
