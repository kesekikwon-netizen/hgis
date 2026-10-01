// Mockup glyph style rendering: a QIconEngine draws the tile, the Lucide SVG and the unsaved
// dot at the size a widget asks for, so a 32 px ribbon chip, a 150 % screen and a 16 px
// chip all get crisp edges instead of a scaled bake. Colours come from lookFor().
#include "KaIconsMockup.h"

#include <QFile>
#include <QHash>
#include <QIconEngine>
#include <QImage>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#include <QPainterPath>
#include <QSvgRenderer>
#include <QtMath>

namespace KaIconsMockup {

namespace {

// The mockup is measured on a 32 px tile; every other size scales these proportionally.
constexpr qreal kTileUnits = 32.0;
constexpr qreal kTileRadius = 0.25;  // of the tile size
constexpr qreal kGlyphUnits = 18.0;  // glyph box inside the tile
constexpr qreal kBorderUnits = 2.0;  // checked-tile border
constexpr qreal kDotDiameter = 7.0;  // including its white ring
constexpr qreal kDotRing = 1.5;
constexpr qreal kDotInset = 6.0;  // centre distance from the tile's top and right edge

QColor rgb(QRgb value) { return QColor::fromRgb(value); }

// The SVG text of one resource, read once; `currentColor` is replaced per render.
QByteArray svgSource(const QString& path) {
  static QHash<QString, QByteArray> cache;
  static QMutex mutex;
  QMutexLocker lock(&mutex);
  const auto found = cache.constFind(path);
  if (found != cache.constEnd()) return *found;
  QFile file(path);
  const QByteArray data = file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
  cache.insert(path, data);
  return data;
}

void drawTile(QPainter& p, qreal px, const Look& look) {
  const qreal radius = px * kTileRadius;
  QPainterPath tile;
  tile.addRoundedRect(QRectF(0, 0, px, px), radius, radius);
  p.setPen(Qt::NoPen);
  p.setBrush(look.tile);
  p.drawPath(tile);
  if (!look.tileBorder.isValid()) return;
  // The stroke is centred on a rect inset by half its width, so it stays inside the tile.
  const qreal width = px * kBorderUnits / kTileUnits;
  QPainterPath ring;
  ring.addRoundedRect(QRectF(width / 2, width / 2, px - width, px - width), radius - width / 2, radius - width / 2);
  p.setPen(QPen(look.tileBorder, width));
  p.setBrush(Qt::NoBrush);
  p.drawPath(ring);
}

void drawGlyph(QPainter& p, qreal px, const QByteArray& svg, const Look& look) {
  if (svg.isEmpty()) return;
  QSvgRenderer renderer(QByteArray(svg).replace("currentColor", look.glyph.name().toLatin1()));
  if (!renderer.isValid()) return;
  const qreal side = px * kGlyphUnits / kTileUnits;
  const QRectF box((px - side) / 2, (px - side) / 2, side, side);
  if (look.glyphOpacity >= 1.0) {
    renderer.render(&p, box);
    return;
  }
  // A faded glyph is one layer: strokes that cross must not darken each other.
  QImage layer(qCeil(px), qCeil(px), QImage::Format_ARGB32_Premultiplied);
  layer.fill(Qt::transparent);
  QPainter lp(&layer);
  lp.setRenderHint(QPainter::Antialiasing, true);
  renderer.render(&lp, box);
  lp.end();
  p.save();
  p.setOpacity(look.glyphOpacity);
  p.drawImage(QPointF(0, 0), layer);
  p.restore();
}

void drawDot(QPainter& p, qreal px) {
  const qreal scale = px / kTileUnits;
  p.setPen(QPen(Qt::white, kDotRing * scale));
  p.setBrush(rgb(0xF2A33A));
  // The ring is centred on the ellipse edge: the outer diameter is kDotDiameter.
  const qreal radius = (kDotDiameter - kDotRing) / 2 * scale;
  p.drawEllipse(QPointF(px - kDotInset * scale, kDotInset * scale), radius, radius);
}

class MockupIconEngine : public QIconEngine {
 public:
  MockupIconEngine(QString svgPath, bool strong) : m_svgPath(std::move(svgPath)), m_strong(strong) {}

  void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) override {
    const qreal dpr = painter->device()->devicePixelRatio();
    const int side = qMin(rect.width(), rect.height());
    const QRect target(rect.x() + (rect.width() - side) / 2, rect.y() + (rect.height() - side) / 2, side, side);
    QPixmap pm = scaledPixmap(QSize(side, side) * dpr, mode, state, dpr);
    pm.setDevicePixelRatio(dpr);
    painter->drawPixmap(target, pm);
  }
  QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override {
    return scaledPixmap(size, mode, state, 1.0);
  }
  // size is in device pixels (QIcon::pixmap passes size * dpr); the result has that many pixels.
  QPixmap scaledPixmap(const QSize& size, QIcon::Mode mode, QIcon::State state, qreal scale) override {
    Q_UNUSED(scale);
    const int px = qMax(1, qMin(size.width(), size.height()));
    const QString key = QString::number(px) + QLatin1Char('/') + QString::number(int(mode)) + QLatin1Char('/') +
                        QString::number(int(state));
    const auto found = m_cache.constFind(key);
    if (found != m_cache.constEnd()) return *found;
    const QPixmap pm = QPixmap::fromImage(render(px, mode, state));
    m_cache.insert(key, pm);
    return pm;
  }
  QSize actualSize(const QSize& size, QIcon::Mode, QIcon::State) override { return size; }
  QList<QSize> availableSizes(QIcon::Mode, QIcon::State) override {
    return {QSize(16, 16), QSize(20, 20), QSize(24, 24), QSize(32, 32), QSize(48, 48), QSize(64, 64), QSize(128, 128)};
  }
  QString key() const override { return QStringLiteral("KaIconsMockup"); }
  QIconEngine* clone() const override { return new MockupIconEngine(m_svgPath, m_strong); }

 private:
  QImage render(int px, QIcon::Mode mode, QIcon::State state) const {
    const Look look = lookFor(mode, state, m_strong);
    QImage image(px, px, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing, true);
    drawTile(p, px, look);
    drawGlyph(p, px, svgSource(m_svgPath), look);
    if (look.dot) drawDot(p, px);
    p.end();
    return image;
  }

  QString m_svgPath;
  bool m_strong;
  mutable QHash<QString, QPixmap> m_cache;
};

}  // namespace

Look lookFor(QIcon::Mode mode, QIcon::State state, bool strong) {
  Look look;
  look.tile = rgb(0xE4EAED);
  look.glyph = rgb(0x2B4858);
  if (mode == QIcon::Disabled) {
    look.tile = rgb(0xEEF1F3);
    look.glyphOpacity = 0.45;
  } else if (strong) {
    look.tile = rgb(0x206CB0);
    look.glyph = Qt::white;
    look.dot = true;
  } else if (state == QIcon::On) {
    look.tile = rgb(0xE0ECF8);
    look.tileBorder = rgb(0x1A68B0);
    look.glyph = rgb(0x105088);
  } else if (mode == QIcon::Active) {
    look.tile = rgb(0xD9E2E7);
  }
  return look;
}

QIcon mockupIcon(const QString& id, bool strong) {
  const QString path = svgPathFor(id);
  if (path.isEmpty()) return QIcon();
  return QIcon(new MockupIconEngine(path, strong));
}

}  // namespace KaIconsMockup
