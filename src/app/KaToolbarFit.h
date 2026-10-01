#pragma once

class QToolBar;

// Keeps a tool bar on one row, never behind the » extension button. When its buttons do not fit,
// the labels hide first (Qt::ToolButtonIconOnly), then the icons shrink from 20 to 16 px; room that
// appears brings both back in the reverse order. The tool button style the bar has when install()
// runs is the first stage, so a bar set to text-beside-icon starts with its labels showing.
// Installing twice on one bar changes nothing the second time.
namespace KaToolbarFit {

void install(QToolBar* bar);

}  // namespace KaToolbarFit
