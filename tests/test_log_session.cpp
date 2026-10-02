// Session log behaviour (evaluation F029, F134, F206): keys and account values are masked
// at the single entry point, several rotations are kept, a portable bundle logs beside its
// own config, and KA_LOG_EXCEPT names the real file:line and the exception text.
// Qt Core only, like ka_catch_log_tests: links src/core/KaSessionLog.cpp directly.
#include "core/KaLogExcept.h"
#include "core/KaSessionLog.h"
#include "core/SecretMask.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtTest>

#include <stdexcept>

namespace {
QString readAll(const QString& path) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
  return QString::fromUtf8(f.readAll());
}

// A fresh log folder for one test. The logger keeps session.log open, which on Windows
// blocks QTemporaryDir's removal, so before the folder goes away the log is pointed back
// at the folder the harness gave us (or a scratch folder) and one line moves the handle.
struct ScopedLogDir {
  QTemporaryDir dir;
  QByteArray before = qgetenv("KA_HGIS_LOG_DIR");
  ScopedLogDir() { qputenv("KA_HGIS_LOG_DIR", dir.path().toLocal8Bit()); }
  ~ScopedLogDir() {
    const QByteArray parking =
        before.isEmpty() ? QDir(QDir::tempPath()).filePath(QStringLiteral("ka-hgis-tests/logs")).toLocal8Bit()
                         : before;
    qputenv("KA_HGIS_LOG_DIR", parking);
    KaSessionLog::line(QStringLiteral("[test] log folder released"));
  }
  bool isValid() const { return dir.isValid(); }
  QString path() const { return dir.path(); }
  QString filePath(const QString& name) const { return dir.filePath(name); }
};
}  // namespace

class TestLogSession : public QObject {
  Q_OBJECT
private slots:
  void sessionLog_keepsSeveralRotations();
  void sessionLog_headerNamesProcess();
  void sessionLog_masksKeysAndAccounts();
  void secretMask_keepsOrdinaryText();
  void portableBundle_logsBesideItsConfig();
  void logExcept_namesSourceAndMessage();
  void cleanup() {
    qunsetenv("KA_HGIS_LOG_MAX_BYTES");
    qunsetenv("KA_HGIS_LOG_KEEP");
  }
};

void TestLogSession::sessionLog_keepsSeveralRotations() {
  // One old file was lost after a few bursts of tile errors (F134). Keep three by default.
  ScopedLogDir tmp;
  QVERIFY(tmp.isValid());
  qputenv("KA_HGIS_LOG_MAX_BYTES", "1024");
  qunsetenv("KA_HGIS_LOG_KEEP");
  QCOMPARE(KaSessionLog::keepOldFiles(), KaSessionLog::kDefaultKeepOldFiles);
  QVERIFY(KaSessionLog::kDefaultKeepOldFiles > 1);
  QCOMPARE(KaSessionLog::rotatedFileName(1), QStringLiteral("session.old.log"));
  QCOMPARE(KaSessionLog::rotatedFileName(2), QStringLiteral("session.old.2.log"));

  for (int i = 0; i < 400; ++i)
    KaSessionLog::line(QStringLiteral("[keep-probe] %1 %2").arg(i).arg(QString(48, QLatin1Char('y'))));

  const QDir dir(tmp.path());
  for (int i = 1; i <= KaSessionLog::kDefaultKeepOldFiles; ++i) {
    const QString name = KaSessionLog::rotatedFileName(i);
    QVERIFY2(QFileInfo(dir.filePath(name)).size() > 0, qPrintable(name));
  }
  const QString beyond = KaSessionLog::rotatedFileName(KaSessionLog::kDefaultKeepOldFiles + 1);
  QVERIFY2(!QFile::exists(dir.filePath(beyond)), qPrintable(beyond));
  // The newest rotation holds later lines than the oldest kept one.
  const QRegularExpression probe(QStringLiteral(R"(\[keep-probe\] (\d+))"));
  const QString newest = readAll(dir.filePath(KaSessionLog::rotatedFileName(1)));
  const QString oldest =
      readAll(dir.filePath(KaSessionLog::rotatedFileName(KaSessionLog::kDefaultKeepOldFiles)));
  const QRegularExpressionMatch newestMatch = probe.match(newest);
  const QRegularExpressionMatch oldestMatch = probe.match(oldest);
  QVERIFY(newestMatch.hasMatch() && oldestMatch.hasMatch());
  QVERIFY2(newestMatch.captured(1).toInt() > oldestMatch.captured(1).toInt(),
           qPrintable(newestMatch.captured(1) + QStringLiteral(" <= ") + oldestMatch.captured(1)));

  qputenv("KA_HGIS_LOG_KEEP", "5");
  QCOMPARE(KaSessionLog::keepOldFiles(), 5);
  qputenv("KA_HGIS_LOG_KEEP", "0");
  QCOMPARE(KaSessionLog::keepOldFiles(), 1);
  qunsetenv("KA_HGIS_LOG_KEEP");
  qunsetenv("KA_HGIS_LOG_MAX_BYTES");
  const QString hint = KaSessionLog::dumpHint();
  QVERIFY2(hint.contains(QStringLiteral("%1개").arg(KaSessionLog::kDefaultKeepOldFiles)), qPrintable(hint));
}

