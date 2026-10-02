#include "KaImageView.h"

#include "KaTheme.h"
#include <QImageReader>

#include <QGraphicsPixmapItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsItemGroup>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QPixmap>
#include <QColor>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QPainter>
#include <cmath>
#include <QPaintEvent>
#include <QRect>
#include <QPolygon>
#include <QPolygonF>

KaAlignLinkOverlay::KaAlignLinkOverlay(QWidget* parent)
    : QWidget(parent) {
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setAttribute(Qt::WA_NoSystemBackground);
  setAttribute(Qt::WA_TranslucentBackground);
}

void KaAlignLinkOverlay::setLinks(const QVector<QLine>& done, const QLine& live, bool hasLive) {
  m_done = done;
  m_live = live;
  m_hasLive = hasLive;
  update();
}

void KaAlignLinkOverlay::drawArrow(QPainter& p, const QLine& ln) {
  const QPointF d = QPointF(ln.p2()) - QPointF(ln.p1());
  const double len = std::hypot(d.x(), d.y());
  if (len < 4) return;
  p.drawLine(ln);
  const QPointF u(d.x() / len, d.y() / len);
  const QPointF n(-u.y(), u.x());
  QPolygonF head;
  head << ln.p2() << (ln.p2() - u * 12 + n * 5) << (ln.p2() - u * 12 - n * 5);
  p.drawPolygon(head);
}

void KaAlignLinkOverlay::drawNumber(QPainter& p, const QPoint& at, int n, const QColor& ring) {
  const int r = 8;
  p.setPen(QPen(ring, 2));
  p.setBrush(QColor(255, 255, 255, 240));
  p.drawEllipse(at, r, r);
  p.setPen(QColor(15, 23, 42));
  QFont f = p.font();
  f.setPointSize(8);
  f.setBold(true);
  p.setFont(f);
  p.drawText(QRect(at.x() - r, at.y() - r, r * 2, r * 2), Qt::AlignCenter, QString::number(n));
}

void KaAlignLinkOverlay::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(QColor(30, 103, 198, 240), 2.4));
  p.setBrush(QColor(30, 103, 198, 240));
  for (int i = 0; i < m_done.size(); ++i) {
    drawArrow(p, m_done[i]);
    drawNumber(p, m_done[i].p1(), i + 1, QColor(30, 103, 198));
    drawNumber(p, m_done[i].p2(), i + 1, QColor(30, 103, 198));
  }
  if (m_hasLive) {
    QPen live(QColor(234, 88, 12, 240), 2.6, Qt::DashLine);
    live.setDashPattern({6, 4});
    p.setPen(live);
    p.setBrush(QColor(234, 88, 12, 240));
    drawArrow(p, m_live);
    drawNumber(p, m_live.p1(), m_done.size() + 1, QColor(234, 88, 12));
  }
}

KaImageView::KaImageView(QWidget* parent)
    : QGraphicsView(parent) {
  setScene(new QGraphicsScene(this));
  setDragMode(QGraphicsView::NoDrag);
  setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
  setResizeAnchor(QGraphicsView::AnchorViewCenter);
  setRenderHint(QPainter::SmoothPixmapTransform, true);
  setBackgroundBrush(KaTheme::tokens().desk);
  setMouseTracking(true);
  setCursor(Qt::CrossCursor);
}

bool KaImageView::applyPixmap(const QPixmap& pm) {
  clearMarks();
  scene()->clear();
  m_pix = nullptr;
  m_detail = nullptr;  // owned by the scene, deleted by clear()
  m_detailWindow = QRect();
  m_dragMark = -1;
  m_fitted = false;
  if (pm.isNull()) return false;
  m_pix = scene()->addPixmap(pm);
  m_pix->setZValue(0);
  scene()->setSceneRect(QRectF(pm.rect()).adjusted(-20, -20, 20, 20));
  fitImage();
  return true;
}

bool KaImageView::loadPath(const QString& path) {
  m_srcScale = 1.0;
  m_srcSize = QSize();
  m_detailReader = nullptr;
  m_lastError.clear();
  // Qt 는 그림 한 장을 통째로 메모리에 편다. Qt6 기본 상한이 256MB 라서
  // 항공사진 원판(1억 화소 이상)은 여기서 조용히 빈 그림으로 떨어진다.
  // 왜 못 열었는지 남겨 두어야 위에서 다른 길로 갈 수 있다.
  QImageReader reader(path);
  reader.setAutoTransform(true);
  const QImage img = reader.read();
  if (img.isNull()) {
    m_lastError = reader.errorString();
    const QSize sz = reader.size();
    if (sz.isValid())
      m_lastError += QStringLiteral(" (%1 x %2 화소)").arg(sz.width()).arg(sz.height());
    applyPixmap(QPixmap());
    return false;
  }
  m_srcSize = img.size();
  return applyPixmap(QPixmap::fromImage(img));
}

bool KaImageView::setPreview(const QPixmap& preview, int sourceWidth, int sourceHeight) {
  m_lastError.clear();
  if (preview.isNull() || preview.width() < 1 || sourceWidth < 1 || sourceHeight < 1)
    return false;
  // 보여 주는 건 축소본이지만 바깥에는 원본 픽셀로 말한다.
  m_srcScale = double(sourceWidth) / double(preview.width());
  m_srcSize = QSize(sourceWidth, sourceHeight);
  m_detailReader = nullptr;  // a reader belongs to one image; the caller sets it afterwards
  return applyPixmap(preview);
}

