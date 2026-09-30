#include "KaEditErrors.h"

#include <QMessageBox>
#include <QPushButton>

namespace KaEditErrors {

KaUserError::Result show(QWidget* parent, const KaUserError::Spec& spec, const QString& details,
                         bool critical) {
  QMessageBox box(critical ? QMessageBox::Critical : QMessageBox::Warning, spec.title,
                  KaUserError::formatBody(spec), QMessageBox::NoButton, parent);
  if (!details.trimmed().isEmpty()) box.setDetailedText(details.trimmed());
  QPushButton* action = nullptr;
  if (!spec.actionLabel.trimmed().isEmpty())
    action = box.addButton(spec.actionLabel.trimmed(), QMessageBox::AcceptRole);
  box.addButton(QStringLiteral("닫기"), QMessageBox::RejectRole);
  box.exec();
  return action && box.clickedButton() == action ? KaUserError::Result::ActionChosen
                                                 : KaUserError::Result::Dismissed;
}

void unexpected(QWidget* parent, const QString& title, const QString& what,
                const QString& details) {
  show(parent,
       {title, what,
        QStringLiteral("프로그램 안에서 예상하지 못한 문제가 생겼습니다. 이미 그린 도형은 그대로 있습니다."),
        QStringLiteral("「저장」(Ctrl+S)으로 지금까지의 작업을 저장한 뒤 다시 해 보세요. "
                       "같은 일이 되풀이되면 「자세히」 내용을 알려 주세요.")},
       details, true);
}

}  // namespace KaEditErrors
