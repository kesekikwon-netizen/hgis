#pragma once
#include "KaTheme.h"

// 새 모양 (2026-10-03, docs/intent/2026-10-03-ui-redesign-dialogs-color-motion.md): paper ground,
// slate ink and one clay accent, chosen in 「화면 보기」. The stock Strata tokens stay as they are;
// this look only rewrites a copy of them.
namespace KaTheme {

// Turns t (the stock or the high-contrast Strata tokens) into the paper look. The caller
// refreshes the legacy aliases afterwards.
void applyPaperLook(Tokens& t, bool highContrast);

}  // namespace KaTheme
