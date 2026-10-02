// KaChip: the one status chip shape. Colour comes from KaTheme tokens inside
// paintChip(); the widget only adds QSS-driven size and font.
#include "KaChip.h"

#include "KaIcons.h"
#include "KaTheme.h"

#include <QFontMetrics>
#include <QHash>
#include <QIcon>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPolygonF>
#include <QStyle>

namespace {

constexpr int kPadSide = 8;   // pill edge to glyph, text to pill edge
constexpr int kGlyphPx = 14;
constexpr int kGlyphGap = 2;  // glyph to text: 8 + 14 + 2 = 24, the QSS padding-left

// Marks a chip draws itself when KaIcons has no glyph for the id, so ok/warn/
// danger never appear as a bare coloured pill even before an icon exists.
void drawBuiltInMark(QPainter& p, const QRectF& box, const QString& id, const QColor& ink) {
  p.save();
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(QPen(ink, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.setBrush(Qt::NoBrush);
  const qreal w = box.width();
  const qreal h = box.height();
  const QPointF o = box.topLeft();
  if (id == QLatin1String("check")) {
    p.drawPolyline(QPolygonF{o + QPointF(w * 0.20, h * 0.55), o + QPointF(w * 0.42, h * 0.76),
                             o + QPointF(w * 0.82, h * 0.30)});
  } else if (id == QLatin1String("warn")) {
    p.drawPolygon(QPolygonF{o + QPointF(w * 0.50, h * 0.12), o + QPointF(w * 0.92, h * 0.86),
                            o + QPointF(w * 0.08, h * 0.86)});
    p.drawLine(o + QPointF(w * 0.50, h * 0.40), o + QPointF(w * 0.50, h * 0.60));
    p.setPen(Qt::NoPen);
    p.setBrush(ink);
    p.drawEllipse(o + QPointF(w * 0.50, h * 0.74), 1.1, 1.1);
  } else if (id == QLatin1String("missing")) {
    p.drawEllipse(box.adjusted(1.2, 1.2, -1.2, -1.2));
    p.drawLine(o + QPointF(w * 0.34, h * 0.34), o + QPointF(w * 0.66, h * 0.66));
    p.drawLine(o + QPointF(w * 0.66, h * 0.34), o + QPointF(w * 0.34, h * 0.66));
  } else if (id == QLatin1String("dot")) {
    p.setPen(Qt::NoPen);
    p.setBrush(ink);
    p.drawEllipse(box.center(), w * 0.22, h * 0.22);
  }
  p.restore();
}

// KaIcons::icon(id, ink) tints a 64 px bake per call; chips repaint often.
QPixmap iconPixmap(const QString& id, const QColor& ink, int px, qreal dpr) {
  static QHash<QString, QPixmap> cache;
  const QString key = id + QLatin1Char('/') + ink.name(QColor::HexArgb) + QLatin1Char('/') +
                      QString::number(px) + QLatin1Char('/') + QString::number(dpr);
  const auto found = cache.constFind(key);
  if (found != cache.constEnd()) return *found;
  const QPixmap pixmap = KaIcons::icon(id, ink).pixmap(QSize(px, px), dpr);
  cache.insert(key, pixmap);
  return pixmap;
}

void drawGlyph(QPainter& p, const QRect& box, const QString& id, const QColor& ink) {
  if (KaIcons::hasIcon(id)) {
    p.drawPixmap(box, iconPixmap(id, ink, box.width(), p.device()->devicePixelRatioF()));
    return;
  }
  drawBuiltInMark(p, QRectF(box), id, ink);
}

QString resolvedGlyph(KaChip::Tone tone, const QString& glyphId) {
  return glyphId.isEmpty() ? KaChip::defaultGlyph(tone) : glyphId;
}

}  // namespace

KaChip::KaChip(const QString& text, Tone tone, QWidget* parent) : QLabel(text, parent), m_tone(tone) {
  setProperty("kaChip", QStringLiteral("true"));
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  syncProperties();
}

void KaChip::setTone(Tone tone) {
  if (m_tone == tone) return;
  m_tone = tone;
  syncProperties();
}

void KaChip::setGlyph(const QString& iconId) {
  if (m_glyph == iconId) return;
  m_glyph = iconId;
  syncProperties();
}

bool KaChip::hasGlyph() const { return !resolvedGlyph(m_tone, m_glyph).isEmpty(); }

QSize KaChip::sizeHint() const { return sizeForText(QFontMetrics(chipFont(font())), text(), hasGlyph()); }

QSize KaChip::minimumSizeHint() const { return sizeHint(); }

void KaChip::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event);
  QPainter p(this);
  p.setFont(font());
  paintChip(p, rect(), text(), m_tone, m_glyph);
}

