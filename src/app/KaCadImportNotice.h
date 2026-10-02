#pragma once

#include "KaCadImport.h"
#include "core/CadCrsGuess.h"

#include <QString>

class QgsMessageBar;

// 「도면을 … 로 읽어 … 로 바꿔 올렸습니다」와 [다른 좌표계로 바꾸기 ▾] [직접 맞추기]. 사용자가 닫을 때까지 둔다.
// 알림마다 도면 묶음 제목을 적어 두어, 같은 도면을 다시 올리면 지난 알림을 내린다.
namespace KaCadImport {

void dropNotices(QgsMessageBar* bar, const QString& title);
void showNotice(const Hooks& hooks, const QString& title, const QString& drawingId, const QString& sourcePath,
                const CadCrsResult& guess, const QString& usedAuthId, const QString& workAuthId);

}  // namespace KaCadImport
