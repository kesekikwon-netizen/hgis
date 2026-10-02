// VworldSettings store scope (evaluation F037): with a scope folder, saving and migrating
// keys only writes files in that folder, never the user's registry store or AppData.
// Key values in this test are fake and are never logged.
#include <QtTest>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <memory>

#include "core/VworldSettings.h"

class TestVworldSettings : public QObject {
  Q_OBJECT
private slots:
  void init() {
    m_env = qgetenv("VWORLD_API_KEY");
    m_hadEnv = qEnvironmentVariableIsSet("VWORLD_API_KEY");
    qunsetenv("VWORLD_API_KEY");
    QVERIFY(m_folder.isValid());
    VworldSettings::Scope scope;
    scope.folder = m_folder.path();
    VworldSettings::setScope(scope);
  }

  void cleanup() {
    VworldSettings::resetScope();
    if (m_hadEnv)
      qputenv("VWORLD_API_KEY", m_env);
    else
      qunsetenv("VWORLD_API_KEY");
  }

  void scopeFolder_holdsEveryStore() {
    const std::unique_ptr<QSettings> native = VworldSettings::openNativeStore();
    QCOMPARE(native->format(), QSettings::IniFormat);
    QCOMPARE(QDir::cleanPath(QFileInfo(native->fileName()).absolutePath()),
             QDir::cleanPath(QDir(m_folder.path()).absolutePath()));

    VworldSettings::saveApiKey(QStringLiteral("SCOPED_TEST_KEY_1"));
    QCOMPARE(VworldSettings::loadApiKey(), QStringLiteral("SCOPED_TEST_KEY_1"));
    QSettings ini(QDir(m_folder.path()).filePath(QStringLiteral("ka-hgis-vworld.ini")), QSettings::IniFormat);
    QCOMPARE(ini.value(QStringLiteral("VWorld/ApiKey")).toString(), QStringLiteral("SCOPED_TEST_KEY_1"));
    const std::unique_ptr<QSettings> reread = VworldSettings::openNativeStore();
    QCOMPARE(reread->value(QStringLiteral("VWorld/ApiKey")).toString(), QStringLiteral("SCOPED_TEST_KEY_1"));

    VworldSettings::saveHistoryGisApiKey(QStringLiteral("SCOPED_HISTORY_KEY"));
    QCOMPARE(VworldSettings::loadHistoryGisApiKey(), QStringLiteral("SCOPED_HISTORY_KEY"));
    VworldSettings::saveHistoryGisApiKey(QString());
  }

  void scopeFolder_migratesLegacyNativeKey() {
    // The legacy vworld/apiKey in the native store moves to VWorld/ApiKey (existing
    // behaviour), now exercised without the Windows registry.
    VworldSettings::saveApiKey(QString());
    {
      const std::unique_ptr<QSettings> native = VworldSettings::openNativeStore();
      native->remove(QStringLiteral("VWorld/ApiKey"));
      native->setValue(QStringLiteral("vworld/apiKey"), QStringLiteral("SCOPED_LEGACY_KEY"));
      native->sync();
    }
    QCOMPARE(VworldSettings::loadApiKey(), QStringLiteral("SCOPED_LEGACY_KEY"));
    QSettings ini(QDir(m_folder.path()).filePath(QStringLiteral("ka-hgis-vworld.ini")), QSettings::IniFormat);
    QCOMPARE(ini.value(QStringLiteral("VWorld/ApiKey")).toString(), QStringLiteral("SCOPED_LEGACY_KEY"));
  }

  void scope_resetsToRealStores() {
    VworldSettings::Scope named;
    named.organization = QStringLiteral("ka-hgis-test");
    VworldSettings::setScope(named);
    QCOMPARE(VworldSettings::scope().organization, QStringLiteral("ka-hgis-test"));
    QCOMPARE(VworldSettings::scope().application, QStringLiteral("ka-hgis"));
    QVERIFY(VworldSettings::scope().folder.isEmpty());
    // Only inspected, never written: a named scope without a folder is a native store.
    QCOMPARE(VworldSettings::openNativeStore()->organizationName(), QStringLiteral("ka-hgis-test"));
    VworldSettings::resetScope();
    QCOMPARE(VworldSettings::scope().organization, QStringLiteral("ka-hgis"));
    QVERIFY(VworldSettings::scope().folder.isEmpty());
  }

private:
  QTemporaryDir m_folder;
  QByteArray m_env;
  bool m_hadEnv = false;
};

int main(int argc, char** argv) {
  QStandardPaths::setTestModeEnabled(true);
  QCoreApplication app(argc, argv);
  QTemporaryDir isolated;
  if (!isolated.isValid()) return 1;
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, isolated.path());
  if (qEnvironmentVariableIsEmpty("KA_HGIS_LOG_DIR"))
    qputenv("KA_HGIS_LOG_DIR", isolated.filePath(QStringLiteral("logs")).toUtf8());
  TestVworldSettings test;
  return QTest::qExec(&test, argc, argv);
}
#include "test_vworld_settings.moc"
