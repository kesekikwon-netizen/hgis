#include <cmath>

#include <QtTest>
#include <QApplication>
#include <QPixmap>
#include <QSignalSpy>
#include <QWheelEvent>

#include "app/KaImageView.h"

// Left pane of the align split: dragging a numbered point fine-tunes it, and a reduced
// preview of a huge scan is re-read from the original once the view zooms past it.
class TestAlignImageView : public QObject {
  Q_OBJECT
private slots:
  void dragMark_reportsOriginalPixels();
  void clickOnMarkWithoutMoving_stillPicks();
  void zoomedPreview_readsOriginalWindow();
  void zoomedOutPreview_readsNothing();
};

namespace {

void showView(KaImageView& view) {
  view.resize(420, 320);
  view.show();
  QVERIFY(QTest::qWaitForWindowExposed(&view));
}

QPixmap filled(int w, int h) {
  QPixmap pm(w, h);
  pm.fill(Qt::white);
  return pm;
}

void wheelIn(KaImageView& view, int steps) {
  const QPointF at = QRectF(view.viewport()->rect()).center();
  for (int i = 0; i < steps; ++i) {
    QWheelEvent ev(at, view.viewport()->mapToGlobal(at), QPoint(), QPoint(0, 120), Qt::NoButton,
                   Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(view.viewport(), &ev);
  }
}

}  // namespace

void TestAlignImageView::dragMark_reportsOriginalPixels() {
  KaImageView view;
  showView(view);
  QVERIFY(view.setPreview(filled(200, 100), 2000, 1000));  // 1 preview px = 10 source px
  view.setMarks({QPointF(1000, 500)});
  QSignalSpy dragged(&view, &KaImageView::markDragged);
  QSignalSpy clicked(&view, &KaImageView::pixelClicked);

  const QPoint from = view.viewPosForPixel(1000, 500);
  const QPoint to = from + QPoint(30, 0);
  QTest::mousePress(view.viewport(), Qt::LeftButton, {}, from);
  QTest::mouseMove(view.viewport(), to);
  QTest::mouseRelease(view.viewport(), Qt::LeftButton, {}, to);

  QCOMPARE(clicked.count(), 0);
  QCOMPARE(dragged.count(), 1);
  const QList<QVariant> args = dragged.takeFirst();
  QCOMPARE(args.at(0).toInt(), 0);
  QVERIFY2(args.at(1).toDouble() > 1050.0, qPrintable(args.at(1).toString()));
  QVERIFY2(std::abs(args.at(2).toDouble() - 500.0) < 15.0, qPrintable(args.at(2).toString()));
}

void TestAlignImageView::clickOnMarkWithoutMoving_stillPicks() {
  KaImageView view;
  showView(view);
  QVERIFY(view.setPreview(filled(200, 100), 2000, 1000));
  view.setMarks({QPointF(1000, 500)});
  QSignalSpy dragged(&view, &KaImageView::markDragged);
  QSignalSpy clicked(&view, &KaImageView::pixelClicked);
  const QPoint at = view.viewPosForPixel(1000, 500);
  QTest::mouseClick(view.viewport(), Qt::LeftButton, {}, at);
  QCOMPARE(dragged.count(), 0);
  QCOMPARE(clicked.count(), 1);

  // Away from any mark a click picks, as before, in original pixels.
  QTest::mouseClick(view.viewport(), Qt::LeftButton, {}, view.viewPosForPixel(300, 200));
  QCOMPARE(clicked.count(), 2);
  QVERIFY(std::abs(clicked.last().at(0).toDouble() - 300.0) < 15.0);
}

void TestAlignImageView::zoomedPreview_readsOriginalWindow() {
  KaImageView view;
  showView(view);
  QVERIFY(view.setPreview(filled(200, 100), 2000, 1000));
  QVector<QRect> windows;
  QVector<QSize> sizes;
  view.setDetailReader([&](const QRect& window, const QSize& out) {
    windows << window;
    sizes << out;
    QImage img(out, QImage::Format_RGB32);
    img.fill(Qt::red);
    return img;
  });
  // Fitted, the 200 px preview fills ~400 px: already past its own resolution.
  QTRY_VERIFY_WITH_TIMEOUT(view.detailShown(), 3000);
  QVERIFY(!windows.isEmpty());
  QVERIFY(QRect(0, 0, 2000, 1000).contains(windows.last()));
  QVERIFY2(sizes.last().width() <= windows.last().width(), "never more pixels than the original");

  const int before = windows.last().width();
  wheelIn(view, 6);
  QTRY_VERIFY_WITH_TIMEOUT(windows.last().width() < before, 3000);
  QCOMPARE(view.detailWindow(), windows.last());
}

void TestAlignImageView::zoomedOutPreview_readsNothing() {
  KaImageView view;
  showView(view);
  // A preview larger than the view is already sharper than the screen: nothing to re-read.
  QVERIFY(view.setPreview(filled(2000, 1000), 20000, 10000));
  int reads = 0;
  view.setDetailReader([&](const QRect&, const QSize& out) {
    ++reads;
    return QImage(out, QImage::Format_RGB32);
  });
  QTest::qWait(500);
  QCOMPARE(reads, 0);
  QVERIFY(!view.detailShown());

  // A normally loaded image (no preview) never uses the reader either.
  QVERIFY(!view.loadPath(QStringLiteral("Z:/no-such-scan.tif")));
  QTest::qWait(300);
  QCOMPARE(reads, 0);
}

#include "test_align_image_view.moc"

QTEST_MAIN(TestAlignImageView)
