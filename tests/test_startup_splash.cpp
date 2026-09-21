#include "app/KaStartupSplash.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QImage>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QTimer>
#include <QtTest>
#include <QWidget>

namespace {

// Saves a frame for visual QA when KA_STARTUP_QA_OUTPUT_DIR is set.
void saveFrame(QWidget& widget, const QString& name) {
  const QString output = qEnvironmentVariable("KA_STARTUP_QA_OUTPUT_DIR");
  if (output.isEmpty()) return;
  QDir().mkpath(output);
  widget.grab().save(QDir(output).filePath(name));
}

void moveMouse(QWidget& widget, const QPointF& pos, Qt::MouseButtons buttons = Qt::NoButton) {
  QMouseEvent move(QEvent::MouseMove, pos, widget.mapToGlobal(pos), Qt::NoButton, buttons,
                   Qt::NoModifier);
  QApplication::sendEvent(&widget, &move);
}

}  // namespace

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
    QVERIFY(splash.readingProgress() > 0 && splash.readingProgress() < 1000);
    QVERIFY(!mainWindow.isVisible());
    QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 1, 1000);
    QVERIFY(elapsed.elapsed() >= 240);
    QVERIFY(mainWindow.isVisible());
    QVERIFY(!splash.isVisible());
    QCOMPARE(splash.readingProgress(), 1000);
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
    const QString credits = KaStartupSplash::creditsText();
    for (const QString& text : {QStringLiteral("만든이"), QStringLiteral("권영인"),
                                QStringLiteral("동국문화재연구원"),
                                QStringLiteral("v2"), QStringLiteral("GPL"),
                                QStringLiteral("VWorld"), QStringLiteral("국가유산")})
      QVERIFY2(credits.contains(text), qPrintable(text));
    QVERIFY2(!credits.contains(QStringLiteral("조유량")), "splash must not list 조유량");
    QVERIFY2(!credits.contains(QStringLiteral("박종환")), "splash must not list 박종환");
    QCOMPARE(splash.accessibleDescription(), credits);
    const QString attribution = KaStartupSplash::attributionText();
    for (const QString& library : {QStringLiteral("QGIS"), QStringLiteral("Qt"),
                                  QStringLiteral("GDAL"), QStringLiteral("PROJ"),
                                  QStringLiteral("GEOS"), QStringLiteral("SQLite"),
                                  QStringLiteral("Chromium")})
      QVERIFY(attribution.contains(library));
    QTest::qWait(qMax(1, 9000 - int(elapsed.elapsed())));
    QCOMPARE(ready.count(), 0);
    QVERIFY(splash.isVisible());
    QVERIFY(beats > 20);
    QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 1, 3000);
    QVERIFY(elapsed.elapsed() >= 10000);
    QVERIFY(elapsed.elapsed() < 13000);
  }

  void plateIsOpaqueAndMotionChangesTheFrame() {
    KaStartupSplash splash(nullptr, 3000);
    splash.show();
    QTest::qWait(80);
    saveFrame(splash, QStringLiteral("startup-cover.png"));
    const QImage cover = splash.grab().toImage();
    // Rounded card: the window corners stay clear so the white mat can sit outside.
    QVERIFY(qAlpha(cover.pixel(1, 1)) < 40);
    QVERIFY(qAlpha(cover.pixel(cover.width() - 2, cover.height() - 2)) < 40);
    QVERIFY(qAlpha(cover.pixel(cover.width() / 2, cover.height() / 2)) > 200);
    const QColor rim = cover.pixelColor(20, cover.height() / 2);
    QVERIFY(rim.red() > 180 && rim.green() > 180 && rim.blue() > 180);
    splash.markReady();
    const QRect plan = splash.planRect().toRect();
    const QImage first = splash.grab(plan).toImage();
    QTRY_VERIFY_WITH_TIMEOUT(splash.motionClock() > 0.25, 2000);
    saveFrame(splash, QStringLiteral("startup-motion.png"));
    const QImage moving = splash.grab().toImage();
    const QImage later = splash.grab(plan).toImage();
    QVERIFY(qAlpha(moving.pixel(1, 1)) < 40);
    QVERIFY(qAlpha(moving.pixel(moving.width() / 2, moving.height() / 2)) > 200);
    QVERIFY(first != later);
  }

  void clickNeverClosesTheNotice() {
    KaStartupSplash splash(nullptr, 4000);
    QSignalSpy ready(&splash, &KaStartupSplash::readyToShow);
    splash.show();
    splash.markReady();
    QTest::qWait(60);
    const QRectF plan = splash.planRect();
    QVERIFY(splash.revealedFraction() < 1.0);
    moveMouse(splash, QPointF(plan.left() + plan.width() * 0.3, plan.top() + plan.height() * 0.4));
    saveFrame(splash, QStringLiteral("startup-scrape.png"));
    QTest::mouseClick(&splash, Qt::LeftButton, Qt::NoModifier, plan.center().toPoint());
    QVERIFY(splash.isVisible());
    QCOMPARE(ready.count(), 0);
    QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 1, 6000);
    QVERIFY(splash.revealedFraction() >= 0.99);
    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(&splash, &leave);
    splash.update();
    QTest::qWait(30);
    saveFrame(splash, QStringLiteral("startup-final.png"));
  }

  void reducedMotionShowsTheFinishedDrawing() {
    qputenv("KA_HGIS_REDUCED_MOTION", "1");
    KaStartupSplash splash(nullptr, 1000);
    qputenv("KA_HGIS_REDUCED_MOTION", "0");
    splash.show();
    splash.markReady();
    QTest::qWait(60);
    QCOMPARE(splash.revealedFraction(), 1.0);
    moveMouse(splash, splash.planRect().center());
    QVERIFY(splash.readingProgress() < 1000);
  }
};

int main(int argc, char** argv) {
  // Tests exercise the motion regardless of the PC's animation setting.
  qputenv("KA_HGIS_REDUCED_MOTION", "0");
  QApplication app(argc, argv);
  StartupSplashTest test;
  return QTest::qExec(&test, argc, argv);
}
#include "test_startup_splash.moc"
