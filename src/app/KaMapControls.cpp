#include "KaMapControls.h"

#include "KaTheme.h"

#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QToolButton>
#include <QVBoxLayout>

#include <qgscoordinatereferencesystem.h>
#include <qgsmapcanvas.h>
#include <qgsmapsettings.h>

#include <cmath>

namespace {

enum class Glyph { Plus, Minus, Fit };

QIcon glyph(Glyph kind) {
  QIcon icon;
  for (qreal dpr : {1.0, 2.0}) {
    QPixmap pixmap(QSize(20, 20) * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(KaTheme::tokens().ink, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (kind != Glyph::Fit) p.drawLine(QPointF(4, 10), QPointF(16, 10));
    if (kind == Glyph::Plus) p.drawLine(QPointF(10, 4), QPointF(10, 16));
    if (kind == Glyph::Fit) {
      QPainterPath corners;
      for (const QPointF& c : {QPointF(3, 3), QPointF(17, 3), QPointF(17, 17), QPointF(3, 17)}) {
        const double sx = c.x() < 10 ? 1 : -1, sy = c.y() < 10 ? 1 : -1;
        corners.moveTo(c + QPointF(0, 5 * sy));
        corners.lineTo(c);
        corners.lineTo(c + QPointF(5 * sx, 0));
      }
      p.drawPath(corners);
    }
    p.end();
    icon.addPixmap(pixmap);
  }
  return icon;
}

QToolButton* button(QWidget* parent, const char* name, Glyph kind, const QString& tip) {
  auto* b = new QToolButton(parent);
  b->setObjectName(QString::fromLatin1(name));
  b->setIcon(glyph(kind));
  b->setIconSize(QSize(20, 20));
  b->setToolTip(tip);
  b->setFixedSize(36, 36);
  b->setFocusPolicy(Qt::NoFocus);
  return b;
}

QString lengthText(double metres) {
  if (metres >= 1000.0) return QStringLiteral("%1 km").arg(metres / 1000.0, 0, 'g', 3);
  return QStringLiteral("%1 m").arg(metres, 0, 'g', 3);
}

}  // namespace

KaMapControls::KaMapControls(QgsMapCanvas* canvas, QWidget* host)
    : QFrame(host), m_canvas(canvas), m_host(host) {
  setObjectName(QStringLiteral("mapControls"));
  setAttribute(Qt::WA_StyledBackground, true);
  auto* column = new QVBoxLayout(this);
  column->setContentsMargins(0, 0, 0, 0);
  column->setSpacing(0);
  auto* zoomIn = button(this, "mapZoomIn", Glyph::Plus, QStringLiteral("확대 (마우스 휠을 앞으로)"));
  auto* zoomOut = button(this, "mapZoomOut", Glyph::Minus, QStringLiteral("축소 (마우스 휠을 뒤로)"));
  auto* fit = button(this, "mapZoomFit", Glyph::Fit, QStringLiteral("조사구역이 화면에 꽉 차게 봅니다"));
  column->addWidget(zoomIn);
  column->addWidget(zoomOut);
  column->addWidget(fit);
  connect(zoomIn, &QToolButton::clicked, this, [this]() { if (m_canvas) m_canvas->zoomIn(); });
  connect(zoomOut, &QToolButton::clicked, this, [this]() { if (m_canvas) m_canvas->zoomOut(); });
  connect(fit, &QToolButton::clicked, this, [this]() {
    if (m_fit) m_fit();
    else if (m_canvas) m_canvas->zoomToFullExtent();
  });
  adjustSize();
  if (m_host) m_host->installEventFilter(this);
  reposition();
  raise();
}

bool KaMapControls::eventFilter(QObject* watched, QEvent* event) {
  if (watched == m_host && event && (event->type() == QEvent::Resize || event->type() == QEvent::Show))
    reposition();
  return QFrame::eventFilter(watched, event);
}

void KaMapControls::reposition() {
  if (!m_host) return;
  move(m_host->width() - width() - 12, 12);
  raise();
}

KaMapScaleBar::KaMapScaleBar(QgsMapCanvas* canvas, QWidget* host)
    : QWidget(host), m_canvas(canvas), m_host(host) {
  setObjectName(QStringLiteral("mapScaleBar"));
  setAttribute(Qt::WA_TransparentForMouseEvents, true);
  setFixedHeight(34);
  if (m_canvas) {
    connect(m_canvas, &QgsMapCanvas::scaleChanged, this, &KaMapScaleBar::refresh);
    connect(m_canvas, &QgsMapCanvas::extentsChanged, this, &KaMapScaleBar::refresh);
    connect(m_canvas, &QgsMapCanvas::destinationCrsChanged, this, &KaMapScaleBar::refresh);
  }
  if (m_host) m_host->installEventFilter(this);
  refresh();
}

bool KaMapScaleBar::eventFilter(QObject* watched, QEvent* event) {
  if (watched == m_host && event && (event->type() == QEvent::Resize || event->type() == QEvent::Show))
    refresh();
  return QWidget::eventFilter(watched, event);
}

void KaMapScaleBar::refresh() {
  if (!m_canvas || m_canvas->width() <= 0) return;
  const QgsCoordinateReferenceSystem crs = m_canvas->mapSettings().destinationCrs();
  const double extent = m_canvas->extent().width();
  if (!crs.isValid() || crs.isGeographic() || !(extent > 0.0)) {
    hide();
    return;
  }
  // Logical pixels, so the bar is right on high-DPI screens too.
  const double perPixel = extent / m_canvas->width();
  const double wanted = perPixel * 120.0;
  const double decade = std::pow(10.0, std::floor(std::log10(wanted)));
  double round = decade;
  for (double step : {2.0, 5.0, 10.0})
    if (step * decade <= wanted) round = step * decade;
  m_metres = round;
  m_pixels = round / perPixel;
  setFixedWidth(int(std::ceil(m_pixels)) + 56);
  reposition();
  show();
  update();
}

void KaMapScaleBar::reposition() {
  if (!m_host) return;
  move(12, m_host->height() - height() - 12);
  raise();
}

void KaMapScaleBar::paintEvent(QPaintEvent*) {
  if (m_pixels <= 0.0) return;
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  const auto& tokens = KaTheme::tokens();
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(255, 255, 255, 225));
  p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 7, 7);
  const QRectF bar(10, 8, m_pixels, 6);
  // Two dark and two light quarters, as on a printed survey scale.
  for (int i = 0; i < 4; ++i) {
    const QRectF part(bar.left() + bar.width() * i / 4.0, bar.top(), bar.width() / 4.0, bar.height());
    p.fillRect(part, i % 2 ? QColor(Qt::white) : tokens.ink);
  }
  p.setPen(QPen(tokens.ink, 1));
  p.setBrush(Qt::NoBrush);
  p.drawRect(bar);
  QFont font = p.font();
  font.setPixelSize(11);
  p.setFont(font);
  p.drawText(QPointF(bar.left() - 2, bar.bottom() + 14), QStringLiteral("0"));
  const QString end = lengthText(m_metres);
  p.drawText(QPointF(bar.right() - 4, bar.bottom() + 14), end);
}

KaMapCornerBar::KaMapCornerBar(QWidget* host) : QFrame(host), m_host(host) {
  setObjectName(QStringLiteral("mapCornerBar"));
  setAttribute(Qt::WA_StyledBackground, true);
  if (m_host) m_host->installEventFilter(this);
}

bool KaMapCornerBar::eventFilter(QObject* watched, QEvent* event) {
  if (watched == m_host && event && (event->type() == QEvent::Resize || event->type() == QEvent::Show))
    reposition();
  return QFrame::eventFilter(watched, event);
}

bool KaMapCornerBar::event(QEvent* event) {
  const bool handled = QFrame::event(event);
  // Children showing or hiding (grid details) change the wanted width.
  if (event && (event->type() == QEvent::LayoutRequest || event->type() == QEvent::Show))
    reposition();
  return handled;
}

void KaMapCornerBar::reposition() {
  if (!m_host) return;
  if (auto* lay = layout()) lay->activate();
  resize(sizeHint());
  move(m_host->width() - width() - 12, m_host->height() - height() - 12);
  raise();
}
