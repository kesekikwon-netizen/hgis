#include "KaUserError.h"
#include <QAbstractButton>
#include <QMessageBox>
#include <QPushButton>

namespace KaUserError {

QString formatBody(const Spec& spec) {
  QStringList parts;
  parts << QStringLiteral("무엇이 안 됐나") << spec.what.trimmed();
  if (!spec.why.trimmed().isEmpty())
    parts << QString() << QStringLiteral("왜 그런가") << spec.why.trimmed();
  if (!spec.how.trimmed().isEmpty())
    parts << QString() << QStringLiteral("어떻게 하나") << spec.how.trimmed();
  return parts.join(QLatin1Char('\n'));
}

static Result show(QWidget* parent, QMessageBox::Icon icon, const Spec& spec) {
  QMessageBox box(icon, spec.title, formatBody(spec), QMessageBox::NoButton, parent);
  QPushButton* action = nullptr;
  if (!spec.actionLabel.trimmed().isEmpty())
    action = box.addButton(spec.actionLabel.trimmed(), QMessageBox::AcceptRole);
  box.addButton(QStringLiteral("닫기"), QMessageBox::RejectRole);
  box.exec();
  if (action && box.clickedButton() == action) return Result::ActionChosen;
  return Result::Dismissed;
}

Result warn(QWidget* parent, const Spec& spec) {
  return show(parent, QMessageBox::Warning, spec);
}

Result critical(QWidget* parent, const Spec& spec) {
  return show(parent, QMessageBox::Critical, spec);
}

}  // namespace KaUserError