void TestLogSession::sessionLog_headerNamesProcess() {
  // Lines from two runs, or from two PCs sharing a portable folder, must not read as one session.
  ScopedLogDir tmp;
  QVERIFY(tmp.isValid());
  KaSessionLog::line(QStringLiteral("[header-probe] first"));
  const QString text = readAll(tmp.filePath(QStringLiteral("session.log")));
  const QString first = text.section(QLatin1Char('\n'), 0, 0);
  QVERIFY2(first.contains(QStringLiteral("pid %1").arg(QCoreApplication::applicationPid())),
           qPrintable(first));
}

void TestLogSession::sessionLog_masksKeysAndAccounts() {
  // F029: QGIS provider warnings reach the log through KaCrashGuard::logLine -> line().
  ScopedLogDir tmp;
  QVERIFY(tmp.isValid());
  const QString key = QStringLiteral("0A1B2C3D-4E5F-6071-8293-A4B5C6D7E8F9");
  const QStringList lines{
      QStringLiteral("[qgis/WMS] Tile https://api.vworld.kr/req/wmts/1.0.0/") + key +
          QStringLiteral("/Satellite/12/1/2.jpeg failed"),
      QStringLiteral("[qgis/WMS] url=https%3A%2F%2Fapi.vworld.kr%2Freq%2Fwmts%2F1.0.0%2F") + key +
          QStringLiteral("%2FBase%2F"),
      QStringLiteral("[qgis/WMS] GetMap https://api.vworld.kr/req/wms?SERVICE=WMS&KEY=secretvalue77&LAYERS=lp_pa_cbnd_bubun"),
      QStringLiteral("[qgis/WMS] url=https://api.vworld.kr/req/wms?SERVICE%3DWMS%26KEY%3Dsecretvalue78%26LAYERS%3Dlp"),
      QStringLiteral("[history] https://hgis.history.go.kr/api?apiKey=secretvalue79&layer=map1919"),
      QStringLiteral("[intranet] login user=field password=secretvalue80"),
      QStringLiteral("[net] Authorization: Basic c2VjcmV0dmFsdWU4MTpwdw=="),
  };
  for (const QString& l : lines) KaSessionLog::line(l);
  const QString text = readAll(tmp.filePath(QStringLiteral("session.log")));
  QVERIFY2(!text.contains(key, Qt::CaseInsensitive), qPrintable(text));
  for (const char* secret : {"secretvalue77", "secretvalue78", "secretvalue79", "secretvalue80",
                             "c2VjcmV0dmFsdWU4MTpwdw"})
    QVERIFY2(!text.contains(QLatin1String(secret)), secret);
  // The rest of each line stays readable for diagnosis.
  QVERIFY2(text.contains(QStringLiteral("/Satellite/12/1/2.jpeg")), qPrintable(text));
  QVERIFY2(text.contains(QStringLiteral("%2FBase%2F")), qPrintable(text));
  QVERIFY2(text.contains(QStringLiteral("KEY=****&LAYERS=lp_pa_cbnd_bubun")), qPrintable(text));
  QVERIFY2(text.contains(QStringLiteral("KEY%3D****%26LAYERS%3Dlp")), qPrintable(text));
  QVERIFY2(text.contains(QStringLiteral("apiKey=****&layer=map1919")), qPrintable(text));
}

