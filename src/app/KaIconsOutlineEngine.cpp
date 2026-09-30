// Outline style rendering: a QIconEngine draws the glyph at the size a widget asks for, so a
// 32 px ribbon chip and a 14 px status chip both get a crisp stroke instead of a scaled bake.
#include "KaIconsOutline.h"

#include "KaTheme.h"

#include <QHash>
#include <QIcon>
#include <QIconEngine>
#include <QPainterPath>

namespace KaIconsOutline {

Glyph outlineGlyphFor(const QString& id) {
  if (Glyph g = outlineGlyphA(id)) return g;
  if (Glyph g = outlineGlyphB(id)) return g;
  return outlineGlyphUi(id);
}

bool hasOutlineGlyph(const QString& id) { return outlineGlyphFor(id) != nullptr; }

namespace {

constexpr qreal kUnits = 64.0;

QColor neutralGrey() {
  const int grey = qGray(KaTheme::iconPalette().disabled.rgb());
  return QColor(grey, grey, grey);
}

void setInkPen(QPainter& p, const QColor& ink) {
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(QPen(ink, KaIconMetrics::outlineStrokeWidth(KaIconMetrics::kOutlineStroke, p.worldTransform().m11()),
                Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.setBrush(Qt::NoBrush);
}

void fillRounded(QPainter& p, const QRectF& rect, qreal radius, const QColor& fill) {
  p.save();
  p.setPen(Qt::NoPen);
  p.setBrush(fill);
  p.drawRoundedRect(rect, radius, radius);
  p.restore();
}

// Multiplies every pixel's alpha, keeping its colour exact (disabled monochrome icons).
QImage withOpacity(const QImage& source, qreal opacity) {
  QImage out = source.convertToFormat(QImage::Format_ARGB32);
  for (int y = 0; y < out.height(); ++y) {
    auto* row = reinterpret_cast<QRgb*>(out.scanLine(y));
    for (int x = 0; x < out.width(); ++x)
      row[x] = qRgba(qRed(row[x]), qGreen(row[x]), qBlue(row[x]), qRound(qAlpha(row[x]) * opacity));
  }
  return out;
}

class OutlineIconEngine : public QIconEngine {
 public:
  OutlineIconEngine(QString id, Glyph glyph, FallbackImage fallback, QColor fixedInk)
      : m_id(std::move(id)), m_glyph(glyph), m_fallback(fallback), m_fixedInk(std::move(fixedInk)) {}

  void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) override {
    const qreal dpr = painter->device()->devicePixelRatio();
    QPixmap pm = scaledPixmap(rect.size() * dpr, mode, state, dpr);
    pm.setDevicePixelRatio(dpr);
    painter->drawPixmap(rect, pm);
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
  QString key() const override { return QStringLiteral("KaIconsOutline"); }
  QIconEngine* clone() const override { return new OutlineIconEngine(m_id, m_glyph, m_fallback, m_fixedInk); }

 private:
  QImage render(int px, QIcon::Mode mode, QIcon::State state) const {
    QImage image(px, px, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(px / kUnits, px / kUnits);
    const auto& palette = KaTheme::iconPalette();
    const auto& tokens = KaTheme::tokens();
    const bool fixed = m_fixedInk.isValid();
    const bool disabled = mode == QIcon::Disabled;
    const bool on = state == QIcon::On;
    const bool unsaved = !fixed && m_id == QLatin1String("save_unsaved");
    QColor ink = fixed ? m_fixedInk : disabled ? neutralGrey() : on ? tokens.accent : palette.ink;
    if (unsaved) {
      // 미저장 저장 단추: the one solid tile in this style, white glyph and a warn dot.
      fillRounded(p, QRectF(2, 2, 60, 60), 14, disabled ? QColor(190, 190, 190) : tokens.accent);
      ink = Qt::white;
    } else if (on && !fixed) {
      fillRounded(p, QRectF(4, 4, 56, 56), 12, disabled ? QColor(232, 232, 232) : tokens.accentWash);
    }
    paintGlyph(p, ink, px);
    if (unsaved) {
      p.setPen(QPen(disabled ? QColor(120, 120, 120) : tokens.warn, 1.6));
      p.setBrush(disabled ? QColor(235, 235, 235) : tokens.warnSurface);
      p.drawEllipse(QPointF(50, 14), 5, 5);
    }
    if (!fixed && mode == QIcon::Selected) {
      p.setPen(QPen(palette.selected, 2.5));
      p.setBrush(Qt::NoBrush);
      p.drawRoundedRect(QRectF(3, 3, 58, 58), 9, 9);
    }
    p.end();
    return fixed && disabled ? withOpacity(image, 0.45) : image;
  }

  void paintGlyph(QPainter& p, const QColor& ink, int px) const {
    p.save();
    if (m_glyph) {
      setInkPen(p, ink);
      m_glyph(p);
    } else if (m_fallback) {
      // Legacy drawing without its tile, tinted to the state ink.
      const QImage flat = m_fallback(m_id, px > 64 ? 128 : 64);
      p.setRenderHint(QPainter::SmoothPixmapTransform, true);
      p.drawImage(QRectF(0, 0, kUnits, kUnits), tintToInk(flat, ink));
    }
    p.restore();
  }

  QString m_id;
  Glyph m_glyph;
  FallbackImage m_fallback;
  QColor m_fixedInk;
  mutable QHash<QString, QPixmap> m_cache;
};

}  // namespace

QIcon outlineIcon(const QString& id, Glyph glyph, FallbackImage fallback, const QColor& fixedInk) {
  return QIcon(new OutlineIconEngine(id, glyph, fallback, fixedInk));
}

QPixmap renderGlyph(Glyph glyph, const QColor& ink, int px, qreal dpr) {
  const int device = qMax(1, qRound(px * dpr));
  QImage image(device, device, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);
  if (glyph) {
    QPainter p(&image);
    p.scale(device / kUnits, device / kUnits);
    setInkPen(p, ink);
    glyph(p);
  }
  QPixmap pm = QPixmap::fromImage(image);
  pm.setDevicePixelRatio(dpr);
  return pm;
}

QImage tintToInk(const QImage& source, const QColor& ink) {
  const QImage src = source.convertToFormat(QImage::Format_ARGB32);
  QImage out(src.size(), QImage::Format_ARGB32);
  out.fill(Qt::transparent);
  // Preserve internal outlines instead of flattening a filled drawing into a silhouette.
  const qreal inkRange = qMax(1, 255 - qGray(KaTheme::iconPalette().ink.rgb()));
  for (int y = 0; y < src.height(); ++y) {
    for (int x = 0; x < src.width(); ++x) {
      const QColor pixel = src.pixelColor(x, y);
      const qreal shade = qMin(1.0, (255 - qGray(pixel.rgb())) / inkRange);
      QColor tint = ink;
      tint.setAlpha(qRound(ink.alphaF() * pixel.alpha() * shade));
      out.setPixelColor(x, y, tint);
    }
  }
  return out;
}

}  // namespace KaIconsOutline
