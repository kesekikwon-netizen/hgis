// KaImageView: mark grabbing and the original-resolution detail layer used by the align
// panel for scans that are shown from a reduced preview (split from KaImageView.cpp).
#include "KaImageView.h"

#include <QGraphicsItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QPixmap>
#include <QPolygonF>
#include <QTimer>
#include <QTransform>
#include <algorithm>
#include <cmath>

namespace {
constexpr double kGrabPixels = 10.0;
constexpr int kDetailDelayMs = 200;
// Largest window read at once (output pixels): 4096 x 4096 x 3 bytes, 48 MB of RGB.
constexpr double kDetailMaxPixels = 4096.0 * 4096.0;
}  // namespace

int KaImageView::markAt(const QPoint& viewPos) const {
  int best = -1;
  double bestD = kGrabPixels;
  for (int i = 0; i < m_markPixels.size(); ++i) {
    const QPoint p = viewPosForPixel(m_markPixels[i].x(), m_markPixels[i].y());
    const double d = std::hypot(double(p.x() - viewPos.x()), double(p.y() - viewPos.y()));
    if (d <= bestD) {
      bestD = d;
      best = i;
    }
  }
  return best;
}

QPointF KaImageView::sourcePixelAt(const QPoint& viewPos) const {
  const QPointF sc = mapToScene(viewPos);
  double x = sc.x() * m_srcScale;
  double y = sc.y() * m_srcScale;
  if (m_srcSize.isValid()) {
    x = std::clamp(x, 0.0, double(m_srcSize.width()));
    y = std::clamp(y, 0.0, double(m_srcSize.height()));
  }
  return {x, y};
}

void KaImageView::scrollContentsBy(int dx, int dy) {
  QGraphicsView::scrollContentsBy(dx, dy);
  scheduleDetail();
}

void KaImageView::setDetailReader(DetailReader reader) {
  m_detailReader = std::move(reader);
  m_detailWindow = QRect();
  scheduleDetail();
}

bool KaImageView::detailShown() const {
  return m_detail && m_detail->isVisible();
}

void KaImageView::hideDetail() {
  if (m_detail) m_detail->hide();
  m_detailWindow = QRect();
}

void KaImageView::scheduleDetail() {
  if (!m_detailReader || !m_pix || m_srcScale <= 1.0) return;
  if (!m_detailTimer) {
    m_detailTimer = new QTimer(this);
    m_detailTimer->setSingleShot(true);
    m_detailTimer->setInterval(kDetailDelayMs);
    connect(m_detailTimer, &QTimer::timeout, this, &KaImageView::refreshDetail);
  }
  m_detailTimer->start();
}

void KaImageView::refreshDetail() {
  if (!m_detailReader || !m_pix || !scene() || m_srcScale <= 1.0 || !m_srcSize.isValid()) return;
  // Screen pixels per preview pixel. At or below 1 the preview already shows all it can.
  const double zoom = transform().m11();
  if (zoom <= 1.05) {
    hideDetail();
    return;
  }
  const QRectF visible = mapToScene(viewport()->rect()).boundingRect().intersected(
      m_pix->mapRectToScene(m_pix->boundingRect()));
  const QRect window =
      QRectF(visible.x() * m_srcScale, visible.y() * m_srcScale, visible.width() * m_srcScale,
             visible.height() * m_srcScale)
          .toAlignedRect()
          .intersected(QRect(QPoint(0, 0), m_srcSize));
  if (window.isEmpty()) {
    hideDetail();
    return;
  }
  // As many pixels as the screen shows, never more than the original has.
  const double perSource = std::min(1.0, zoom / m_srcScale);
  double ow = std::ceil(window.width() * perSource);
  double oh = std::ceil(window.height() * perSource);
  if (ow * oh > kDetailMaxPixels) {
    const double k = std::sqrt(kDetailMaxPixels / (ow * oh));
    ow = std::floor(ow * k);
    oh = std::floor(oh * k);
  }
  const QSize out(std::max(1, int(ow)), std::max(1, int(oh)));
  if (window == m_detailWindow && out == m_detailOut && detailShown()) return;
  const QImage img = m_detailReader(window, out);
  if (img.isNull()) {
    hideDetail();
    return;
  }
  if (!m_detail) {
    m_detail = scene()->addPixmap(QPixmap());
    m_detail->setZValue(1);  // above the preview (0), under the marks (50)
    m_detail->setTransformationMode(Qt::SmoothTransformation);
  }
  m_detail->setPixmap(QPixmap::fromImage(img));
  m_detail->setPos(window.x() / m_srcScale, window.y() / m_srcScale);
  m_detail->setTransform(QTransform::fromScale(window.width() / m_srcScale / img.width(),
                                               window.height() / m_srcScale / img.height()));
  m_detail->show();
  m_detailWindow = window;
  m_detailOut = out;
}
