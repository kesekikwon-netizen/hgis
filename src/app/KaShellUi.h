#pragma once

#include <QString>

class QAction;
class QProgressDialog;
class QWidget;

// Small main-window helpers. They change only wording and enabled state that the user sees;
// they never add ribbon chips or change chip size, spacing or labels.
namespace KaShellUi {

// Tooltip line on record tools (선택·측거·그리기·시굴격자·버퍼) while no survey is open.
QString needSurveyReason();
// Tooltip line on the GeoTIFF chip while another tab than 지도 is showing.
QString mapTabOnlyReason();

// Enables or disables the target. While disabled, the reason is added to the tooltip on its
// own line; once enabled again, the original tooltip comes back unchanged. A tooltip set by
// other code while the reason was showing becomes the new original.
void setEnabledWithReason(QAction* action, bool enabled, const QString& reason);
void setEnabledWithReason(QWidget* widget, bool enabled, const QString& reason);

// Non-modal busy dialog for place search. Search is not a download, so the text says what is
// being looked for instead of reusing the download wording.
QProgressDialog* createSearchProgress(QWidget* parent, const QString& query);
QString searchProgressText(const QString& query);

}  // namespace KaShellUi
