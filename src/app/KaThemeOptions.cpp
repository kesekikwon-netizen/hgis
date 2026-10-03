#include "KaThemeOptions.h"

#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QMenu>
#include <QSettings>
#include <QWidget>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif

namespace KaTheme {
namespace {

const QString kHighContrastKey = QStringLiteral("ui/highContrast");
const QString kLargeTextKey = QStringLiteral("ui/largeText");
const QString kPaperLookKey = QStringLiteral("ui/paperLook");

QApplication* application() {
  return qobject_cast<QApplication*>(QCoreApplication::instance());
}

// Windows 11 paints a window's caption in the colour the app asks for. 새 모양 asks for the paper
// chrome with slate text on every top-level window; the stock look hands the caption back to Windows.
class CaptionTint final : public QObject {
public:
  using QObject::QObject;

protected:
  bool eventFilter(QObject* watched, QEvent* event) override {
    if (event->type() == QEvent::Show || event->type() == QEvent::PaletteChange)
      if (auto* window = qobject_cast<QWidget*>(watched); window && window->isWindow())
        tint(window);
    return false;
  }

private:
  static void tint(QWidget* window) {
#ifdef Q_OS_WIN
    const HWND handle = reinterpret_cast<HWND>(window->internalWinId());  // 0 until the window exists
    if (!handle)
      return;
    const bool paper = displayOptions().paperLook;
    const auto ref = [](const QColor& c) { return COLORREF(RGB(c.red(), c.green(), c.blue())); };
    const COLORREF face = paper ? ref(tokens().chrome) : COLORREF(0xFFFFFFFF);  // DWMWA_COLOR_DEFAULT
    const COLORREF text = paper ? ref(tokens().ink) : COLORREF(0xFFFFFFFF);
    DwmSetWindowAttribute(handle, 35 /* DWMWA_CAPTION_COLOR */, &face, sizeof(face));
    DwmSetWindowAttribute(handle, 36 /* DWMWA_TEXT_COLOR */, &text, sizeof(text));
#else
    Q_UNUSED(window);
#endif
  }
};

}  // namespace

DisplayOptions savedDisplayOptions() {
  QSettings settings;
  DisplayOptions options;
  options.highContrast = settings.value(kHighContrastKey, false).toBool();
  options.largeText = settings.value(kLargeTextKey, false).toBool();
  options.paperLook = settings.value(kPaperLookKey, false).toBool();
  return options;
}

void saveDisplayOptions(const DisplayOptions& options) {
  QSettings settings;
  settings.setValue(kHighContrastKey, options.highContrast);
  settings.setValue(kLargeTextKey, options.largeText);
  settings.setValue(kPaperLookKey, options.paperLook);
}

void applySavedDisplayOptions(QApplication* app) {
  DisplayOptions options = savedDisplayOptions();
  // 새 모양 is the look on trial (docs/intent/2026-10-03-ui-redesign-dialogs-color-motion.md): the app
  // starts in it until the user has ticked or unticked 「새 모양」 once.
  if (!QSettings().contains(kPaperLookKey))
    options.paperLook = true;
  if (app && !app->findChild<QObject*>(QStringLiteral("kaCaptionTint"), Qt::FindDirectChildrenOnly)) {
    auto* tint = new CaptionTint(app);
    tint->setObjectName(QStringLiteral("kaCaptionTint"));
    app->installEventFilter(tint);
  }
  if (options == displayOptions())
    return;  // stock look: keep the sheet apply() installed
  setDisplayOptions(app, options);
}

QMenu* createDisplayOptionsMenu(QWidget* parent) {
  auto* menu = new QMenu(QStringLiteral("화면 보기"), parent);
  menu->setObjectName(QStringLiteral("displayOptionsMenu"));
  menu->setToolTipsVisible(true);
  auto* paper = menu->addAction(QStringLiteral("새 모양 (종이·흙색)"));
  paper->setObjectName(QStringLiteral("actionPaperLook"));
  paper->setCheckable(true);
  paper->setToolTip(QStringLiteral("종이색 바탕에 먹색 글자와 흙색 강조를 씁니다. 끄면 이전 모양으로 돌아갑니다"));
  auto* contrast = menu->addAction(QStringLiteral("고대비 화면 (햇빛 아래)"));
  contrast->setObjectName(QStringLiteral("actionHighContrast"));
  contrast->setCheckable(true);
  contrast->setToolTip(QStringLiteral("패널 경계선과 보조 글자를 진하게 합니다. 끄면 기본 화면으로 돌아갑니다"));
  auto* large = menu->addAction(QStringLiteral("큰 글씨"));
  large->setObjectName(QStringLiteral("actionLargeText"));
  large->setCheckable(true);
  large->setToolTip(QStringLiteral("본문 글자를 13 px에서 15 px로 키웁니다. 리본 칸과 레이어 목록 크기는 그대로입니다"));
  const auto sync = [paper, contrast, large]() {
    const DisplayOptions& now = displayOptions();
    paper->setChecked(now.paperLook);
    contrast->setChecked(now.highContrast);
    large->setChecked(now.largeText);
  };
  sync();
  QObject::connect(menu, &QMenu::aboutToShow, menu, sync);
  const auto choose = [paper, contrast, large]() {
    DisplayOptions options;
    options.paperLook = paper->isChecked();
    options.highContrast = contrast->isChecked();
    options.largeText = large->isChecked();
    saveDisplayOptions(options);
    setDisplayOptions(application(), options);
  };
  QObject::connect(paper, &QAction::triggered, menu, choose);
  QObject::connect(contrast, &QAction::triggered, menu, choose);
  QObject::connect(large, &QAction::triggered, menu, choose);
  return menu;
}

}  // namespace KaTheme
