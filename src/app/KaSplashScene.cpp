#include "KaSplashScene.h"

#include "KaSplashPalette.h"

#include <QPainter>
#include <QPainterPathStroker>
#include <QTransform>

#include <algorithm>
#include <cmath>

using namespace KaSplashPalette;

namespace {
constexpr int kMaxGrains = 180;
constexpr double kPi = 3.14159265358979323846;
}  // namespace

void KaSplashScene::setRect(const QRectF& plan, qreal devicePixelRatio) {
  const qreal dpr = devicePixelRatio > 0 ? devicePixelRatio : 1.0;
  if (plan == m_rect && qFuzzyCompare(dpr, m_dpr) && !m_soil.isNull()) return;
  m_rect = plan;
  m_dpr = dpr;
  rebuild();
}

double KaSplashScene::unitRandom() {
  m_seed = m_seed * 1664525u + 1013904223u;
  return double(m_seed >> 8) / double(1u << 24);
}

void KaSplashScene::addFeature(const QPainterPath& shape, const QString& name,
                               const QString& detail, const QPointF& label, Style style) {
  QPainterPathStroker stroker;
  stroker.setWidth(14);
  QPainterPath hit = stroker.createStroke(shape);
  if (style != Style::Dashed) hit = hit.united(shape);
  m_features.append({shape, hit, name, detail, label, style});
}

void KaSplashScene::rebuild() {
  const double w = m_rect.width(), h = m_rect.height();
  if (w < 1 || h < 1) return;
  m_soil = QImage(QSize(int(std::ceil(w * m_dpr)), int(std::ceil(h * m_dpr))),
                  QImage::Format_ARGB32_Premultiplied);
  m_soil.setDevicePixelRatio(m_dpr);
  m_soil.fill(kSoil);
  m_seed = 0x2545F491u;  // the same soil every launch
  {
    QPainter p(&m_soil);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    for (int i = 0; i < 7; ++i) {  // soil colour changes (토색 차이)
      p.setBrush(withAlpha(i % 2 ? kOchre : kLoam, 0.35));
      p.drawEllipse(QPointF(unitRandom() * w, unitRandom() * h), w * (0.12 + unitRandom() * 0.2),
                    h * (0.08 + unitRandom() * 0.14));
    }
    const QColor shades[] = {kSand, kOchre, kStone, kLoam};
    const int grains = int(w * h / 70);
    for (int i = 0; i < grains; ++i) {
      const double size = 0.8 + unitRandom() * 2.2;
      p.fillRect(QRectF(unitRandom() * w, unitRandom() * h, size, size),
                 withAlpha(shades[i % 4], 0.35 + unitRandom() * 0.5));
    }
    p.setBrush(withAlpha(kStone, 0.8));
    for (int i = 0; i < 26; ++i) {
      const double r = 1.5 + unitRandom() * 3.5;
      p.drawEllipse(QPointF(unitRandom() * w, unitRandom() * h), r * 1.3, r);
    }
  }
  if (m_autoScrape > 0.0) {  // keep what was already scraped after a resize
    const double done = m_autoScrape;
    m_autoScrape = 0.0;
    advance(kScrapeStart + done * kScrapeLength, 0.0);
  }

  m_features.clear();
  QPainterPath house;
  house.addRoundedRect(QRectF(w * 0.12, h * 0.2, w * 0.36, h * 0.34), 8, 8);
  for (const QPointF& q : {QPointF(0.16, 0.25), QPointF(0.44, 0.25), QPointF(0.16, 0.49),
                           QPointF(0.44, 0.49)})
    house.addEllipse(QPointF(w * q.x(), h * q.y()), h * 0.018, h * 0.018);
  addFeature(house, QStringLiteral("1호 주거지"), QStringLiteral("방형 · 주혈 4기"),
             QPointF(w * 0.13, h * 0.17), Style::Outline);
  QPainterPath pitBase;
  pitBase.addEllipse(QPointF(0, 0), w * 0.1, h * 0.13);
  QTransform place;
  place.translate(w * 0.73, h * 0.38);
  place.rotate(17);
  addFeature(place.map(pitBase), QStringLiteral("2호 수혈"), QStringLiteral("원형 수혈"),
             QPointF(w * 0.63, h * 0.2), Style::Outline);
  QPainterPath ditch(QPointF(w * 0.04, h * 0.84));
  ditch.cubicTo(QPointF(w * 0.35, h * 0.66), QPointF(w * 0.6, h * 0.95), QPointF(w * 0.97, h * 0.75));
  addFeature(ditch, QStringLiteral("3호 구"), QStringLiteral("구상유구"), QPointF(w * 0.6, h * 0.94),
             Style::Dashed);
  QPainterPath finds;
  finds.setFillRule(Qt::WindingFill);
  for (const QPointF& q : {QPointF(0.27, 0.34), QPointF(0.34, 0.43), QPointF(0.7, 0.33),
                           QPointF(0.62, 0.82)}) {
    const QPointF c(w * q.x(), h * q.y());
    const double s = h * 0.02;
    finds.addPolygon(QPolygonF({c + QPointF(0, -s), c + QPointF(s, s), c + QPointF(-s, s), c + QPointF(0, -s)}));
  }
  addFeature(finds, QStringLiteral("유물"), QStringLiteral("토기편 4점"), QPointF(), Style::Filled);
}

