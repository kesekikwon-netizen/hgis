#pragma once

#include <QList>
#include <QMarginsF>
#include <QPageSize>
#include <QRectF>
#include <QSizeF>
#include <QString>

class QPagedPaintDevice;

// 큰 도면을 작은 용지 여러 장에 나눠 찍는다(예: A0 도면을 A3 9장으로).
// 종이에는 「PDF 내보내기」와 같은 도면 PDF를 잘라 그린다. 그래서 인쇄물과 PDF가 같다.
// 길이 단위는 모두 mm.
struct TilePlanRequest {
  QSizeF drawingMm;           // 도면 PDF 한 쪽 크기
  double scale = 1.0;         // 붙였을 때 크기 = 도면 × scale. 1이면 도면 축척 그대로
  QPageSize sheet;            // 나눌 용지. 방향은 plan 이 정한다
  QMarginsF printerMarginMm;  // 프린터가 찍지 못하는 가장자리
  double overlapMm = 10.0;    // 이웃 장과 같은 그림이 겹치는 폭
  double labelBandMm = 0.0;   // 아래쪽에 장 번호·안내를 찍는 띠. 도면은 그 위까지만 찍는다
};

struct TileSheet {
  int row = 0;
  int col = 0;
  QRectF areaMm;  // 붙였을 때의 도면 좌표에서 이 장이 맡는 칸. 끝 장은 도면 밖으로 나갈 수 있다
};

struct TilePlan {
  bool ok = false;
  QString error;
  QPageSize sheet;
  bool landscape = false;
  QMarginsF printerMarginMm;
  double scale = 1.0;
  double overlapMm = 0.0;
  double labelBandMm = 0.0;
  QSizeF outputMm;   // 붙였을 때 크기
  QSizeF contentMm;  // 한 장에서 도면이 찍히는 칸(프린터 여백·번호 띠 제외)
  int rows = 0;
  int cols = 0;
  QList<TileSheet> sheets;  // 위에서 아래로, 왼쪽에서 오른쪽으로
};

class TilePrint {
public:
  // 도면을 target 용지 안에 비율대로 넣는 배율. 용지 방향은 도면 방향에 맞춘다.
  static double fitScale(const QSizeF& drawingMm, const QPageSize& target);
  static TilePlan plan(const TilePlanRequest& request, bool landscapeSheet);
  // 세로·가로 중 장수가 적은 쪽. 같으면 세로.
  static TilePlan bestPlan(const TilePlanRequest& request);
  static QSizeF pdfPageSizeMm(const QString& pdfPath, QString* error = nullptr);

  struct Marks {
    bool cutLines = true;  // 오른쪽·아래 이웃이 시작하는 곳의 자르는 점선과 맞춤 표시
    QString title;         // 번호 띠에 함께 찍는 이름
  };
  // pdfPath 첫 쪽을 plan 대로 잘라 device 의 각 쪽에 그린다. 용지·방향·여백은 여기서 맞춘다.
  // dpi 는 그림 해상도의 상한이다. 프린터가 더 높아도 이보다 곱게 그리지 않는다.
  static bool renderTiles(const QString& pdfPath, const TilePlan& plan, QPagedPaintDevice* device,
                          double dpi, const Marks& marks, QString* error);
  // 용지 한 장에 비율대로 맞춰 가운데에 찍는다. 용지 방향은 도면 방향에 맞춘다.
  static bool renderFit(const QString& pdfPath, const QPageSize& sheet,
                        const QMarginsF& printerMarginMm, QPagedPaintDevice* device, double dpi,
                        QString* error);
};
