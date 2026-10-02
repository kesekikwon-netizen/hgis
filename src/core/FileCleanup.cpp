#include "FileCleanup.h"

#include "KaSessionLog.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTimer>

#include <qgsogrproviderutils.h>

namespace FileCleanup {
namespace {

bool removeNow(const QString& path) {
  QgsOgrProviderUtils::invalidateCachedDatasets(path);
  return QFile::remove(path) || !QFileInfo::exists(path);
}

}  // namespace

bool removeWhenFree(const QString& path) {
  const QString clean = QFileInfo(path).absoluteFilePath();
  if (removeNow(clean)) return true;
  KaSessionLog::line(QStringLiteral("[cleanup] 아직 열려 있어 잠시 뒤 다시 지운다 — %1").arg(QDir::toNativeSeparators(clean)));
  QTimer::singleShot(5000, QCoreApplication::instance(), [clean] {
    if (!removeNow(clean)) QTimer::singleShot(55000, QCoreApplication::instance(), [clean] { removeNow(clean); });
  });
  return false;
}

}  // namespace FileCleanup