void KaChip::syncProperties() {
  setProperty("kaTone", toneName(m_tone));
  setProperty("kaGlyph", hasGlyph() ? QStringLiteral("true") : QStringLiteral("false"));
  // Dynamic properties only reach QSS selectors after a re-polish.
  style()->unpolish(this);
  style()->polish(this);
  updateGeometry();
  update();
}

void KaChip::paintChip(QPainter& p, const QRect& rect, const QString& text, Tone tone, const QString& glyphId) {
  const auto& metrics = KaTheme::buttonMetrics();
  const int height = metrics.chipHeight;
  const QRect pill(rect.left(), rect.top() + (rect.height() - height) / 2, rect.width(), height);
  const QColor ink = toneInk(tone);
  p.save();
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(Qt::NoPen);
  p.setBrush(toneSurface(tone));
  p.drawRoundedRect(pill, metrics.chipRadius, metrics.chipRadius);
  // A faint edge in the tone's ink keeps a light chip visible on a white card.
  // Same alpha for every tone, so the shape mask never depends on the tone.
  QColor edge = ink;
  edge.setAlpha(46);
  p.setPen(QPen(edge, 1.0));
  p.setBrush(Qt::NoBrush);
  p.drawRoundedRect(QRectF(pill).adjusted(0.5, 0.5, -0.5, -0.5), metrics.chipRadius - 0.5, metrics.chipRadius - 0.5);

  int x = pill.left() + kPadSide;
  const QString glyph = resolvedGlyph(tone, glyphId);
  if (!glyph.isEmpty()) {
    const QRect box(x, pill.top() + (height - kGlyphPx) / 2, kGlyphPx, kGlyphPx);
    drawGlyph(p, box, glyph, ink);
    x += kGlyphPx + kGlyphGap;
  }
  p.setFont(chipFont(p.font()));
  p.setPen(ink);
  const QRect textRect(x, pill.top(), pill.right() - kPadSide - x + 1, height);
  p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, text);
  p.restore();
}

QSize KaChip::sizeForText(const QFontMetrics& fm, const QString& text, bool hasGlyph) {
  const int lead = hasGlyph ? kPadSide + kGlyphPx + kGlyphGap : kPadSide;
  return QSize(lead + fm.horizontalAdvance(text) + kPadSide, KaTheme::buttonMetrics().chipHeight);
}

QFont KaChip::chipFont(QFont base) {
  base.setPixelSize(12);  // 11 read too small in Malgun Gothic on a 100 % field screen
  base.setWeight(QFont::Bold);
  return base;
}

QColor KaChip::toneInk(Tone tone) {
  const auto& t = KaTheme::tokens();
  switch (tone) {
    case Tone::Ok: return t.ok;
    case Tone::Warn: return t.warn;
    case Tone::Danger: return t.danger;
    case Tone::Accent: return t.accentDeep;
    case Tone::Neutral: break;
  }
  return t.inkMuted;
}

QColor KaChip::toneSurface(Tone tone) {
  const auto& t = KaTheme::tokens();
  switch (tone) {
    case Tone::Ok: return t.successSurface;
    case Tone::Warn: return t.warnSurface;
    case Tone::Danger: return t.dangerSurface;
    case Tone::Accent: return t.accentWash;
    case Tone::Neutral: break;
  }
  return t.altRow;
}

QString KaChip::defaultGlyph(Tone tone) {
  switch (tone) {
    case Tone::Ok: return QStringLiteral("check");
    case Tone::Warn: return QStringLiteral("warn");
    case Tone::Danger: return QStringLiteral("missing");
    case Tone::Accent:
    case Tone::Neutral: break;
  }
  return QString();
}

QString KaChip::toneName(Tone tone) {
  switch (tone) {
    case Tone::Ok: return QStringLiteral("ok");
    case Tone::Warn: return QStringLiteral("warn");
    case Tone::Danger: return QStringLiteral("danger");
    case Tone::Accent: return QStringLiteral("accent");
    case Tone::Neutral: break;
  }
  return QStringLiteral("neutral");
}

KaChip::Tone KaChip::toneFromName(const QString& name) {
  if (name == QLatin1String("ok")) return Tone::Ok;
  if (name == QLatin1String("warn")) return Tone::Warn;
  if (name == QLatin1String("danger")) return Tone::Danger;
  if (name == QLatin1String("accent")) return Tone::Accent;
  return Tone::Neutral;
}
