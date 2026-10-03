#include "CadPendingCopies.h"

#include "FileCleanup.h"
#include "KaSessionLog.h"
#include "SurveyBundle.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>

#include <qgsmaplayer.h>
#include <qgsproject.h>
#include <qgsproviderregistry.h>

namespace CadPendingCopies {
namespace {

// 두 앱 창이 함께 써도 서로의 기록을 덮지 않게 파일마다 따로 적는다(QSettings 는 키 단위로 합친다).
QString ledgerFile() {
  return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + QStringLiteral("/cad-copies.ini");
}

QString clean(const QString& path) { return QFileInfo(path).absoluteFilePath(); }

// 만든 시각: 지운 변환본 자리에 나중에 들어온 다른 파일은 시각이 달라 지우지 않는다.
QString bornOf(const QString& path) {
  const QDateTime born = QFileInfo(path).birthTime();
  return born.isValid() ? QString::number(born.toMSecsSinceEpoch()) : QString();
}

QString keyOf(const QString& path) {
  return QStringLiteral("pending/") +
         QString::fromLatin1(QCryptographicHash::hash(clean(path).toLower().toUtf8(), QCryptographicHash::Sha1).toHex());
}

QStringList usedFiles(const QgsProject* project) {
  QStringList used;
  for (const QgsMapLayer* layer : project->mapLayers())
    used << clean(QgsProviderRegistry::instance()
                      ->decodeUri(layer->providerType(), layer->source())
                      .value(QStringLiteral("path"))
                      .toString());
  return used;
}

}  // namespace

void add(const QString& copyPath) {
  QSettings(ledgerFile(), QSettings::IniFormat)
      .setValue(keyOf(copyPath), QStringList{clean(copyPath), bornOf(copyPath)});
}

bool isPending(const QString& copyPath) {
  return QSettings(ledgerFile(), QSettings::IniFormat).contains(keyOf(copyPath));
}

void markSaved(const QgsProject* project) {
  if (!project) return;
  QSettings::Status status = QSettings::NoError;
  {
    QSettings ledger(ledgerFile(), QSettings::IniFormat);
    for (const QString& file : usedFiles(project)) ledger.remove(keyOf(file));
    ledger.sync();
    status = ledger.status();
  }
  if (status == QSettings::NoError) return;
  // 「저장됨」을 적지 못하면 목록을 버린다: 다음 실행에서 아무것도 지우지 않는 쪽.
  KaSessionLog::line(
      QStringLiteral("[cleanup] 도면 변환본 목록을 쓰지 못해 비웁니다 — %1").arg(QDir::toNativeSeparators(ledgerFile())));
  QFile::setPermissions(ledgerFile(), QFileDevice::ReadOwner | QFileDevice::WriteOwner);
  QFile::remove(ledgerFile());
}

int removeUnsaved(const QgsProject* project, const QString& surveyPath) {
  if (!project || surveyPath.isEmpty()) return 0;
  markSaved(project);  // 막 연 조사는 저장된 작업공간에서 읽었다: 그것이 쓰는 변환본은 목록에서 빠진다
  const QString folder =
      clean(QFileInfo(surveyPath).absoluteDir().filePath(SurveyBundle::collectedFolderName() + QStringLiteral("/도면")));
  QSettings ledger(ledgerFile(), QSettings::IniFormat);
  ledger.beginGroup(QStringLiteral("pending"));
  const QStringList keys = ledger.childKeys();
  ledger.endGroup();
  int removed = 0;
  for (const QString& key : keys) {
    const QStringList entry = ledger.value(QStringLiteral("pending/") + key).toStringList();
    const QString file = entry.value(0);
    if (QFileInfo(file).absolutePath().compare(folder, Qt::CaseInsensitive) != 0) continue;
    const bool ours = QFileInfo::exists(file) && !entry.value(1).isEmpty() && bornOf(file) == entry.value(1);
    if (ours && !FileCleanup::removeIfFree(file)) continue;  // 열려 있으면 다음에 다시 본다
    ledger.remove(QStringLiteral("pending/") + key);  // 지웠거나, 없거나, 그 자리의 다른 파일이다
    if (!ours) continue;
    KaSessionLog::line(
        QStringLiteral("[cleanup] 저장된 적 없는 도면 변환본을 지웠습니다 — %1").arg(QDir::toNativeSeparators(file)));
    ++removed;
  }
  return removed;
}

}  // namespace CadPendingCopies
