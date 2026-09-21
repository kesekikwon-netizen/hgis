#include <QtTest>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include "core/KaSecretStore.h"
#include "core/TopographicSettings.h"

class TopographicSettingsTest : public QObject {
  Q_OBJECT
private:
  static bool fixture(const QString& path, const QString& username, const QString& password) {
    QSettings settings(path, QSettings::IniFormat);
    settings.setValue(QStringLiteral("ngii/username"), username);
    settings.setValue(QStringLiteral("ngii/password"), password);
    settings.sync(); return settings.status() == QSettings::NoError;
  }
private slots:
  void fallbackOrderAndExplicitEmptyOverride() {
    QTemporaryDir temp; QVERIFY(temp.isValid());
    const auto personal = temp.filePath(QStringLiteral("personal.ini"));
    const auto bundled = temp.filePath(QStringLiteral("bundled.ini"));
    const auto repo = temp.filePath(QStringLiteral("repo.ini"));
    QVERIFY(fixture(repo, QStringLiteral("fixture-repo"), QStringLiteral("fixture-password")));
    auto value = TopographicSettings::readFromFiles(personal, {bundled, repo});
    QCOMPARE(value.username, QStringLiteral("fixture-repo"));
    QVERIFY(fixture(bundled, QStringLiteral("fixture-bundled"), QStringLiteral("fixture-other")));
    value = TopographicSettings::readFromFiles(personal, {bundled, repo});
    QCOMPARE(value.username, QStringLiteral("fixture-bundled"));
    QVERIFY(fixture(personal, QStringLiteral("fixture-personal"), QStringLiteral("fixture-local")));
    value = TopographicSettings::readFromFiles(personal, {bundled, repo});
    QCOMPARE(value.username, QStringLiteral("fixture-personal"));
    QCOMPARE(value.password, QStringLiteral("fixture-local"));
    QSettings migrated(personal, QSettings::IniFormat); migrated.setFallbacksEnabled(false);
    QVERIFY(migrated.contains(QStringLiteral("ngii/password_dpapi")));
    QVERIFY(!migrated.contains(QStringLiteral("ngii/password")));
    QSettings bundledSettings(bundled, QSettings::IniFormat); bundledSettings.setFallbacksEnabled(false);
    QVERIFY(bundledSettings.contains(QStringLiteral("ngii/password")));
    QVERIFY(!bundledSettings.contains(QStringLiteral("ngii/password_dpapi")));
    QString error;
    QVERIFY2(TopographicSettings::saveToFile(personal, {}, &error), qPrintable(error));
    value = TopographicSettings::readFromFiles(personal, {bundled, repo});
    QVERIFY(value.username.isEmpty()); QVERIFY(value.password.isEmpty());
    QVERIFY(QFileInfo::exists(personal));
  }
  void roundTripSpecialCharactersAndAtomicFailure() {
    QTemporaryDir temp; QVERIFY(temp.isValid());
    const auto personal = temp.filePath(QStringLiteral("settings/ngii-account.ini"));
    const TopographicSettings::Credentials expected{QStringLiteral("fixture-user"),
      QStringLiteral("  가상 @비밀\\path;=\"value\"\nline\tend  ")};
    QString error;
    QVERIFY2(TopographicSettings::saveToFile(personal, expected, &error), qPrintable(error));
    const auto actual = TopographicSettings::readFromFiles(personal, {});
    QCOMPARE(actual.username, expected.username); QCOMPARE(actual.password, expected.password);
    QSettings stored(personal, QSettings::IniFormat); stored.setFallbacksEnabled(false);
    QVERIFY(stored.contains(QStringLiteral("ngii/password_dpapi")));
    QVERIFY(!stored.contains(QStringLiteral("ngii/password")));
    QFile existing(personal); QVERIFY(existing.open(QIODevice::ReadOnly)); const auto before = existing.readAll(); existing.close();
    // A regular file cannot be used as the next file's parent directory.
    QVERIFY(!TopographicSettings::saveToFile(personal + QStringLiteral("/child.ini"), {}, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(existing.open(QIODevice::ReadOnly)); QCOMPARE(existing.readAll(), before);
    QCOMPARE(QDir(QFileInfo(personal).absolutePath()).entryList(QDir::Files | QDir::Hidden),
             QStringList{QStringLiteral("ngii-account.ini")});
  }
  void existingEmptyOrUnreadablePersonalBlocksFallback() {
    QTemporaryDir temp; const auto personal = temp.filePath(QStringLiteral("personal.ini"));
    const auto fallback = temp.filePath(QStringLiteral("fallback.ini"));
    QVERIFY(fixture(fallback, QStringLiteral("fixture-default"), QStringLiteral("fixture-secret")));
    QFile empty(personal); QVERIFY(empty.open(QIODevice::WriteOnly)); empty.close();
    QVERIFY(TopographicSettings::readFromFiles(personal, {fallback}).username.isEmpty());
    const auto directory = temp.filePath(QStringLiteral("directory.ini")); QVERIFY(QDir().mkpath(directory));
    QVERIFY(TopographicSettings::readFromFiles(directory, {fallback}).username.isEmpty());
  }

  void portableSecretsTravelWithoutDpapi() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto personal = temp.filePath(QStringLiteral("ngii-account.ini"));
    const TopographicSettings::Credentials expected{QStringLiteral("portable-user"),
                                                    QStringLiteral("pw-across-pc")};
    KaSecretStore::setPortableSecretsForTests(true);
    QString error;
    QVERIFY2(TopographicSettings::saveToFile(personal, expected, &error), qPrintable(error));
    const auto actual = TopographicSettings::readFromFiles(personal, {});
    QCOMPARE(actual.username, expected.username);
    QCOMPARE(actual.password, expected.password);
    QSettings stored(personal, QSettings::IniFormat);
    stored.setFallbacksEnabled(false);
    QVERIFY(stored.contains(QStringLiteral("ngii/password_portable")));
    QVERIFY(!stored.contains(QStringLiteral("ngii/password_dpapi")));
    QVERIFY(!stored.contains(QStringLiteral("ngii/password")));
    KaSecretStore::resetPortableSecretsForTests();
  }

  void deadDpapiPersonalFallsBackToBundledPlaintext() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto personal = temp.filePath(QStringLiteral("personal.ini"));
    const auto bundled = temp.filePath(QStringLiteral("bundled.ini"));
    QVERIFY(fixture(bundled, QStringLiteral("fixture-bundled"), QStringLiteral("fixture-secret")));
    QSettings dead(personal, QSettings::IniFormat);
    dead.setFallbacksEnabled(false);
    dead.setValue(QStringLiteral("ngii/username"), QStringLiteral("stale-user"));
    dead.setValue(QStringLiteral("ngii/password_dpapi"), QStringLiteral("not-valid-dpapi"));
    dead.sync();
    const auto value = TopographicSettings::readFromFiles(personal, {bundled});
    QCOMPARE(value.username, QStringLiteral("fixture-bundled"));
    QCOMPARE(value.password, QStringLiteral("fixture-secret"));
  }
};
QTEST_GUILESS_MAIN(TopographicSettingsTest)
#include "test_topographic_settings.moc"
