// When a save cannot replace the survey file, the session log says why: who holds it (this app or
// another program, by exe name), that nobody does, or that it could not tell, plus the read-only flag.
// A field report (「이름-저장.gpkg」 next to the survey) then carries usable evidence.
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "core/KaSessionLog.h"
#include "core/SurveyDurability.h"
#include "core/SurveyProjectFactory.h"
#include "core/SurveyStorage.h"
#include <qgsapplication.h>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

#ifdef Q_OS_WIN
// Opens the file the way SQLite does (read and write sharing, no delete sharing), so a rename
// onto it is refused while the handle lives.
HANDLE holdOpen(const QString& path) {
  return CreateFileW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(path).utf16()), GENERIC_READ,
                     FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
}
#endif

QString sessionLogText() {
  QFile log(QDir(KaSessionLog::dir()).filePath(QStringLiteral("session.log")));
  return log.open(QIODevice::ReadOnly) ? QString::fromUtf8(log.readAll()) : QString();
}

// "(pid 1234)" or "(pid 1234 · 이 앱)", never the start of another number such as "(pid 12345)".
bool namesThisProcess(const QString& text) {
  const QString pid = QString::number(QCoreApplication::applicationPid());
  return text.contains(QStringLiteral("(pid %1)").arg(pid)) || text.contains(QStringLiteral("(pid %1 ").arg(pid));
}

QStringList entriesForThisProcess(const QStringList& holders) {
  QStringList mine;
  for (const QString& entry : holders)
    if (namesThisProcess(entry)) mine << entry;
  return mine;
}

bool writeFile(const QString& path) {
  QFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write("x") == 1;
}

}  // namespace

class TestSurveyDurability : public QObject {
  Q_OBJECT
 private slots:
  void processesHolding_namesThisAppWhileItHoldsTheFile() {
#ifdef Q_OS_WIN
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("잡힌 조사.gpkg"));
    QVERIFY(writeFile(path));
    QVERIFY(entriesForThisProcess(SurveyDurability::processesHolding(path)).isEmpty());
    HANDLE handle = holdOpen(path);
    QVERIFY(handle != INVALID_HANDLE_VALUE);
    const QStringList mine = entriesForThisProcess(SurveyDurability::processesHolding(path));
    CloseHandle(handle);
    const QString exe = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
    QVERIFY2(mine.size() == 1 && mine.first().contains(QStringLiteral("이 앱")) &&
                 mine.first().contains(exe, Qt::CaseInsensitive),
             qPrintable(mine.join(QStringLiteral(" / "))));
    QVERIFY(entriesForThisProcess(SurveyDurability::processesHolding(path)).isEmpty());
#else
    QSKIP("Windows Restart Manager");
#endif
  }

  void copySurvey_renameRefused_logsWhoHoldsTheFile() {
#ifdef Q_OS_WIN
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString error;
    const QString source = SurveyProjectFactory::createNewSurvey(dir.path(), QStringLiteral("새세대"), &error,
                                                                 QStringLiteral("EPSG:5186"));
    const QString target = SurveyProjectFactory::createNewSurvey(dir.path(), QStringLiteral("현장조사"), &error,
                                                                 QStringLiteral("EPSG:5186"));
    QVERIFY2(!source.isEmpty() && !target.isEmpty(), qPrintable(error));
    HANDLE handle = holdOpen(target);
    QVERIFY(handle != INVALID_HANDLE_VALUE);
    QString written;
    const bool copied = SurveyStorage::copySurvey(source, target, &error, &written);
    CloseHandle(handle);
    QVERIFY2(copied, qPrintable(error));
    QVERIFY(written.contains(QStringLiteral("-저장")));
    const QString log = sessionLogText();
    const QString line = QStringLiteral("[save] 원본 교체 거부 진단 — ");
    QVERIFY2(log.contains(line), "옆 파일로 저장했는데 원본 교체가 막힌 원인을 기록하지 않았습니다.");
    const QString tail = log.mid(log.lastIndexOf(line)).section(QLatin1Char('\n'), 0, 0);
    QVERIFY2(tail.contains(QStringLiteral("이 앱")) && tail.contains(QStringLiteral("읽기 전용 아님")), qPrintable(tail));
#else
    QSKIP("Windows sharing-lock contract");
#endif
  }

  // Restart Manager refuses paths of 260 characters or more: that must read as "could not tell",
  // never as "nobody holds it".
  void lockDiagnosis_saysWhenItCannotTell() {
    const QString longPath = QDir::temp().filePath(QString(300, QLatin1Char('a')) + QStringLiteral(".gpkg"));
    const QString diagnosis = SurveyDurability::lockDiagnosis(longPath);
    QVERIFY2(diagnosis.contains(QStringLiteral("확인하지 못함")), qPrintable(diagnosis));
    QVERIFY2(diagnosis.contains(QStringLiteral("Restart Manager 등록 오류")), qPrintable(diagnosis));
    QVERIFY2(!diagnosis.contains(QStringLiteral("열고 있는 프로그램 없음")), qPrintable(diagnosis));
    // The read-only flag cannot be read either: say so instead of leaving it out.
    QVERIFY2(diagnosis.contains(QStringLiteral("속성 확인 못 함")), qPrintable(diagnosis));
  }

  // A read-only target refuses the rename with the same "access denied" while nobody holds it.
  void lockDiagnosis_namesAReadOnlyTarget() {
#ifdef Q_OS_WIN
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("읽기전용 조사.gpkg"));
    QVERIFY(writeFile(path));
    const QString nativePath = QDir::toNativeSeparators(path);
    const auto native = reinterpret_cast<LPCWSTR>(nativePath.utf16());
    QVERIFY(SetFileAttributesW(native, FILE_ATTRIBUTE_READONLY));
    const QString diagnosis = SurveyDurability::lockDiagnosis(path);
    SetFileAttributesW(native, FILE_ATTRIBUTE_NORMAL);
    QVERIFY2(diagnosis.contains(QStringLiteral("· 읽기 전용")) && !diagnosis.contains(QStringLiteral("읽기 전용 아님")),
             qPrintable(diagnosis));
    QVERIFY2(diagnosis.contains(QStringLiteral("열고 있는 프로그램 없음")) && !namesThisProcess(diagnosis),
             qPrintable(diagnosis));
#else
    QSKIP("Windows file attributes");
#endif
  }
};

int main(int argc, char** argv) {
  QStandardPaths::setTestModeEnabled(true);
  QTemporaryDir isolated;
  if (!isolated.isValid()) return 1;
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, isolated.filePath(QStringLiteral("settings")));
  qputenv("KA_HGIS_LOG_DIR", QDir::toNativeSeparators(isolated.filePath(QStringLiteral("logs"))).toLocal8Bit());
  qputenv("LOCALAPPDATA", QDir::toNativeSeparators(isolated.filePath(QStringLiteral("local"))).toLocal8Bit());
  QgsApplication app(argc, argv, true);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  TestSurveyDurability tests;
  const int result = QTest::qExec(&tests, argc, argv);
  QgsApplication::exitQgis();
  return result;
}

#include "test_survey_durability.moc"
