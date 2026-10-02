#pragma once
#include <QString>

#include <functional>

class QWidget;

// Field-facing error dialogs: what failed, why, and how to fix (optional action).
// One dialog per error, as before: details fold under Qt's 「자세한 내용」 button
// and 「로그 폴더 열기」 opens the log folder without closing the dialog.
namespace KaUserError {

struct Spec {
  QString title;
  QString what;
  QString why;
  QString how;
  QString actionLabel;  // empty → 닫기 only
  QString details;      // optional exception text or server reply, folded away
};

enum class Result { Dismissed, ActionChosen };

QString formatBody(const Spec& spec);
// The same three parts with bold subheadings; spec text is HTML-escaped.
QString formatBodyHtml(const Spec& spec);
Result warn(QWidget* parent, const Spec& spec);
Result critical(QWidget* parent, const Spec& spec);

// The folder 「로그 폴더 열기」 opens (session.log, crash logs). Empty hides the
// button. The app sets it once at startup from KaCrashGuard::logDir().
void setLogFolder(const QString& folder);
QString logFolder();
// Replaces how the folder is opened (QDesktopServices by default); tests use it
// so no Explorer window opens.
void setFolderOpener(std::function<bool(const QString& folder)> opener);

}  // namespace KaUserError
