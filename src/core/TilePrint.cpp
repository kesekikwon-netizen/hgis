#include "TilePrint.h"

#include <QFont>
#include <QFontMetricsF>
#include <QImage>
#include <QPageLayout>
#include <QPagedPaintDevice>
#include <QPainter>
#include <QPdfDocument>
#include <QPdfDocumentRenderOptions>
#include <QPen>

#include <algorithm>
#include <cmath>

namespace {
constexpr double kMmPerInch = 25.4;
constexpr double kPointsPerMm = 72.0 / kMmPerInch;
// pdfium 에 한 번에 맡기는 띠 높이(px). A3 한 장을 300 DPI 로 통째로 그리면 70 MB 라 나눠 그린다.
constexpr int kBandPx = 1024;

bool fail(QString* error, const QString& message) {
  if (error) *error = message;
  return false;
}

QSizeF portraitMm(const QPageSize& size) {
  const QSizeF s = size.size(QPageSize::Millimeter);
  return s.width() <= s.height() ? s : s.transposed();
}

// 첫 장은 content 만큼, 둘째 장부터는 앞 장과 overlap 만큼 겹치므로 content - overlap 씩 나아간다.
int sheetsAlong(double lengthMm, double contentMm, double overlapMm) {
  if (lengthMm <= contentMm + 1e-6) return 1;
  return int(std::ceil((lengthMm - overlapMm) / (contentMm - overlapMm) - 1e-9));
}

bool loadPdf(QPdfDocument& doc, const QString& pdfPath, QString* error) {
  if (doc.load(pdfPath) != QPdfDocument::Error::None || doc.pageCount() < 1)
    return fail(error, QStringLiteral("도면 PDF를 열지 못했습니다."));
  return true;
}

QSizeF pageMm(QPdfDocument& doc) {
  const QSizeF points = doc.pagePointSize(0);
  return QSizeF(points.width() / kPointsPerMm, points.height() / kPointsPerMm);
}

// 쪽 전체를 fullPx 로 키웠을 때 clipPx 부분만 그려 painter 의 target 에 놓는다.
bool drawRegion(QPdfDocument& doc, QPainter& painter, const QSize& fullPx, const QRect& clipPx,
                const QRectF& target, QString* error) {
  const double ky = target.height() / clipPx.height();
  for (int y = 0; y < clipPx.height(); y += kBandPx) {
    const int h = std::min(kBandPx, clipPx.height() - y);
    QPdfDocumentRenderOptions options;
    options.setScaledSize(fullPx);
    options.setScaledClipRect(QRect(clipPx.x(), clipPx.y() + y, clipPx.width(), h));
    const QImage band = doc.render(0, QSize(clipPx.width(), h), options);
    if (band.isNull())
      return fail(error, QStringLiteral("도면 그림을 만들지 못했습니다. 메모리가 부족할 수 있습니다."));
    painter.drawImage(QRectF(target.x(), target.y() + y * ky, target.width(), h * ky), band);
  }
  return true;
}

// 용지·방향·여백을 맞추고, 도면이 찍힐 칸이 실제로 들어가는지 확인한다.
bool preparePage(QPagedPaintDevice* device, const QPageSize& sheet, bool landscape,
                 const QMarginsF& marginMm, const QSizeF& needMm, QString* error) {
  device->setPageLayout(QPageLayout(sheet, landscape ? QPageLayout::Landscape : QPageLayout::Portrait,
                                    marginMm, QPageLayout::Millimeter));
  const QRectF paint = device->pageLayout().paintRect(QPageLayout::Millimeter);
  if (paint.width() + 0.5 < needMm.width() || paint.height() + 0.5 < needMm.height()) {
    // 줄여 찍으면 붙였을 때 축척이 틀어진다. 여백을 다시 잡도록 멈춘다.
    return fail(error, QStringLiteral("프린터가 여백을 더 크게 잡습니다. 인쇄 창을 닫았다가 다시 열어 주세요."));
  }
  return true;
}
}  // namespace

double TilePrint::fitScale(const QSizeF& drawingMm, const QPageSize& target) {
  if (drawingMm.isEmpty() || !target.isValid()) return 1.0;
  QSizeF t = portraitMm(target);
  if (drawingMm.width() > drawingMm.height()) t.transpose();
  return std::min(t.width() / drawingMm.width(), t.height() / drawingMm.height());
}

