#include <QtTest>
#include <QApplication>
#include <QPageSize>
#include <QPainter>
#include <QPdfDocument>
#include <QPdfWriter>
#include <QTemporaryDir>

#include "app/KaPrintDialog.h"
#include "core/TilePrint.h"

namespace {
constexpr double kPointsPerMm = 72.0 / 25.4;

// 네 모서리 칸이 다른 색인 도면 PDF(widthMm × heightMm).
QString makeQuadrantPdf(const QString& path, const QSizeF& mm) {
  QPdfWriter writer(path);
  writer.setResolution(72);
  // QPageSize keeps sizes portrait; a wide drawing is a portrait size turned landscape.
  const QSizeF portrait(qMin(mm.width(), mm.height()), qMax(mm.width(), mm.height()));
  writer.setPageLayout(QPageLayout(QPageSize(portrait, QPageSize::Millimeter),
                                   mm.width() > mm.height() ? QPageLayout::Landscape : QPageLayout::Portrait,
                                   QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter));
  QPainter painter(&writer);
  const double w = writer.width();
  const double h = writer.height();
  painter.fillRect(QRectF(0, 0, w / 2, h / 2), QColor(220, 20, 20));        // 왼쪽 위 빨강
  painter.fillRect(QRectF(w / 2, 0, w / 2, h / 2), QColor(20, 170, 40));    // 오른쪽 위 초록
  painter.fillRect(QRectF(0, h / 2, w / 2, h / 2), QColor(20, 40, 220));    // 왼쪽 아래 파랑
  painter.fillRect(QRectF(w / 2, h / 2, w / 2, h / 2), QColor(240, 210, 20));  // 오른쪽 아래 노랑
  painter.end();
  return path;
}

// 출력 PDF 쪽을 그려 종이 위치(mm)의 색을 읽는다.
QColor colorAtMm(QPdfDocument& doc, int page, const QPointF& mm) {
  const QSizeF points = doc.pagePointSize(page);
  const double pxPerMm = 2.0;
  const QSize size(qRound(points.width() / kPointsPerMm * pxPerMm), qRound(points.height() / kPointsPerMm * pxPerMm));
  const QImage image = doc.render(page, size).convertToFormat(QImage::Format_ARGB32);
  return image.pixelColor(qRound(mm.x() * pxPerMm), qRound(mm.y() * pxPerMm));
}

bool near(const QColor& c, int r, int g, int b) {
  return c.alpha() > 200 && qAbs(c.red() - r) < 40 && qAbs(c.green() - g) < 40 && qAbs(c.blue() - b) < 40;
}

bool paper(const QColor& c) {
  return c.alpha() < 30 || (c.red() > 230 && c.green() > 230 && c.blue() > 230);
}

TilePlanRequest request(const QSizeF& drawingMm, QPageSize::PageSizeId sheet, double labelBandMm) {
  TilePlanRequest r;
  r.drawingMm = drawingMm;
  r.sheet = QPageSize(sheet);
  r.printerMarginMm = QMarginsF(5, 5, 5, 5);
  r.overlapMm = 10.0;
  r.labelBandMm = labelBandMm;
  return r;
}
}  // namespace

class TestTilePrint : public QObject {
  Q_OBJECT
private slots:
  void a0DrawingNeedsNineA3Sheets() {
    // A3 한 장: 가장자리 5 mm, 번호 띠 6 mm → 도면 칸 287×404 mm, 겹침 10 mm.
    const TilePlan plan = TilePrint::bestPlan(request(QSizeF(841, 1189), QPageSize::A3, 6.0));
    QVERIFY2(plan.ok, qPrintable(plan.error));
    QVERIFY(!plan.landscape);
    QCOMPARE(plan.cols, 3);
    QCOMPARE(plan.rows, 3);
    QCOMPARE(plan.sheets.size(), 9);
    QCOMPARE(plan.contentMm, QSizeF(287, 404));
    QCOMPARE(plan.sheets.first().areaMm, QRectF(0, 0, 287, 404));
    // 이웃 장은 겹침만큼 앞 장 끝을 다시 찍는다.
    QCOMPARE(plan.sheets.at(1).areaMm.left(), 277.0);
    QCOMPARE(plan.sheets.at(3).areaMm.top(), 394.0);
    const QRectF last = plan.sheets.last().areaMm;
    QVERIFY2(last.right() >= 841.0 && last.bottom() >= 1189.0, "끝 장이 도면 끝까지 닿지 않습니다.");
  }

