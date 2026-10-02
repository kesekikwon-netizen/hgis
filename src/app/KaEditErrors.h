#pragma once

#include "KaUserError.h"

#include <QString>

class QWidget;

// Error dialogs of the drawing and editing flows. The body keeps the KaUserError
// three-part standard (무엇이 안 됐나 / 왜 그런가 / 어떻게 하나); raw technical text such
// as an exception message or a provider error goes only under the folded 「자세히」.
namespace KaEditErrors {

KaUserError::Result show(QWidget* parent, const KaUserError::Spec& spec,
                         const QString& details = QString(), bool critical = false);

// An unexpected failure (exception) while drawing or editing. The shapes already in the
// edit buffer stay; the user is told to save and continue.
void unexpected(QWidget* parent, const QString& title, const QString& what,
                const QString& details);

}  // namespace KaEditErrors