void KaImageView::clearMarks() {
  for (auto* m : m_marks) {
    if (m && scene()) scene()->removeItem(m);
    delete m;
  }
  m_marks.clear();
  m_markPixels.clear();
}

void KaImageView::addMarkItem(double pixelX, double pixelY, int number, const QColor& ring) {
  if (!scene()) return;
  auto* g = new QGraphicsItemGroup();
  g->setFlag(QGraphicsItem::ItemIgnoresTransformations);
  const qreal r = 9;
  auto* fill = new QGraphicsEllipseItem(-r, -r, r * 2, r * 2);
  fill->setPen(QPen(ring, 2.4));
  fill->setBrush(QBrush(QColor(255, 255, 255, 240)));
  auto* h = new QGraphicsLineItem(-r - 4, 0, r + 4, 0);
  auto* v = new QGraphicsLineItem(0, -r - 4, 0, r + 4);
  h->setPen(QPen(ring, 2));
  v->setPen(QPen(ring, 2));
  auto* tx = new QGraphicsSimpleTextItem(QString::number(number));
  QFont f = tx->font();
  f.setPointSize(9);
  f.setBold(true);
  tx->setFont(f);
  tx->setBrush(QColor(15, 23, 42));
  const QRectF br = tx->boundingRect();
  tx->setPos(-br.width() * 0.5, -r - br.height() - 1);
  g->addToGroup(fill);
  g->addToGroup(h);
  g->addToGroup(v);
  g->addToGroup(tx);
  g->setPos(pixelX / m_srcScale, pixelY / m_srcScale);
  g->setZValue(50);
  scene()->addItem(g);
  m_marks.append(g);
}

void KaImageView::setMarks(const QVector<QPointF>& pts, const QPointF* pending) {
  clearMarks();
  m_markPixels = pts;  // finished pairs only: those can be dragged
  for (int i = 0; i < pts.size(); ++i)
    addMarkItem(pts[i].x(), pts[i].y(), i + 1, QColor(220, 38, 38));
  if (pending)
    addMarkItem(pending->x(), pending->y(), pts.size() + 1, QColor(234, 179, 8));
}

QPoint KaImageView::viewPosForPixel(double pixelX, double pixelY) const {
  return mapFromScene(QPointF(pixelX / m_srcScale, pixelY / m_srcScale));
}

void KaImageView::fitImage() {
  if (!m_pix) return;
  fitInView(m_pix, Qt::KeepAspectRatio);
  m_fitted = true;
  scheduleDetail();
}

void KaImageView::wheelEvent(QWheelEvent* e) {
  const double s = e->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
  scale(s, s);
  m_fitted = false;
  scheduleDetail();
  emit viewChanged();
}

void KaImageView::mousePressEvent(QMouseEvent* e) {
  if (e->button() == Qt::MiddleButton) {
    m_panning = true;
    m_lastPan = e->pos();
    setCursor(Qt::ClosedHandCursor);
    e->accept();
    return;
  }
  if (e->button() == Qt::LeftButton && m_pix) {
    // Pressing on a numbered mark drags it for fine-tuning instead of picking anew.
    m_dragMark = markAt(e->pos());
    if (m_dragMark >= 0) {
      m_dragPress = e->pos();
      setCursor(Qt::SizeAllCursor);
      e->accept();
      return;
    }
    const QPointF sc = mapToScene(e->pos());
    emit pixelClicked(sc.x() * m_srcScale, sc.y() * m_srcScale);
    e->accept();
    return;
  }
  QGraphicsView::mousePressEvent(e);
}

void KaImageView::mouseMoveEvent(QMouseEvent* e) {
  if (m_dragMark >= 0 && m_dragMark < m_marks.size()) {
    const QPointF src = sourcePixelAt(e->pos());
    m_marks[m_dragMark]->setPos(src.x() / m_srcScale, src.y() / m_srcScale);
    e->accept();
    return;
  }
  if (m_panning) {
    const QPoint d = e->pos() - m_lastPan;
    m_lastPan = e->pos();
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - d.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() - d.y());
    emit viewChanged();
    e->accept();
    return;
  }
  QGraphicsView::mouseMoveEvent(e);
}

void KaImageView::mouseReleaseEvent(QMouseEvent* e) {
  if (m_dragMark >= 0 && e->button() == Qt::LeftButton) {
    const int index = m_dragMark;
    m_dragMark = -1;
    setCursor(Qt::CrossCursor);
    // Barely moved: keep the old click meaning (a new left point there).
    if ((e->pos() - m_dragPress).manhattanLength() < 3) {
      const QPointF sc = mapToScene(m_dragPress);
      emit pixelClicked(sc.x() * m_srcScale, sc.y() * m_srcScale);
    } else {
      const QPointF src = sourcePixelAt(e->pos());
      emit markDragged(index, src.x(), src.y());
    }
    e->accept();
    return;
  }
  if (m_panning && e->button() == Qt::MiddleButton) {
    m_panning = false;
    setCursor(Qt::CrossCursor);
    e->accept();
    return;
  }
  QGraphicsView::mouseReleaseEvent(e);
}

void KaImageView::resizeEvent(QResizeEvent* e) {
  QGraphicsView::resizeEvent(e);
  if (m_fitted) fitImage();
  scheduleDetail();
  emit viewChanged();
}

// markAt, sourcePixelAt, scrollContentsBy and the original-resolution detail:
// KaAlignImageDetail.cpp