  void a3DrawingEnlargedToA0KeepsAspect() {
    const QSizeF a3(297, 420);
    const double scale = TilePrint::fitScale(a3, QPageSize(QPageSize::A0));
    QVERIFY(qAbs(scale - 1189.0 / 420.0) < 0.001);
    QVERIFY(qAbs(TilePrint::fitScale(a3.transposed(), QPageSize(QPageSize::A0)) - scale) < 1e-9);
    TilePlanRequest r = request(a3, QPageSize::A3, 6.0);
    r.scale = scale;
    const TilePlan plan = TilePrint::bestPlan(r);
    QVERIFY2(plan.ok, qPrintable(plan.error));
    QCOMPARE(plan.sheets.size(), 9);
    QVERIFY(plan.outputMm.width() <= 841.0 + 0.01 && plan.outputMm.height() <= 1189.0 + 0.01);
  }

  void smallDrawingStaysOnOneSheet() {
    const TilePlan plan = TilePrint::bestPlan(request(QSizeF(210, 297), QPageSize::A3, 6.0));
    QVERIFY2(plan.ok, qPrintable(plan.error));
    QCOMPARE(plan.sheets.size(), 1);
  }

  void overlapWiderThanSheetIsRefused() {
    TilePlanRequest r = request(QSizeF(841, 1189), QPageSize::A4, 0.0);
    r.overlapMm = 250.0;
    const TilePlan plan = TilePrint::bestPlan(r);
    QVERIFY(!plan.ok);
    QVERIFY(!plan.error.isEmpty());
  }

