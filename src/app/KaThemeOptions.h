#pragma once

#include "KaTheme.h"

class QApplication;
class QMenu;
class QWidget;

// 「화면 보기」 options: 새 모양, 고대비 화면 and 큰 글씨. KaTheme itself defaults to the stock Strata
// look; the app starts in 새 모양 (the look on trial) until the user ticks or unticks it once.
namespace KaTheme {

DisplayOptions savedDisplayOptions();
void saveDisplayOptions(const DisplayOptions& options);
// Call once at startup, after KaTheme::apply(): re-applies what the user chose.
void applySavedDisplayOptions(QApplication* app);
// The 「화면 보기」 submenu. Each tick saves the choice and restyles the app at once.
QMenu* createDisplayOptionsMenu(QWidget* parent);

}  // namespace KaTheme
