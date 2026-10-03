#include "KaSplashStrata.h"

#include "KaSplashCredits.h"
#include "KaSplashPalette.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

#include <algorithm>
#include <array>

namespace KaSplashStrata {
namespace {

// Everything is laid out on the notice's 700 x 400 card and scaled with it.
constexpr double kWidth = 700.0, kHeight = 400.0;
// A notice shrunk for a small screen has a card wider than 700:400; the strata run on past the edge.
constexpr double kOverhang = 120.0;

const QColor kPaper(0xFA, 0xF9, 0xF5);
const QColor kSlate(0x14, 0x14, 0x13);
const QColor kInk2(0x3D, 0x3D, 0x3A);
const QColor kMuted(0x5E, 0x5D, 0x59);
const QColor kClay(0xD9, 0x77, 0x57);
const QColor kBedrock(0x26, 0x26, 0x24);
const QColor kOat(0xE3, 0xDA, 0xCC);

// The top edge of one stratum: a start point, one full cubic, then two more whose first control
// mirrors the control before it (so the edge stays smooth): x0 y0, c1 c2 p1, c2 p2, c2 p3.
struct Stratum {
  QColor fill;
  std::array<double, 16> edge;
};
// Bedrock first, topsoil last (the ground line is the topsoil's edge).
const std::array<Stratum, 5> kStrata = {{
    {kBedrock, {0, 344, 120, 338, 200, 350, 290, 342, 460, 332, 550, 338, 650, 330, 700, 334}},
    {QColor(0xB5, 0x57, 0x3A), {0, 316, 110, 308, 190, 322, 280, 312, 450, 300, 540, 306, 650, 298, 700, 302}},
    {kClay, {0, 286, 100, 276, 180, 294, 270, 282, 440, 268, 530, 276, 650, 266, 700, 268}},
    {kOat, {0, 256, 90, 248, 170, 262, 262, 252, 432, 236, 522, 244, 642, 232, 700, 236}},
    {QColor(0x78, 0x8C, 0x5D), {0, 240, 90, 232, 170, 246, 262, 236, 432, 220, 522, 228, 642, 216, 700, 220}},
}};

double ease(double x) {
  x = std::clamp(x, 0.0, 1.0);
  return 1.0 - (1.0 - x) * (1.0 - x) * (1.0 - x);
}

QPainterPath edgePath(const std::array<double, 16>& p) {
  QPainterPath path(QPointF(p[0], p[1]));
  path.cubicTo(p[2], p[3], p[4], p[5], p[6], p[7]);
  path.cubicTo(2 * p[6] - p[4], 2 * p[7] - p[5], p[8], p[9], p[10], p[11]);
  path.cubicTo(2 * p[10] - p[8], 2 * p[11] - p[9], p[12], p[13], p[14], p[15]);
  return path;
}

QFont serifFont(double pixels) {
  QFont font(QStringLiteral("Noto Serif KR"));
  font.setFamilies({QStringLiteral("Noto Serif KR"), QStringLiteral("Batang"), QStringLiteral("Malgun Gothic")});
  font.setPixelSize(int(pixels + 0.5));
  font.setWeight(QFont::Medium);
  return font;
}

// A notice line is never cut off: the text shrinks to the width instead.
void drawFitted(QPainter& painter, QFont font, const QString& text, const QPointF& baseline, double width) {
  while (font.pixelSize() > 8 && QFontMetricsF(font).horizontalAdvance(text) > width)
    font.setPixelSize(font.pixelSize() - 1);
  painter.setFont(font);
  painter.drawText(baseline, text);
}

void paintSection(QPainter& painter, double seconds) {
  // Topsoil first, so each lower stratum covers the foot of the one above it.
  for (int i = int(kStrata.size()) - 1; i >= 0; --i) {
    QPainterPath body = edgePath(kStrata[size_t(i)].edge);
    body.lineTo(kWidth + kOverhang, body.currentPosition().y());
    body.lineTo(kWidth + kOverhang, kHeight);
    body.lineTo(0, kHeight);
    body.closeSubpath();
    painter.fillPath(body, kStrata[size_t(i)].fill);
  }
  if (const double shown = pitReveal(seconds); shown > 0.0) {
    QPainterPath cut(QPointF(452, 231));
    cut.cubicTo(456, 266, 470, 298, 500, 302);
    cut.cubicTo(530, 306, 544, 268, 548, 226);
    QPainterPath fill = cut;
    fill.cubicTo(520, 226, 480, 230, 452, 231);
    painter.setOpacity(shown);
    painter.fillPath(fill, QColor(0x4D, 0x4C, 0x48));
    painter.setPen(QPen(KaSplashPalette::withAlpha(kOat, 0.45), 1.2));
    QPainterPath lens(QPointF(462, 262));
    lens.cubicTo(482, 270, 520, 268, 540, 258);
    lens.moveTo(472, 284);
    lens.cubicTo(490, 290, 514, 288, 530, 280);
    painter.drawPath(lens);
    painter.strokePath(cut, QPen(kSlate, 1.8, Qt::SolidLine, Qt::RoundCap));
    painter.setOpacity(1.0);
  }
  if (const double drawn = groundLineReveal(seconds); drawn > 0.0) {
    QPainterPath ground = edgePath(kStrata.back().edge);
    ground.lineTo(kWidth + kOverhang, ground.currentPosition().y());
    QPen pen(kSlate, 1.8, Qt::SolidLine, Qt::RoundCap);
    const double length = ground.length() / pen.widthF();  // dash lengths are in pen widths
    pen.setDashPattern({length * drawn, length * 4.0});
    painter.strokePath(ground, pen);
    // The level string above the section, with its two pegs and the reading.
    painter.setOpacity(std::clamp((drawn - 0.35) / 0.4, 0.0, 1.0));
    painter.setPen(QPen(kSlate, 1.0, Qt::DashLine));
    painter.drawLine(QPointF(396, 204), QPointF(664, 204));
    painter.setPen(QPen(kSlate, 1.2));
    painter.drawLine(QPointF(396, 198), QPointF(396, 228));
    painter.drawLine(QPointF(664, 198), QPointF(664, 222));
    QFont reading(QStringLiteral("Consolas"));
    reading.setPixelSize(10);
    painter.setFont(reading);
    painter.setPen(kMuted);
    painter.drawText(QRectF(430, 184, 200, 14), Qt::AlignHCenter | Qt::AlignVCenter, QStringLiteral("EL 62.0 m"));
    painter.setOpacity(1.0);
  }
  if (const double landed = markerReveal(seconds); landed > 0.0) {
    painter.save();
    painter.translate(610, 214);
    painter.scale(0.3 + 0.7 * landed, 0.3 + 0.7 * landed);
    painter.setOpacity(landed);
    QPainterPath mark(QPointF(0, -9));
    mark.lineTo(8, 5);
    mark.lineTo(-8, 5);
    mark.closeSubpath();
    painter.fillPath(mark, kPaper);
    painter.strokePath(mark, QPen(kSlate, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setPen(Qt::NoPen);
    painter.setBrush(kSlate);
    painter.drawEllipse(QPointF(0, 0.5), 1.6, 1.6);
    painter.restore();
  }
}

}  // namespace

double pitReveal(double seconds) { return ease(seconds / 0.45); }
double groundLineReveal(double seconds) { return ease((seconds - 0.25) / 0.75); }
double markerReveal(double seconds) { return ease((seconds - 0.95) / 0.3); }

QColor paper() { return kPaper; }
QColor bedrock() { return kBedrock; }
QColor noticeInk() { return KaSplashPalette::withAlpha(kPaper, 0.96); }

void paint(QPainter& painter, const QRectF& card, double radius, const QPixmap& icon, const Frame& frame) {
  painter.save();
  painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing);
  QPainterPath outline;
  outline.addRoundedRect(card, radius, radius);
  painter.setClipPath(outline);
  painter.fillPath(outline, kPaper);
  painter.translate(card.topLeft());
  const double unit = card.height() / kHeight;
  painter.scale(unit, unit);

  paintSection(painter, frame.still ? 10.0 : frame.seconds);

  if (!icon.isNull())
    painter.drawPixmap(QRectF(44, 38, 44, 44), icon, QRectF(icon.rect()));
  painter.setPen(kSlate);
  painter.setFont(serifFont(60));
  painter.drawText(QPointF(42, 142), KaSplashCredits::productName());
  painter.setPen(kMuted);
  painter.setFont(KaSplashPalette::uiFont(14));
  painter.drawText(QPointF(44, 176), KaSplashCredits::productSubtitle(frame.version));
  painter.setPen(kInk2);
  painter.setFont(KaSplashPalette::uiFont(12.5));
  painter.drawText(QPointF(44, 210), frame.status);
  // A hairline that fills as the reading interval passes.
  painter.fillRect(QRectF(44, 221, 200, 2), QColor(0xE8, 0xE6, 0xDC));
  painter.fillRect(QRectF(44, 221, 200 * std::clamp(frame.progress, 0.0, 1.0), 2), kClay);

  // The two notice lines sit on the bedrock in paper ink, worded exactly as on the stock card.
  painter.setPen(noticeInk());
  drawFitted(painter, KaSplashPalette::uiFont(11.5), KaSplashCredits::copyrightLine(), QPointF(44, 372), kWidth - 88);
  painter.setPen(KaSplashPalette::withAlpha(kPaper, 0.86));
  drawFitted(painter, KaSplashPalette::lightFont(11), KaSplashCredits::dataLine(), QPointF(44, 389), kWidth - 88);
  painter.restore();
  // A hairline edge so the paper card reads against a light desktop.
  painter.save();
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(QPen(QColor(31, 30, 29, 40), 1.0));
  painter.setBrush(Qt::NoBrush);
  painter.drawRoundedRect(card.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
  painter.restore();
}

}  // namespace KaSplashStrata
