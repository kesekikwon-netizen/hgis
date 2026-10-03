#include "app/KaSplashCredits.h"
#include "app/KaSplashStrata.h"
#include "app/KaStartupSplash.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QImage>
#include <QPainter>
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
  // The interval counts from when the notice appears (user 2026-10-04 「첫 로딩화면을 5초로 하라」).
  // A slow initialization still never reveals the main window early, and once it is ready after the
  // interval has passed the home screen opens at once instead of waiting another interval.
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
    splash.markReady();  // Repeated readiness must not open the main window twice.
    QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 1, 1000);
    QVERIFY(elapsed.elapsed() < 200);  // the interval already passed while preparing
    QVERIFY(mainWindow.isVisible());
    QVERIFY(!splash.isVisible());
    QCOMPARE(splash.readingProgress(), 1000);
    QTest::qWait(80);
    QCOMPARE(ready.count(), 1);
  }

  // The home screen opens 5 s after the notice appears; initialization (measured about 1.4 s on
  // 2026-10-03) ends inside that time.
  void defaultFiveSecondsFromShowStayResponsiveAndKeepNoticesVisible() {
    QCOMPARE(KaStartupSplash::ReadingDurationMs, 5000);
    QElapsedTimer elapsed;
    elapsed.start();
    KaStartupSplash splash;
    QSignalSpy ready(&splash, &KaStartupSplash::readyToShow);
    splash.show();
    QTest::qWait(1400);  // initialization
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
    QTest::qWait(qMax(1, 4600 - int(elapsed.elapsed())));
    QCOMPARE(ready.count(), 0);
    QVERIFY(splash.isVisible());
    QVERIFY(beats > 10);
    QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 1, 1500);
    QVERIFY(elapsed.elapsed() >= 5000);
    QVERIFY(elapsed.elapsed() < 6000);
  }

  // The 3 s count from the notice's appearance, but the dots start flowing from the left when the
  // app is ready instead of jumping to the middle of the lane (code review 2026-10-03).
  void dotsStartFlowingWhenReady() {
    KaStartupSplash splash(nullptr, 3000);
    splash.show();
    QTest::qWait(700);  // a slow initialization
    splash.markReady();
    QTest::qWait(50);
    QVERIFY2(splash.flowSeconds() < 0.3, qPrintable(QString::number(splash.flowSeconds())));
    QVERIFY(splash.readingProgress() > 200);  // the reading time still counts from the notice
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

  // 새 모양 notice: the app is busy until it is ready, so the bedrock, the name and the two notice
  // lines are in the first frame, and the notice ink reads on the bedrock. The strata above it
  // stack up only once the app is ready (user 2026-10-04: 지층이 아래부터 한 겹씩 쌓인다).
  void strataNoticeShowsBedrockAndReadableNoticesFromTheFirstFrame() {
    KaStartupSplash splash(nullptr, 3000, true);
    splash.show();
    QVERIFY(QTest::qWaitForWindowExposed(&splash));
    saveFrame(splash, QStringLiteral("startup-strata-first.png"));
    const QImage frame = splash.grab().toImage();
    const QRectF card = splash.cardRect();
    const double unit = card.height() / 400.0;
    const double dpr = frame.devicePixelRatio();
    const auto at = [&](double x, double y) { return frame.pixelColor(int((card.left() + x * unit) * dpr), int((card.top() + y * unit) * dpr)); };
    QCOMPARE(at(672, 380), KaSplashStrata::bedrock());  // beside the notice lines
    QVERIFY(inkContrast(KaSplashStrata::noticeInk(), KaSplashStrata::bedrock()) >= 4.5);
    QCOMPARE(at(350, 20), KaSplashStrata::paper());
    QCOMPARE(at(350, 300), KaSplashStrata::paper());  // the strata have not stacked yet
  }

  // Once the app is ready the strata stack from the bedrock up, then the pit, the ground line and
  // the control point come in that order and stay; reduced motion shows the finished picture at once.
  void strataTimelineRunsInOrderAndReducedMotionSkipsIt() {
    QCOMPARE(KaSplashStrata::stratumReveal(0, 0.0), 1.0);  // the bedrock carries the notices
    for (int i = 1; i < 5; ++i) {
      QCOMPARE(KaSplashStrata::stratumReveal(i, 0.0), 0.0);
      QVERIFY(KaSplashStrata::stratumReveal(i, 1.0) <= KaSplashStrata::stratumReveal(i - 1, 1.0));
    }
    QVERIFY(KaSplashStrata::stratumReveal(4, 1.0) < 1.0 && KaSplashStrata::stratumReveal(4, 1.9) == 1.0);
    QCOMPARE(KaSplashStrata::pitReveal(1.9), 0.0);
    QVERIFY(KaSplashStrata::pitReveal(2.4) == 1.0 && KaSplashStrata::groundLineReveal(2.2) == 0.0);
    QVERIFY(KaSplashStrata::groundLineReveal(3.0) == 1.0 && KaSplashStrata::markerReveal(3.0) == 0.0);
    QCOMPARE(KaSplashStrata::markerReveal(KaSplashStrata::kTimelineSeconds), 1.0);
    // The control point's lower edge at (610, 219) is slate once it has landed, paper before.
    const auto markerEdge = [](const char* reduced) {
      qputenv("KA_HGIS_REDUCED_MOTION", reduced);
      KaStartupSplash splash(nullptr, 3000, true);
      splash.show();
      if (!QTest::qWaitForWindowExposed(&splash)) return QColor();
      saveFrame(splash, QStringLiteral("startup-strata-motion-%1.png").arg(QLatin1String(reduced)));
      const QRectF card = splash.cardRect();
      const double unit = card.height() / 400.0;
      const QImage frame = splash.grab().toImage();
      return frame.pixelColor(int((card.left() + 610 * unit) * frame.devicePixelRatio()), int((card.top() + 219 * unit) * frame.devicePixelRatio()));
    };
    const QColor waiting = markerEdge("0");
    const QColor finished = markerEdge("1");
    qunsetenv("KA_HGIS_REDUCED_MOTION");
    QCOMPARE(waiting, KaSplashStrata::paper());
    QVERIFY2(finished.isValid() && finished.lightness() < 110, qPrintable(finished.name()));
  }

  // A small screen shrinks the notice and its card becomes wider than 700:400: the strata still reach
  // the right edge (no stripe of paper beside the bedrock).
  void strataReachTheRightEdgeOfAWiderCard() {
    QImage image(760, 400, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    KaSplashStrata::paint(painter, QRectF(0, 0, 760, 400), 16, QPixmap(), KaSplashStrata::Frame());
    painter.end();
    QCOMPARE(image.pixelColor(750, 372), KaSplashStrata::bedrock());
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
