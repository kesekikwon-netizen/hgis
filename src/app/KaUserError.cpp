#include "KaUserError.h"
#include <QAbstractButton>
#include <QDesktopServices>
#include <QDir>
#include <QMessageBox>
#include <QPushButton>
#include <QUrl>

namespace KaUserError {
namespace {

QString& logFolderStorage() {
  static QString folder;
  return folder;
}

std::function<bool(const QString&)>& folderOpener() {
  static std::function<bool(const QString&)> opener = [](const QString& folder) {
    QDir().mkpath(folder);
    return QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
  };
  return opener;
}

QString htmlParagraph(const QString& text) {
  QString escaped = text.trimmed().toHtmlEscaped();
  escaped.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
  return escaped;
}

}  // namespace

QString formatBody(const Spec& spec) {
  QStringList parts;
  parts << QStringLiteral("무엇이 안 됐나") << spec.what.trimmed();
  if (!spec.why.trimmed().isEmpty())
    parts << QString() << QStringLiteral("왜 그런가") << spec.why.trimmed();
  if (!spec.how.trimmed().isEmpty())
    parts << QString() << QStringLiteral("어떻게 하나") << spec.how.trimmed();
  return parts.join(QLatin1Char('\n'));
}

QString formatBodyHtml(const Spec& spec) {
  QString html;
  const auto section = [&html](const QString& heading, const QString& text, bool always) {
    if (!always && text.trimmed().isEmpty()) return;
    html += QStringLiteral("<p style=\"margin:0 0 10px 0\"><b>%1</b><br>%2</p>")
                .arg(heading, htmlParagraph(text));
  };
  section(QStringLiteral("무엇이 안 됐나"), spec.what, true);
  section(QStringLiteral("왜 그런가"), spec.why, false);
  section(QStringLiteral("어떻게 하나"), spec.how, false);
  return html;
}

void setLogFolder(const QString& folder) { logFolderStorage() = folder.trimmed(); }

QString logFolder() { return logFolderStorage(); }

void setFolderOpener(std::function<bool(const QString&)> opener) {
  if (opener) folderOpener() = std::move(opener);
}

static Result show(QWidget* parent, QMessageBox::Icon icon, const Spec& spec) {
  QMessageBox box(icon, spec.title, QString(), QMessageBox::NoButton, parent);
  box.setObjectName(QStringLiteral("kaUserError"));
  box.setTextFormat(Qt::RichText);
  box.setText(formatBodyHtml(spec));
  if (!spec.details.trimmed().isEmpty())
    box.setDetailedText(spec.details.trimmed());
  QPushButton* action = nullptr;
  if (!spec.actionLabel.trimmed().isEmpty())
    action = box.addButton(spec.actionLabel.trimmed(), QMessageBox::AcceptRole);
  const QString folder = logFolder();
  if (!folder.isEmpty()) {
    QPushButton* logs = box.addButton(QStringLiteral("로그 폴더 열기"), QMessageBox::ActionRole);
    logs->setObjectName(QStringLiteral("kaUserErrorOpenLogs"));
    logs->setToolTip(QStringLiteral("개발자에게 보낼 session.log가 있는 폴더를 엽니다: %1")
                         .arg(QDir::toNativeSeparators(folder)));
    // Every QMessageBox button closes the box; this one only opens the folder.
    QObject::disconnect(logs, &QAbstractButton::clicked, nullptr, nullptr);
    QObject::connect(logs, &QAbstractButton::clicked, &box, [folder]() { folderOpener()(folder); });
  }
  QPushButton* close = box.addButton(QStringLiteral("닫기"), QMessageBox::RejectRole);
  // Enter runs the offered fix when there is one; Esc and the title-bar close always dismiss.
  box.setDefaultButton(action ? action : close);
  box.setEscapeButton(close);
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
