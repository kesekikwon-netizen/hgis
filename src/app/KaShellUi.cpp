#include "KaShellUi.h"

#include <QAction>
#include <QProgressDialog>
#include <QVariant>
#include <QWidget>

namespace KaShellUi {
namespace {

constexpr char kBaseTip[] = "kaShellBaseToolTip";
constexpr char kReasonTip[] = "kaShellReasonToolTip";

QString composed(const QString& base, const QString& reason) {
  return base.isEmpty() ? reason : base + QLatin1Char('\n') + reason;
}

// T is QAction or QWidget: both have toolTip/setToolTip/setEnabled and dynamic properties.
template <typename T>
void applyReason(T* target, bool enabled, const QString& reason) {
  if (!target) return;
  const QString shownReason = target->property(kReasonTip).toString();
  QString base = target->toolTip();
  if (!shownReason.isEmpty()) {
    const QString storedBase = target->property(kBaseTip).toString();
    // Keep a tooltip that other code set meanwhile; otherwise strip our own line again.
    if (base == composed(storedBase, shownReason)) base = storedBase;
  }
  const QString line = reason.trimmed();
  if (enabled || line.isEmpty()) {
    if (!shownReason.isEmpty()) target->setToolTip(base);
    target->setProperty(kBaseTip, QVariant());
    target->setProperty(kReasonTip, QVariant());
  } else {
    target->setProperty(kBaseTip, base);
    target->setProperty(kReasonTip, line);
    target->setToolTip(composed(base, line));
  }
  target->setEnabled(enabled);
}

}  // namespace

QString needSurveyReason() {
  return QStringLiteral("먼저 「새 조사」를 만들거나 「열기」로 조사를 여세요.");
}

QString mapTabOnlyReason() {
  return QStringLiteral("「지도」 탭에서 쓸 수 있습니다. 지도 탭으로 옮긴 뒤 누르세요.");
}

void setEnabledWithReason(QAction* action, bool enabled, const QString& reason) {
  applyReason(action, enabled, reason);
}

void setEnabledWithReason(QWidget* widget, bool enabled, const QString& reason) {
  applyReason(widget, enabled, reason);
}

QString searchProgressText(const QString& query) {
  QString shown = query.simplified();
  if (shown.size() > 30) shown = shown.left(29) + QChar(0x2026);
  return shown.isEmpty()
             ? QStringLiteral("위치를 찾는 중입니다. 지도 작업을 계속할 수 있습니다.")
             : QStringLiteral("「%1」 위치를 찾는 중입니다. 지도 작업을 계속할 수 있습니다.").arg(shown);
}

QProgressDialog* createSearchProgress(QWidget* parent, const QString& query) {
  auto* dialog = new QProgressDialog(searchProgressText(query), QStringLiteral("취소"), 0, 0, parent);
  dialog->setObjectName(QStringLiteral("locationSearchProgress"));
  dialog->setWindowTitle(QStringLiteral("위치 검색"));
  dialog->setWindowModality(Qt::NonModal);
  dialog->setMinimumDuration(0);
  dialog->setAutoClose(false);
  dialog->setAutoReset(false);
  return dialog;
}

}  // namespace KaShellUi
