#include "app/KaSplashCredits.h"
#include "app/KaStartupSplash.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QImage>
#include <QSignalSpy>
#include <QTimer>
#include <QtTest>
#include <QWidget>

#include <cmath>

namespace {

// Saves a frame for visual QA when KA_STARTUP_QA_OUTPUT_DIR is set.
void saveFrame(QWidget& widget, const QString& name) {
  const QString output = qEnvironmentVariable("KA_STARTUP_QA_OUTPUT_DIR");
  if (output.isEmpty()) return;
  QDir().mkpath(output);
  widget.grab().save(QDir(output).filePath(name));
}

QImage grabArea(QWidget& widget, const QRectF& area) {
  return widget.grab(area.toAlignedRect()).toImage();
}

double luminance(const QColor& c) {
  const auto lin = [](double v) { return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
  return 0.2126 * lin(c.redF()) + 0.7152 * lin(c.greenF()) + 0.0722 * lin(c.blueF());
}

// Contrast of ink (with its alpha) painted over an opaque background.
double inkContrast(const QColor& ink, const QColor& background) {
  const double a = ink.alphaF();
  const QColor text = QColor::fromRgbF(float(ink.redF() * a + background.redF() * (1 - a)),
                                       float(ink.greenF() * a + background.greenF() * (1 - a)),
                                       float(ink.blueF() * a + background.blueF() * (1 - a)));
  const double x = luminance(text), y = luminance(background);
  return (qMax(x, y) + 0.05) / (qMin(x, y) + 0.05);
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

  void defaultFiveSecondsStayResponsiveAndKeepNoticesVisible() {
    QCOMPARE(KaStartupSplash::ReadingDurationMs, 5000);
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
    saveFrame(splash, QStringLiteral("startup-v2.png"));
    const QString credits = KaStartupSplash::creditsText();
    for (const QString& text : {QStringLiteral("Strata"), QStringLiteral("필드고고학 GIS"),
                                QStringLiteral("만든이"), QStringLiteral("권영인"),
                                QStringLiteral("동국문화재연구원"), QStringLiteral("v2"),
                                QStringLiteral("GPL"), QStringLiteral("VWorld"),
                                QStringLiteral("국가유산")})
      QVERIFY2(credits.contains(text), qPrintable(text));
    QVERIFY2(!credits.contains(QStringLiteral("조유량")), "splash must not list 조유량");
    QVERIFY2(!credits.contains(QStringLiteral("박종환")), "splash must not list 박종환");
    QCOMPARE(splash.accessibleDescription(), credits);
    // The painted short form keeps the creator, the licence and every provider.
    QVERIFY(KaSplashCredits::copyrightLine().contains(QStringLiteral("권영인")));
    QVERIFY(KaSplashCredits::copyrightLine().contains(QStringLiteral("GPL")));
    for (const QString& provider : {QStringLiteral("VWorld"), QStringLiteral("국토정보플랫폼"),
                                    QStringLiteral("국가유산"), QStringLiteral("흙토람"),
                                    QStringLiteral("KIGAM"), QStringLiteral("국사편찬위원회")})
      QVERIFY2(KaSplashCredits::dataLine().contains(provider), qPrintable(provider));
    QCOMPARE(KaSplashCredits::productSubtitle(QStringLiteral("2.0.0")),
             QStringLiteral("필드고고학 GIS · v2.0.0"));
    const QString attribution = KaStartupSplash::attributionText();
    for (const QString& library : {QStringLiteral("QGIS"), QStringLiteral("Qt"),
                                  QStringLiteral("GDAL"), QStringLiteral("PROJ"),
                                  QStringLiteral("GEOS"), QStringLiteral("SQLite"),
                                  QStringLiteral("Chromium")})
      QVERIFY(attribution.contains(library));
    QTest::qWait(qMax(1, 4000 - int(elapsed.elapsed())));
    QCOMPARE(ready.count(), 0);
    QVERIFY(splash.isVisible());
    QVERIFY(beats > 20);
    QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 1, 2500);
    QVERIFY(elapsed.elapsed() >= 5000);
    QVERIFY(elapsed.elapsed() < 8000);
  }

  void cardIsRoundedOpaqueAndOnlyTheDotsMove() {
    KaStartupSplash splash(nullptr, 3000);
    splash.show();
    QTest::qWait(80);
    saveFrame(splash, QStringLiteral("startup-cover.png"));
    const QImage cover = splash.grab().toImage();
    const qreal dpr = cover.devicePixelRatio();
    auto pixel = [&](const QPointF& p) { return cover.pixelColor((p * dpr).toPoint()); };
    const QRectF card = splash.cardRect();
    // The window and the rounded card corners stay clear.
    QVERIFY(pixel(QPointF(1, 1)).alpha() < 40);
    QVERIFY(pixel(card.topLeft() + QPointF(1.5, 1.5)).alpha() < 40);
    // The card itself is the opaque theme blue.
    const QColor centre = pixel(card.center());
    QCOMPARE(centre.alpha(), 255);
    QVERIFY(centre.blue() > centre.red() + 40);

    const QRectF dots = splash.dotsRect();
    QVERIFY(card.contains(dots));
    QVERIFY(dots.top() > card.center().y());  // every line of text sits in the lower part
    const QRectF art(card.left(), card.top(), card.width(), dots.top() - card.top() - 30);
    splash.markReady();
    QTest::qWait(30);
    const QImage artBefore = grabArea(splash, art);
    const QImage dotsBefore = grabArea(splash, dots);
    QTest::qWait(350);
    saveFrame(splash, QStringLiteral("startup-motion.png"));
    QCOMPARE(grabArea(splash, art), artBefore);  // the artwork never moves
    QVERIFY(grabArea(splash, dots) != dotsBefore);  // the dots flow
  }

  // The licence notice is the smallest text, so it must keep 4.5:1 over every
  // pixel of the card behind it (blue gradient, contour lines, foot shade).
  void noticesKeepReadableContrast() {
    KaStartupSplash splash(nullptr, 1500);
    const QImage backdrop = splash.backdropImage();
    const QRect area = splash.noticesRect().toAlignedRect().intersected(backdrop.rect());
    QVERIFY(!area.isEmpty());
    double copyright = 99.0;
    double data = 99.0;
    for (int y = area.top(); y <= area.bottom(); ++y) {
      for (int x = area.left(); x <= area.right(); x += 2) {
        const QColor background = backdrop.pixelColor(x, y);
        QCOMPARE(background.alpha(), 255);
        copyright = qMin(copyright, inkContrast(KaSplashCredits::copyrightInk(), background));
        data = qMin(data, inkContrast(KaSplashCredits::dataInk(), background));
      }
    }
    QVERIFY2(copyright >= 4.5, qPrintable(QStringLiteral("copyright line %1:1").arg(copyright)));
    QVERIFY2(data >= 4.5, qPrintable(QStringLiteral("data line %1:1").arg(data)));
  }

  void clickNeverClosesTheNotice() {
    KaStartupSplash splash(nullptr, 1500);
    QSignalSpy ready(&splash, &KaStartupSplash::readyToShow);
    splash.show();
    splash.markReady();
    QTest::qWait(60);
    QTest::mouseClick(&splash, Qt::LeftButton, Qt::NoModifier, splash.cardRect().center().toPoint());
    QTest::keyClick(&splash, Qt::Key_Escape);
    QVERIFY(splash.isVisible());
    QCOMPARE(ready.count(), 0);
    QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 1, 4000);
    saveFrame(splash, QStringLiteral("startup-final.png"));
  }

  void reducedMotionKeepsTheDotsStill() {
    qputenv("KA_HGIS_REDUCED_MOTION", "1");
    KaStartupSplash splash(nullptr, 1500);
    qputenv("KA_HGIS_REDUCED_MOTION", "0");
    splash.show();
    splash.markReady();
    QTest::qWait(60);
    const QImage first = grabArea(splash, splash.dotsRect());
    QTest::qWait(300);
    QCOMPARE(grabArea(splash, splash.dotsRect()), first);
    QVERIFY(splash.readingProgress() > 0 && splash.readingProgress() < 1000);
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