void TestLogSession::secretMask_keepsOrdinaryText() {
  const QString plain = QStringLiteral("[boot] 타일 캐시 512 MB — C:/Users/Public/ka-hgis/cache (2026-09-29)");
  QCOMPARE(SecretMask::mask(plain), plain);
  // App properties that merely end in "key" are not secrets.
  const QString prop = QStringLiteral("ka_hgis/layer_key=feature_poly reference_kind=dem");
  QCOMPARE(SecretMask::mask(prop), prop);
  QCOMPARE(SecretMask::mask(QStringLiteral("crtfc_key=abc123&x=1")), QStringLiteral("crtfc_key=****&x=1"));
  QCOMPARE(SecretMask::mask(QStringLiteral("serviceKey%3Dabc%26type%3Djson")),
           QStringLiteral("serviceKey%3D****%26type%3Djson"));
}

void TestLogSession::portableBundle_logsBesideItsConfig() {
  // F134: a portable copy used on several PCs keeps its logs with it.
  QTemporaryDir bundle;
  QVERIFY(bundle.isValid());
  QVERIFY(KaSessionLog::portableLogDirFor(bundle.path()).isEmpty());
  QVERIFY(KaSessionLog::portableLogDirFor(QString()).isEmpty());
  QVERIFY(QDir(bundle.path()).mkpath(QStringLiteral("apps/qgis-dev")));
  QCOMPARE(QDir::cleanPath(KaSessionLog::portableLogDirFor(bundle.path())),
           QDir::cleanPath(QDir(bundle.path()).absoluteFilePath(QStringLiteral("config/logs"))));
  // KA_HGIS_LOG_DIR still wins over both defaults. Nothing is logged into the bundle, so
  // the folder is removed cleanly; the previous folder is restored for the next test.
  const QByteArray before = qgetenv("KA_HGIS_LOG_DIR");
  qputenv("KA_HGIS_LOG_DIR", bundle.path().toLocal8Bit());
  QCOMPARE(QDir::cleanPath(KaSessionLog::dir()), QDir::cleanPath(QDir::fromNativeSeparators(bundle.path())));
  qputenv("KA_HGIS_LOG_DIR", before);
}

void TestLogSession::logExcept_namesSourceAndMessage() {
  // F206: the tag is generated, so it cannot drift from the handler that wrote it.
  ScopedLogDir tmp;
  QVERIFY(tmp.isValid());
  QString line;
  int expectedLine = 0;
  try {
    throw std::runtime_error("probe transform failed");
  } catch (...) {
    expectedLine = __LINE__ + 1;
    line = KaLogExceptDetail::exceptLine(__FILE__, __LINE__);
    KA_LOG_EXCEPT();
  }
  QVERIFY2(line.startsWith(QStringLiteral("[except] tests/test_log_session.cpp:%1").arg(expectedLine)),
           qPrintable(line));
  QVERIFY2(line.contains(QStringLiteral("probe transform failed")), qPrintable(line));
  // Outside a handler there is no exception text.
  QCOMPARE(KaLogExceptDetail::exceptLine("C:/x/src/core/Foo.cpp", 7),
           QStringLiteral("[except] core/Foo.cpp:7"));
  QCOMPARE(KaLogExceptDetail::exceptLine("src\\app\\Bar.cpp", 9), QStringLiteral("[except] app/Bar.cpp:9"));
  const QString text = readAll(tmp.filePath(QStringLiteral("session.log")));
  QVERIFY2(text.contains(QStringLiteral("[except] tests/test_log_session.cpp:%1 — probe transform failed")
                             .arg(expectedLine + 1)),
           qPrintable(text));
}

#include "test_log_session.moc"

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  TestLogSession test;
  return QTest::qExec(&test, argc, argv);
}
