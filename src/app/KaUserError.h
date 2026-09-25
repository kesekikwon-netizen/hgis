#pragma once
#include <QString>

class QWidget;

// Field-facing error dialogs: what failed, why, and how to fix (optional action).
namespace KaUserError {

struct Spec {
  QString title;
  QString what;
  QString why;
  QString how;
  QString actionLabel;  // empty → 닫기 only
};

enum class Result { Dismissed, ActionChosen };

QString formatBody(const Spec& spec);
Result warn(QWidget* parent, const Spec& spec);
Result critical(QWidget* parent, const Spec& spec);

}  // namespace KaUserError
