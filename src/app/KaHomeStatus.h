#pragma once

#include "core/AccountStatus.h"

#include <QList>
#include <QString>
#include <QVector>

// Local readiness of this PC, shown quietly on the home page (KaHomeConnectionCard).
//
// Everything here reads settings and local files only: no network call runs at startup and
// no key, ID or password value is ever returned or shown (설정됨 / 미설정 only). The internet
// check runs only when the user presses its button.
namespace KaHomeStatus {

enum class Folder { None, Writable, ReadOnly, Missing, Network };

struct Inputs {
  QList<AccountStatus::Entry> accounts;
  bool crsDatabase = false;
  QString folderPath;
  Folder folder = Folder::None;
};

struct Line {
  QString text;  // 「VWorld API 키 · 설정됨」
  QString hint;  // where to fix it (menu path, folder); empty when nothing to do
  bool ok = false;
};

Inputs collectLocal();
// Folder where the next survey is created by default (same order as MainWindow).
QString defaultSurveyFolder();
Folder folderState(const QString& directory);
// EPSG:5186/5187/5179 resolve through proj.db.
bool crsDatabaseReady();
QVector<Line> describe(const Inputs& inputs);

}  // namespace KaHomeStatus
