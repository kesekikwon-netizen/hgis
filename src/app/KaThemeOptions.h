#pragma once

#include "KaTheme.h"

class QApplication;
class QMenu;
class QWidget;

// Opt-in 「화면 보기」 options: 고대비 화면 and 큰 글씨. The stock Strata look
// stays the default; nothing changes until the user ticks an option.
namespace KaTheme {

DisplayOptions savedDisplayOptions();
void saveDisplayOptions(const DisplayOptions& options);
// Call once at startup, after KaTheme::apply(): re-applies what the user chose.
void applySavedDisplayOptions(QApplication* app);
// The 「화면 보기」 submenu. Each tick saves the choice and restyles the app at once.
QMenu* createDisplayOptionsMenu(QWidget* parent);

}  // namespace KaTheme
