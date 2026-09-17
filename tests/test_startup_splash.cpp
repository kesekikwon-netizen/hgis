#include "app/KaStartupSplash.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QImage>
#include <QLabel>
#include <QProgressBar>
#include <QSignalSpy>
#include <QTimer>
#include <QtTest>

class StartupSplashTest : public QObject {
  Q_OBJECT
private slots:
  void readinessAndReadingIntervalGateMainWindow() {
    KaStartupSplash splash(nullptr, 240);
    QWidget mainWindow;
    connect(&splash, &KaStartupSplash::readyToShow, &mainWindow, [&]() {
      mainWindow.show();
      splash.close();
    });
    QSignalSpy ready(&splash, &KaStartupSplash::readyToShow);
    splash.show();
    QTest::qWait(280);
    QCOMPARE(ready.count(), 0);  // A slow initialization never reveals the main window.
    QVERIFY(!mainWindow.isVisible());
    QTest::mouseClick(&splash, Qt::LeftButton, Qt::NoModifier, QPoint(8, 8));
    QVERIFY(splash.isVisible());
    QElapsedTimer elapsed;
    elapsed.start();
    splash.markReady();
    splash.markReady();  // Repeated readiness must not restart the reading interval.
    QTest::qWait(100);
    auto* progress = splash.findChild<QProgressBar*>(QStringLiteral("startupProgress"));
    QVERIFY(progress);
    QVERIFY(progress->value() > 0 && progress->value() < 1000);
    QVERIFY(!mainWindow.isVisible());
    QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 1, 1000);
    QVERIFY(elapsed.elapsed() >= 240);
    QVERIFY(mainWindow.isVisible());
    QVERIFY(!splash.isVisible());
    QCOMPARE(progress->value(), 1000);
    QTest::qWait(80);
    QCOMPARE(ready.count(), 1);
  }

  void defaultTenSecondsStayResponsiveAndKeepNoticesVisible() {
    QCOMPARE(KaStartupSplash::ReadingDurationMs, 10000);
    KaStartupSplash splash;
    QSignalSpy ready(&splash, &KaStartupSplash::readyToShow);
    splash.show();
    QElapsedTimer elapsed;
    elapsed.start();
    splash.markReady();
    int beats = 0;
    QTimer heartbeat;
    connect(&heartbeat, &QTimer::timeout, this, [&]() { ++beats; });
    heartbeat.start(20);
    QTest::qWait(250);
    const QString output = qEnvironmentVariable("KA_STARTUP_QA_OUTPUT_DIR");
    if (!output.isEmpty()) {
      QDir().mkpath(output);
      QVERIFY(splash.grab().save(QDir(output).filePath(QStringLiteral("startup-v2.png"))));
    }
    QVERIFY(splash.findChild<QLabel*>(QStringLiteral("startupAuthor"))->text()
                .contains(QStringLiteral("권영인")));
    QVERIFY(splash.findChild<QLabel*>(QStringLiteral("startupVersion"))->text()
                .startsWith(QStringLiteral("v2")));
    const QString attribution = KaStartupSplash::attributionText();
    for (const QString& library : {QStringLiteral("QGIS"), QStringLiteral("Qt"),
                                  QStringLiteral("GDAL"), QStringLiteral("PROJ"),
                                  QStringLiteral("GEOS"), QStringLiteral("SQLite"),
                                  QStringLiteral("Chromium")})
      QVERIFY(attribution.contains(library));
    QTest::qWait(qMax(1, 9000 - int(elapsed.elapsed())));
    QCOMPARE(ready.count(), 0);
    QVERIFY(splash.isVisible());
    QVERIFY(beats > 20);  // The normal event loop keeps processing other work.
    QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 1, 2000);
    QVERIFY(elapsed.elapsed() >= 10000);
    QVERIFY(elapsed.elapsed() < 12000);
  }
};

QTEST_MAIN(StartupSplashTest)
#include "test_startup_splash.moc"