TilePlan TilePrint::plan(const TilePlanRequest& request, bool landscapeSheet) {
  TilePlan p;
  p.sheet = request.sheet;
  p.landscape = landscapeSheet;
  p.printerMarginMm = request.printerMarginMm;
  p.scale = request.scale;
  p.overlapMm = std::max(0.0, request.overlapMm);
  p.labelBandMm = std::max(0.0, request.labelBandMm);
  if (request.drawingMm.isEmpty() || request.scale <= 0.0 || !request.sheet.isValid()) {
    p.error = QStringLiteral("도면이나 용지 크기를 알 수 없습니다.");
    return p;
  }
  p.outputMm = request.drawingMm * request.scale;
  QSizeF sheet = portraitMm(request.sheet);
  if (landscapeSheet) sheet.transpose();
  const QMarginsF& m = request.printerMarginMm;
  p.contentMm = QSizeF(sheet.width() - m.left() - m.right(),
                       sheet.height() - m.top() - m.bottom() - p.labelBandMm);
  if (p.contentMm.width() <= p.overlapMm + 1.0 || p.contentMm.height() <= p.overlapMm + 1.0) {
    p.error = QStringLiteral("겹침과 여백이 용지보다 큽니다. 겹침을 줄여 주세요.");
    return p;
  }
  p.cols = sheetsAlong(p.outputMm.width(), p.contentMm.width(), p.overlapMm);
  p.rows = sheetsAlong(p.outputMm.height(), p.contentMm.height(), p.overlapMm);
  const double stepW = p.contentMm.width() - p.overlapMm;
  const double stepH = p.contentMm.height() - p.overlapMm;
  for (int row = 0; row < p.rows; ++row) {
    for (int col = 0; col < p.cols; ++col) {
      TileSheet tile;
      tile.row = row;
      tile.col = col;
      tile.areaMm = QRectF(col * stepW, row * stepH, p.contentMm.width(), p.contentMm.height());
      p.sheets.append(tile);
    }
  }
  p.ok = true;
  return p;
}

TilePlan TilePrint::bestPlan(const TilePlanRequest& request) {
  const TilePlan portrait = plan(request, false);
  const TilePlan landscape = plan(request, true);
  if (!landscape.ok) return portrait;
  if (!portrait.ok) return landscape;
  return landscape.sheets.size() < portrait.sheets.size() ? landscape : portrait;
}

QSizeF TilePrint::pdfPageSizeMm(const QString& pdfPath, QString* error) {
  QPdfDocument doc;
  if (!loadPdf(doc, pdfPath, error)) return {};
  return pageMm(doc);
}