  void tilesPutEachPartOfTheDrawingOnItsOwnPage() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = makeQuadrantPdf(dir.filePath(QStringLiteral("drawing.pdf")), QSizeF(400, 300));
    QCOMPARE(TilePrint::pdfPageSizeMm(source).toSize(), QSize(400, 300));
    const TilePlan plan = TilePrint::bestPlan(request(QSizeF(400, 300), QPageSize::A4, 0.0));
    QVERIFY2(plan.ok, qPrintable(plan.error));
    // A4 가로 칸 287×200 mm, 겹침 10 mm → 가로 2장 × 세로 2장.
    QVERIFY(plan.landscape);
    QCOMPARE(plan.cols, 2);
    QCOMPARE(plan.rows, 2);
    const QString tiles = dir.filePath(QStringLiteral("tiles.pdf"));
    {
      QPdfWriter writer(tiles);
      writer.setResolution(100);
      QString error;
      TilePrint::Marks marks;
      marks.cutLines = false;
      QVERIFY2(TilePrint::renderTiles(source, plan, &writer, 100.0, marks, &error), qPrintable(error));
    }
    QPdfDocument out;
    QCOMPARE(out.load(tiles), QPdfDocument::Error::None);
    QCOMPARE(out.pageCount(), 4);
    const QSizeF page = out.pagePointSize(0) / kPointsPerMm;
    QVERIFY2(qAbs(page.width() - 297.0) < 1.0 && qAbs(page.height() - 210.0) < 1.0, "A4 가로 쪽이 아닙니다.");
    // 종이 위치 = 가장자리 5 mm + 이 장의 칸 안 위치.
    QVERIFY2(near(colorAtMm(out, 0, QPointF(5 + 20, 5 + 20)), 220, 20, 20), "첫 장 왼쪽 위가 도면 왼쪽 위(빨강)가 아닙니다.");
    QVERIFY2(near(colorAtMm(out, 0, QPointF(5 + 250, 5 + 20)), 20, 170, 40), "첫 장 오른쪽이 도면 오른쪽 위(초록)가 아닙니다.");
    // 넷째 장(2행 2열) 칸은 도면 (277, 190)부터다. 도면 (390, 290)은 노랑.
    QVERIFY2(near(colorAtMm(out, 3, QPointF(5 + 113, 5 + 100)), 240, 210, 20), "마지막 장이 도면 오른쪽 아래(노랑)가 아닙니다.");
    // 도면 밖(가로 400 mm 너머)은 비어 있어야 한다.
    QVERIFY2(paper(colorAtMm(out, 3, QPointF(5 + 200, 5 + 50))), "도면 밖에 그림이 찍혔습니다.");
  }

  void fitPrintsTheWholeDrawingCentred() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = makeQuadrantPdf(dir.filePath(QStringLiteral("drawing.pdf")), QSizeF(400, 300));
    const QString out = dir.filePath(QStringLiteral("fit.pdf"));
    {
      QPdfWriter writer(out);
      writer.setResolution(100);
      QString error;
      QVERIFY2(TilePrint::renderFit(source, QPageSize(QPageSize::A4), QMarginsF(5, 5, 5, 5), &writer, 100.0, &error),
               qPrintable(error));
    }
    QPdfDocument doc;
    QCOMPARE(doc.load(out), QPdfDocument::Error::None);
    QCOMPARE(doc.pageCount(), 1);
    // 가로 도면은 A4 가로에 2/3 배율로 들어가고 좌우 10.2 mm씩 비운다.
    const double k = 200.0 / 300.0;
    const double left = 5 + (287 - 400 * k) / 2;
    QVERIFY2(near(colorAtMm(doc, 0, QPointF(left + 100 * k, 5 + 75 * k)), 220, 20, 20), "왼쪽 위 칸(빨강)이 아닙니다.");
    QVERIFY2(near(colorAtMm(doc, 0, QPointF(left + 300 * k, 5 + 225 * k)), 240, 210, 20), "오른쪽 아래 칸(노랑)이 아닙니다.");
    QVERIFY2(paper(colorAtMm(doc, 0, QPointF(left - 5, 5 + 100))), "도면 왼쪽 여백에 그림이 찍혔습니다.");
  }

  void printDialogCountsSheetsAndWarnsAboutEnlarging() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = makeQuadrantPdf(dir.filePath(QStringLiteral("drawing.pdf")), QSizeF(297, 420));
    KaPrintDialog dialog(source, QStringLiteral("시험 도면"));
    dialog.setTiled(true);
    dialog.setSheet(KaPrintDialog::SheetA4);
    dialog.setOutput(KaPrintDialog::OutputA0);
    QVERIFY2(dialog.plan().ok, qPrintable(dialog.plan().error));
    const int count = int(dialog.plan().sheets.size());
    QVERIFY(count > 1);
    QVERIFY2(dialog.summary().contains(QStringLiteral("= %1장").arg(count)), qPrintable(dialog.summary()));
    QVERIFY2(dialog.warning().contains(QStringLiteral("283%")), qPrintable(dialog.warning()));
    const QString tiles = dir.filePath(QStringLiteral("dialog-tiles.pdf"));
    QString error;
    QVERIFY2(dialog.saveTilesPdf(tiles, &error), qPrintable(error));
    QPdfDocument doc;
    QCOMPARE(doc.load(tiles), QPdfDocument::Error::None);
    QCOMPARE(doc.pageCount(), count);

    dialog.setOutput(KaPrintDialog::OutputDrawing);
    QVERIFY2(dialog.warning().isEmpty(), "도면 크기 그대로인데 축척 경고가 나옵니다.");
    QVERIFY(dialog.summary().contains(QStringLiteral("도면 축척 그대로")));
    dialog.setTiled(false);
    QVERIFY(dialog.summary().contains(QStringLiteral("한 장에 맞춰")));
  }
};

QTEST_MAIN(TestTilePrint)
#include "test_tile_print.moc"
