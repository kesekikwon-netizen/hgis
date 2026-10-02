#include "KaDrawingSheetSet.h"

#include "core/LayoutService.h"

#include <QAction>
#include <QMenu>
#include <algorithm>

#include <qgslayoutmanager.h>
#include <qgsprintlayout.h>
#include <qgsproject.h>

namespace {
const QString kSavedProperty = QStringLiteral("ka_hgis/saved_sheet");
const QString kTitleProperty = QStringLiteral("ka_hgis/saved_sheet_title");

QgsPrintLayout* printLayout(QgsProject* project, const QString& name) {
  if (!project || !project->layoutManager()) return nullptr;
  return dynamic_cast<QgsPrintLayout*>(project->layoutManager()->layoutByName(name));
}

void fail(QString* error, const QString& text) {
  if (error) *error = text;
}
}  // namespace

namespace KaDrawingSheetSet {

QString layoutNameFor(const QString& title) {
  return QStringLiteral("도면 보관: %1").arg(title.trimmed());
}

QVector<Entry> list(QgsProject* project) {
  QVector<Entry> out;
  if (!project || !project->layoutManager()) return out;
  for (QgsPrintLayout* layout : project->layoutManager()->printLayouts()) {
    if (!layout || !layout->customProperty(kSavedProperty, false).toBool()) continue;
    out.append({layout->name(), layout->customProperty(kTitleProperty).toString()});
  }
  std::sort(out.begin(), out.end(), [](const Entry& a, const Entry& b) {
    return a.title.localeAwareCompare(b.title) < 0;
  });
  return out;
}

bool contains(QgsProject* project, const QString& title) {
  QgsPrintLayout* layout = printLayout(project, layoutNameFor(title));
  return layout && layout->customProperty(kSavedProperty, false).toBool();
}

bool store(QgsProject* project, const QString& workingName, const QString& title, QString* error) {
  const QString name = title.trimmed();
  if (name.isEmpty()) {
    fail(error, QStringLiteral("도면 이름을 적으세요."));
    return false;
  }
  QgsPrintLayout* working = printLayout(project, workingName);
  if (!working) {
    fail(error, QStringLiteral("보관할 용지가 없습니다."));
    return false;
  }
  QgsPrintLayout* copy = working->clone();
  if (!copy) {
    fail(error, QStringLiteral("도면을 복사하지 못했습니다."));
    return false;
  }
  const QString layoutName = layoutNameFor(name);
  if (QgsPrintLayout* old = printLayout(project, layoutName)) {
    if (!old->customProperty(kSavedProperty, false).toBool()) {
      delete copy;
      fail(error, QStringLiteral("같은 이름의 다른 조판이 있습니다. 다른 이름을 쓰세요."));
      return false;
    }
    project->layoutManager()->removeLayout(old);
  }
  copy->setName(layoutName);
  copy->setCustomProperty(kSavedProperty, true);
  copy->setCustomProperty(kTitleProperty, name);
  // A stored copy is never a submission sheet by itself.
  copy->setCustomProperty(QStringLiteral("ka_hgis/auto_template"), false);
  copy->setCustomProperty(QStringLiteral("ka_hgis/user_composed"), false);
  if (!project->layoutManager()->addLayout(copy)) {
    delete copy;
    fail(error, QStringLiteral("도면을 보관하지 못했습니다."));
    return false;
  }
  project->setDirty(true);
  return true;
}

QgsPrintLayout* restore(QgsProject* project, const QString& title, const QString& workingName,
                        QString* error) {
  QgsPrintLayout* saved = printLayout(project, layoutNameFor(title));
  if (!saved || !saved->customProperty(kSavedProperty, false).toBool()) {
    fail(error, QStringLiteral("보관한 도면 「%1」을 찾지 못했습니다.").arg(title));
    return nullptr;
  }
  QgsPrintLayout* copy = saved->clone();
  if (!copy) {
    fail(error, QStringLiteral("도면을 복사하지 못했습니다."));
    return nullptr;
  }
  copy->setName(workingName);
  copy->removeCustomProperty(kSavedProperty);
  copy->removeCustomProperty(kTitleProperty);
  LayoutService::applySingleRasterPassRendering(copy);
  // Reopening a stored sheet is an explicit composing choice by the user.
  LayoutService::markStudioSheetComposed(copy);
  // Park the old working sheet under another name so a failed add loses nothing.
  QgsPrintLayout* old = printLayout(project, workingName);
  if (old) old->setName(workingName + QStringLiteral(" (바꾸는 중)"));
  if (!project->layoutManager()->addLayout(copy)) {
    delete copy;
    if (old) old->setName(workingName);
    fail(error, QStringLiteral("도면을 열지 못했습니다."));
    return nullptr;
  }
  if (old) project->layoutManager()->removeLayout(old);
  project->setDirty(true);
  return copy;
}

bool remove(QgsProject* project, const QString& title) {
  QgsPrintLayout* saved = printLayout(project, layoutNameFor(title));
  if (!saved || !saved->customProperty(kSavedProperty, false).toBool()) return false;
  const bool removed = project->layoutManager()->removeLayout(saved);
  if (removed) project->setDirty(true);
  return removed;
}

void fillMenu(QMenu* menu, QgsProject* project, const MenuActions& actions) {
  if (!menu) return;
  menu->clear();
  menu->setToolTipsVisible(true);
  QAction* store = menu->addAction(QStringLiteral("지금 도면 보관…"));
  store->setToolTip(QStringLiteral("지금 용지를 이름 붙여 따로 둡니다. 레이어 켜짐도 같은 이름의 세트로 둡니다."));
  QObject::connect(store, &QAction::triggered, menu, [actions]() {
    if (actions.store) actions.store();
  });
  menu->addSeparator();
  const QVector<Entry> sheets = list(project);
  if (sheets.isEmpty()) {
    menu->addAction(QStringLiteral("보관한 도면이 없습니다"))->setEnabled(false);
  } else {
    for (const Entry& sheet : sheets) {
      const QString title = sheet.title;
      QAction* open = menu->addAction(QStringLiteral("열기: %1").arg(title));
      QObject::connect(open, &QAction::triggered, menu, [actions, title]() {
        if (actions.restore) actions.restore(title);
      });
    }
    QMenu* drop = menu->addMenu(QStringLiteral("보관한 도면 지우기"));
    for (const Entry& sheet : sheets) {
      const QString title = sheet.title;
      QObject::connect(drop->addAction(title), &QAction::triggered, menu, [actions, title]() {
        if (actions.remove) actions.remove(title);
      });
    }
  }
  menu->addSeparator();
  menu->addAction(QStringLiteral("제출 PDF는 지금 열린 도면으로 만듭니다"))->setEnabled(false);
}

}  // namespace KaDrawingSheetSet
