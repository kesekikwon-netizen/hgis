#pragma once
// 시험이 Qt 폴더 창(QFileDialog::getExistingDirectory)에서 폴더를 고르는 방법. 「새 조사」 시험들이 같이 쓴다.
#include <QDebug>
#include <QFileDialog>
#include <QFileInfo>
#include <QLineEdit>
#include <QStringList>
#include <QTest>

namespace FolderDialogFixture {

// 느린 CI처럼 폴더 창이 활성이고 이름 칸에 초점이 있게 만든다. 창을 활성으로 못 만드는 PC도 있어
// (2026-10-03 다른 작업 폴더) 결과는 기록만 한다. chooseFolder는 초점과 상관없이 맞아야 한다.
inline void focusNameEdit(QFileDialog* folder) {
  auto* name = folder->findChild<QLineEdit*>(QStringLiteral("fileNameEdit"));
  folder->activateWindow();
  if (QTest::qWaitForWindowActive(folder, 2000) && name) name->setFocus();
  qInfo() << "Folder dialog name edit focused:" << (name && name->hasFocus());
}

// dir을 골라 누르고, 고른 폴더가 dir이 아니면 취소한다. selectFile(전체 경로)는 창을 위 폴더로
// 옮기고, 이름 칸에 초점이 있으면 이름 칸을 비워 둬 위 폴더가 골라진다(CI 2026-10-03 e252ac3).
// 그래서 위 폴더로 가서 이름 칸을 직접 채운다.
inline bool chooseFolder(QFileDialog* folder, const QString& dir, QStringList* chosen = nullptr) {
  const QFileInfo desired(dir);
  folder->setDirectory(desired.absolutePath());
  folder->selectFile(desired.fileName());
  if (auto* name = folder->findChild<QLineEdit*>(QStringLiteral("fileNameEdit")))
    name->setText(desired.fileName());
  const QStringList files = folder->selectedFiles();
  if (chosen) *chosen = files;
  const bool ok = files.size() == 1 &&
      QFileInfo(files.first()).canonicalFilePath() == desired.canonicalFilePath();
  qInfo() << "New survey selected folders:" << files << "matches fixture:" << ok;
  QMetaObject::invokeMethod(folder, ok ? "accept" : "reject", Qt::DirectConnection);
  return ok;
}

}  // namespace FolderDialogFixture