void KaSplashScene::erase(const QPainterPath& localPath) {
  QPainter p(&m_soil);
  p.setRenderHint(QPainter::Antialiasing);
  p.setCompositionMode(QPainter::CompositionMode_Clear);
  p.fillPath(localPath, Qt::black);
}

void KaSplashScene::spray(const QPointF& localPos, int count, double speed) {
  for (int i = 0; i < count && m_grains.size() < kMaxGrains; ++i) {
    const double angle = -kPi * (0.15 + unitRandom() * 0.7);
    const double v = speed * (0.4 + unitRandom() * 0.8);
    m_grains.append({localPos, QPointF(std::cos(angle) * v, std::sin(angle) * v), 1.0, i % 3});
  }
}

void KaSplashScene::advance(double phase, double dt) {
  if (m_soil.isNull()) return;
  const double scrape = clamp01((phase - kScrapeStart) / kScrapeLength);
  if (scrape > m_autoScrape) {
    m_autoScrape = scrape;
    const double w = m_rect.width(), h = m_rect.height(), slope = h * kSlope;
    const double edge = scrape * (w + slope);
    QPainterPath swept;
    swept.moveTo(-slope - 4, -4);
    swept.lineTo(edge + 4 * kSlope, -4);
    swept.lineTo(edge - slope - 4 * kSlope, h + 4);
    swept.lineTo(-slope - 4, h + 4);
    swept.closeSubpath();
    erase(swept);
    if (scrape < 1.0 && dt > 0.0) {
      for (int i = 0; i < 3; ++i) {
        const double t = unitRandom();
        spray(QPointF(edge - slope * t, h * t), 1, h * 0.5);
      }
    }
  }
  for (Grain& grain : m_grains) {
    grain.pos += grain.velocity * dt;
    grain.velocity.ry() += m_rect.height() * 1.4 * dt;
    grain.life -= dt * 1.5;
  }
  m_grains.erase(std::remove_if(m_grains.begin(), m_grains.end(),
                                [](const Grain& g) { return g.life <= 0.0; }),
                 m_grains.end());
}

bool KaSplashScene::pointerMoved(const QPointF& pos, bool pressed) {
  m_pointerInside = m_rect.contains(pos);
  m_pointer = pos - m_rect.topLeft();
  if (!m_pointerInside || m_soil.isNull()) {
    m_hasLastScrape = false;
    return false;
  }
  m_interacted = true;
  const double radius = m_rect.height() * (pressed ? 0.075 : 0.05);
  QPainterPath brush;
  brush.setFillRule(Qt::WindingFill);
  if (m_hasLastScrape) {
    QPainterPath segment(m_lastScrape);
    segment.lineTo(m_pointer);
    QPainterPathStroker stroker;
    stroker.setWidth(radius * 2);
    stroker.setCapStyle(Qt::RoundCap);
    brush.addPath(stroker.createStroke(segment));
  }
  brush.addEllipse(m_pointer, radius, radius);
  erase(brush);
  spray(m_pointer, pressed ? 6 : 3, m_rect.height() * 0.6);
  m_lastScrape = m_pointer;
  m_hasLastScrape = true;
  return true;
}

void KaSplashScene::pointerLeft() {
  m_pointerInside = false;
  m_hasLastScrape = false;
}

bool KaSplashScene::uncovered(const QPointF& localPos) const {
  const QPoint pixel(int(localPos.x() * m_dpr), int(localPos.y() * m_dpr));
  return !m_soil.valid(pixel) || qAlpha(m_soil.pixel(pixel)) < 60;
}

int KaSplashScene::hoveredFeature() const {
  if (!m_pointerInside || !uncovered(m_pointer)) return -1;
  for (int i = 0; i < m_features.size(); ++i)
    if (m_features.at(i).hit.contains(m_pointer)) return i;
  return -1;
}

double KaSplashScene::revealedFraction() const {
  if (m_soil.isNull()) return 0.0;
  int open = 0, total = 0;
  for (int y = 0; y < m_soil.height(); y += 4)
    for (int x = 0; x < m_soil.width(); x += 4, ++total)
      if (qAlpha(m_soil.pixel(x, y)) < 60) ++open;
  return total ? double(open) / total : 0.0;
}
