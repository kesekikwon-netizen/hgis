// GDAL failures must reach the session log, because a desktop launch has no
// console. Repeated failures fold into a count so the log does not overflow.
#include "app/KaCrashGuard.h"
#include "app/KaGdalErrorLog.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <cpl_error.h>

namespace {

QString sessionLog() {
  QFile log(QDir(KaCrashGuard::logDir()).filePath(QStringLiteral("session.log")));
  if (!log.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
  return QString::fromUtf8(log.readAll());
}

}  // namespace

class GdalErrorLogTest : public QObject {
  Q_OBJECT
private slots:
  void onlyFailuresAreRecorded() {
    QVERIFY(KaGdalErrorLog::describe(CE_Warning, 1, QStringLiteral("Field width truncated")).isEmpty());
    QVERIFY(KaGdalErrorLog::describe(CE_Debug, 0, QStringLiteral("debug")).isEmpty());
    QCOMPARE(KaGdalErrorLog::describe(CE_Failure, 1,
                                      QStringLiteral("In GetNextRawFeature(): unable to open database file")),
             QStringLiteral("[gdal] ERROR 1: In GetNextRawFeature(): unable to open database file"));
    QVERIFY(KaGdalErrorLog::describe(CE_Fatal, 3, QStringLiteral("x")).startsWith(QStringLiteral("[gdal] FATAL 3")));
  }

  void failuresReachTheSessionLogAndRepeatsFold() {
    KaGdalErrorLog::install();
    KaGdalErrorLog::install();  // a second install must not chain the handler to itself
    const char* flood = "In GetNextRawFeature(): sqlite3_step() : unable to open database file";
    CPLError(CE_Warning, CPLE_AppDefined, "%s", "warning noise");
    for (int i = 0; i < 120; ++i) CPLError(CE_Failure, CPLE_AppDefined, "%s", flood);
    CPLError(CE_Failure, CPLE_AppDefined, "%s", "a different failure");

    const QString text = sessionLog();
    QVERIFY2(text.contains(QStringLiteral("[gdal] ERROR 1: In GetNextRawFeature()")), qPrintable(text));
    QVERIFY(text.contains(QStringLiteral("a different failure")));
    QVERIFY(!text.contains(QStringLiteral("warning noise")));
    // 120 identical failures become one line plus repeat counts, not 120 lines.
    QCOMPARE(text.count(QStringLiteral("unable to open database file")), 1);
    QVERIFY(text.contains(QStringLiteral("반복")));
  }
};

int main(int argc, char** argv) {
  QTemporaryDir logs;
  if (!logs.isValid()) return 1;
  qputenv("KA_HGIS_LOG_DIR", logs.path().toUtf8());
  QCoreApplication app(argc, argv);
  GdalErrorLogTest test;
  return QTest::qExec(&test, argc, argv);
}

#include "test_gdal_error_log.moc"
