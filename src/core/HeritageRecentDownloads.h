#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>

#include "HeritageFetchPlan.h"
#include "HeritageStyle.h"

// Recent, already validated intranet downloads of one 시/군 (evaluation F174).
//
// The intranet builds each ZIP on request (40~120 s per dataset). When the user picks
// 「최근 받은 자료 다시 쓰기」 in the one confirm dialog, a dataset that was downloaded,
// checked and put on the map within the last days is loaded again from the local copy.
// Nothing is reused unless the user chose it. Unlimited retry of a bad download stays.
//
// The record lives next to the downloads (local AppData, never the survey or a portable
// package): <root>/recent.json, where <root> is <AppLocalData>/주변유적/<시도 시군>/원본.
namespace HeritageRecentDownloads {

constexpr int kMaxAgeDays = 30;

// Same place MainWindow downloads a 시/군 into.
QString defaultRoot(const HeritageCity& city);
QString manifestPath(const QString& root);

// Records the files of one dataset after they passed the ZIP/SHP check and map load.
bool record(const QString& root, HeritageDataset dataset, const QStringList& files,
            const QDateTime& when = QDateTime::currentDateTime());

// Files of a recent record of that dataset. Empty when there is none, it is older than
// maxAgeDays, or any file is gone or empty.
QStringList find(const QString& root, HeritageDataset dataset, int maxAgeDays = kMaxAgeDays,
                 const QDateTime& now = QDateTime::currentDateTime());

// Newest usable record under root. Invalid when nothing can be reused.
QDateTime latest(const QString& root, int maxAgeDays = kMaxAgeDays,
                 const QDateTime& now = QDateTime::currentDateTime());

}  // namespace HeritageRecentDownloads
