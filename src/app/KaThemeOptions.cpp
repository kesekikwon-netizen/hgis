#include "KaThemeOptions.h"

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QSettings>

namespace KaTheme {
namespace {

const QString kHighContrastKey = QStringLiteral("ui/highContrast");
const QString kLargeTextKey = QStringLiteral("ui/largeText");

QApplication* application() {
  return qobject_cast<QApplication*>(QCoreApplication::instance());
}

}  // namespace

DisplayOptions savedDisplayOptions() {
  QSettings settings;
  DisplayOptions options;
  options.highContrast = settings.value(kHighContrastKey, false).toBool();
  options.largeText = settings.value(kLargeTextKey, false).toBool();
  return options;
}

void saveDisplayOptions(const DisplayOptions& options) {
  QSettings settings;
  settings.setValue(kHighContrastKey, options.highContrast);
  settings.setValue(kLargeTextKey, options.largeText);
}

void applySavedDisplayOptions(QApplication* app) {
  const DisplayOptions options = savedDisplayOptions();
  if (options == displayOptions())
    return;  // stock look: keep the sheet apply() installed
  setDisplayOptions(app, options);
}

QMenu* createDisplayOptionsMenu(QWidget* parent) {
  auto* menu = new QMenu(QStringLiteral("화면 보기"), parent);
  menu->setObjectName(QStringLiteral("displayOptionsMenu"));
  menu->setToolTipsVisible(true);
  auto* contrast = menu->addAction(QStringLiteral("고대비 화면 (햇빛 아래)"));
  contrast->setObjectName(QStringLiteral("actionHighContrast"));
  contrast->setCheckable(true);
  contrast->setToolTip(QStringLiteral("패널 경계선과 보조 글자를 진하게 합니다. 끄면 기본 화면으로 돌아갑니다"));
  auto* large = menu->addAction(QStringLiteral("큰 글씨"));
  large->setObjectName(QStringLiteral("actionLargeText"));
  large->setCheckable(true);
  large->setToolTip(QStringLiteral("본문 글자를 13 px에서 15 px로 키웁니다. 리본 칸과 레이어 목록 크기는 그대로입니다"));
  const auto sync = [contrast, large]() {
    const DisplayOptions& now = displayOptions();
    contrast->setChecked(now.highContrast);
    large->setChecked(now.largeText);
  };
  sync();
  QObject::connect(menu, &QMenu::aboutToShow, menu, sync);
  const auto choose = [contrast, large]() {
    DisplayOptions options;
    options.highContrast = contrast->isChecked();
    options.largeText = large->isChecked();
    saveDisplayOptions(options);
    setDisplayOptions(application(), options);
  };
  QObject::connect(contrast, &QAction::triggered, menu, choose);
  QObject::connect(large, &QAction::triggered, menu, choose);
  return menu;
}

}  // namespace KaTheme