bool TilePrint::renderTiles(const QString& pdfPath, const TilePlan& plan, QPagedPaintDevice* device,
                            double dpi, const Marks& marks, QString* error) {
  if (!device) return fail(error, QStringLiteral("인쇄할 곳이 없습니다."));
  if (!plan.ok || plan.sheets.isEmpty())
    return fail(error, plan.error.isEmpty() ? QStringLiteral("나눠 찍을 계획이 없습니다.") : plan.error);
  QPdfDocument doc;
  if (!loadPdf(doc, pdfPath, error)) return false;
  const QSizeF need(plan.contentMm.width(), plan.contentMm.height() + plan.labelBandMm);
  if (!preparePage(device, plan.sheet, plan.landscape, plan.printerMarginMm, need, error)) return false;

  QPainter painter;
  if (!painter.begin(device)) return fail(error, QStringLiteral("프린터를 시작하지 못했습니다."));
  painter.setRenderHint(QPainter::SmoothPixmapTransform);
  const double d = device->logicalDpiX() / kMmPerInch;               // 종이 px/mm
  const double r = std::min(dpi, double(device->logicalDpiX())) / kMmPerInch;  // 그림 px/mm
  const QSize fullPx(qRound(plan.outputMm.width() * r), qRound(plan.outputMm.height() * r));
  const QRect fullRect(QPoint(0, 0), fullPx);
  const double cw = plan.contentMm.width() * d;
  const double ch = plan.contentMm.height() * d;
  const int total = int(plan.sheets.size());
  for (int i = 0; i < total; ++i) {
    const TileSheet& tile = plan.sheets.at(i);
    if (i > 0 && !device->newPage()) {
      painter.end();
      return fail(error, QStringLiteral("다음 장으로 넘기지 못했습니다."));
    }
    const QRectF area = tile.areaMm;
    // 이웃 장과 경계가 1 px 도 어긋나지 않게 그림 px 칸을 먼저 정하고 그 칸을 종이에 옮긴다.
    const QRect clip = QRect(QPoint(int(std::floor(area.left() * r)), int(std::floor(area.top() * r))),
                             QPoint(int(std::ceil(area.right() * r)) - 1,
                                    int(std::ceil(area.bottom() * r)) - 1)) & fullRect;
    if (!clip.isEmpty()) {
      const QRectF target((clip.x() / r - area.x()) * d, (clip.y() / r - area.y()) * d,
                          clip.width() / r * d, clip.height() / r * d);
      if (!drawRegion(doc, painter, fullPx, clip, target, error)) {
        painter.end();
        return false;
      }
    }
    if (marks.cutLines) {
      QPen cut(QColor(80, 80, 80));
      cut.setWidthF(0.25 * d);
      cut.setDashPattern({8.0, 6.0});
      painter.setPen(cut);
      if (tile.col + 1 < plan.cols) {
        const double x = (plan.contentMm.width() - plan.overlapMm) * d;
        painter.drawLine(QPointF(x, 0), QPointF(x, ch));
      }
      if (tile.row + 1 < plan.rows) {
        const double y = (plan.contentMm.height() - plan.overlapMm) * d;
        painter.drawLine(QPointF(0, y), QPointF(cw, y));
      }
      // 앞 장의 잘린 가장자리를 맞출 시작선. 도면이 하얀 곳이어도 보이게 모서리에 짧게 찍는다.
      QPen tick(QColor(80, 80, 80));
      tick.setWidthF(0.3 * d);
      painter.setPen(tick);
      const double len = 5.0 * d;
      if (tile.col > 0) {
        painter.drawLine(QPointF(0, 0), QPointF(0, len));
        painter.drawLine(QPointF(0, ch - len), QPointF(0, ch));
      }
      if (tile.row > 0) {
        painter.drawLine(QPointF(0, 0), QPointF(len, 0));
        painter.drawLine(QPointF(cw - len, 0), QPointF(cw, 0));
      }
    }
    if (plan.labelBandMm > 0.0) {
      QFont font = painter.font();
      font.setPixelSize(std::max(6, int(std::lround(2.6 * d))));
      painter.setFont(font);
      painter.setPen(QColor(60, 60, 60));
      const QString name = marks.title.trimmed().isEmpty() ? QStringLiteral("도면") : marks.title.trimmed();
      const QString text = QStringLiteral("%1 · %2행 %3열 (%4/%5장, 가로 %6장 × 세로 %7장) · "
                                          "점선을 따라 잘라 옆 장 그림의 시작선에 맞춰 겹쳐 붙이세요")
                               .arg(name)
                               .arg(tile.row + 1)
                               .arg(tile.col + 1)
                               .arg(i + 1)
                               .arg(total)
                               .arg(plan.cols)
                               .arg(plan.rows);
      const QRectF band(0, ch + 0.8 * d, cw, (plan.labelBandMm - 0.8) * d);
      painter.drawText(band, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                       QFontMetricsF(font).elidedText(text, Qt::ElideRight, band.width()));
    }
  }
  painter.end();
  return true;
}

bool TilePrint::renderFit(const QString& pdfPath, const QPageSize& sheet, const QMarginsF& printerMarginMm,
                          QPagedPaintDevice* device, double dpi, QString* error) {
  if (!device) return fail(error, QStringLiteral("인쇄할 곳이 없습니다."));
  QPdfDocument doc;
  if (!loadPdf(doc, pdfPath, error)) return false;
  const QSizeF drawing = pageMm(doc);
  if (drawing.isEmpty()) return fail(error, QStringLiteral("도면 크기를 알 수 없습니다."));
  if (!preparePage(device, sheet, drawing.width() > drawing.height(), printerMarginMm, QSizeF(1, 1), error))
    return false;
  const QRectF paint = device->pageLayout().paintRect(QPageLayout::Millimeter);
  const double k = std::min(paint.width() / drawing.width(), paint.height() / drawing.height());
  const QSizeF outMm = drawing * k;
  QPainter painter;
  if (!painter.begin(device)) return fail(error, QStringLiteral("프린터를 시작하지 못했습니다."));
  painter.setRenderHint(QPainter::SmoothPixmapTransform);
  const double d = device->logicalDpiX() / kMmPerInch;
  const double r = std::min(dpi, double(device->logicalDpiX())) / kMmPerInch;
  const QSize fullPx(qRound(outMm.width() * r), qRound(outMm.height() * r));
  const QRectF target((paint.width() - outMm.width()) / 2.0 * d, (paint.height() - outMm.height()) / 2.0 * d,
                      outMm.width() * d, outMm.height() * d);
  const bool ok = drawRegion(doc, painter, fullPx, QRect(QPoint(0, 0), fullPx), target, error);
  painter.end();
  return ok;
}
