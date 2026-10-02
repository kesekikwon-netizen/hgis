#pragma once

#include <QGraphicsView>
#include <QWidget>
#include <QVector>
#include <QPointF>
#include <QLine>
#include <QImage>
#include <QRect>
#include <QSize>
#include <functional>

class QGraphicsPixmapItem;
class QGraphicsItem;
class QTimer;

class KaAlignLinkOverlay : public QWidget {
  Q_OBJECT
public:
  explicit KaAlignLinkOverlay(QWidget* parent = nullptr);
  void setLinks(const QVector<QLine>& done, const QLine& live, bool hasLive);

protected:
  void paintEvent(QPaintEvent* e) override;

private:
  void drawArrow(QPainter& p, const QLine& ln);
  void drawNumber(QPainter& p, const QPoint& at, int n, const QColor& ring);
  QVector<QLine> m_done;
  QLine m_live;
  bool m_hasLive = false;
};

class KaImageView : public QGraphicsView {
  Q_OBJECT
public:
  explicit KaImageView(QWidget* parent = nullptr);

  bool loadPath(const QString& path);
  // 원본이 Qt 로 열기엔 너무 큰 그림용. 화면에는 축소본을 보여 주되,
  // 클릭 좌표는 언제나 원본 픽셀로 돌려준다(정합 계산이 원본 기준이라서).
  bool setPreview(const QPixmap& preview, int sourceWidth, int sourceHeight);
  QString lastError() const { return m_lastError; }
  void clearMarks();
  void setMarks(const QVector<QPointF>& pts, const QPointF* pending = nullptr);
  void fitImage();
  bool hasImage() const { return m_pix != nullptr; }
  // 화면 1픽셀이 원본 몇 픽셀인지. 원본 그대로면 1.
  double sourceScale() const { return m_srcScale; }
  QPoint viewPosForPixel(double pixelX, double pixelY) const;

  // For a reduced preview: once the view is zoomed past the preview's resolution, the
  // visible window is read again from the original (source pixels in, image out).
  using DetailReader = std::function<QImage(const QRect& sourceWindow, const QSize& outSize)>;
  void setDetailReader(DetailReader reader);
  bool detailShown() const;
  QRect detailWindow() const { return m_detailWindow; }

signals:
  void pixelClicked(double x, double y);
  // A numbered mark was dragged to a new place (original pixels).
  void markDragged(int index, double x, double y);
  void viewChanged();

protected:
  void wheelEvent(QWheelEvent* e) override;
  void mousePressEvent(QMouseEvent* e) override;
  void mouseMoveEvent(QMouseEvent* e) override;
  void mouseReleaseEvent(QMouseEvent* e) override;
  void resizeEvent(QResizeEvent* e) override;
  void scrollContentsBy(int dx, int dy) override;

private:
  void addMarkItem(double pixelX, double pixelY, int number, const QColor& ring);
  int markAt(const QPoint& viewPos) const;
  QPointF sourcePixelAt(const QPoint& viewPos) const;
  void scheduleDetail();
  void refreshDetail();
  void hideDetail();

  bool applyPixmap(const QPixmap& pm);
  double m_srcScale = 1.0;
  QSize m_srcSize;
  QString m_lastError;
  QGraphicsPixmapItem* m_pix = nullptr;
  QVector<QGraphicsItem*> m_marks;
  QVector<QPointF> m_markPixels;
  int m_dragMark = -1;
  QPoint m_dragPress;
  QPoint m_lastPan;
  bool m_panning = false;
  bool m_fitted = false;
  DetailReader m_detailReader;
  QGraphicsPixmapItem* m_detail = nullptr;
  QTimer* m_detailTimer = nullptr;
  QRect m_detailWindow;
  QSize m_detailOut;
};
