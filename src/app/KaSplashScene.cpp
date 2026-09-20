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
  if (plan == m_rect && qFuzzyCompare(dpr, m_dpr) && !m_features.isEmpty()) return;
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
  m_soil = QImage();
  m_grains.clear();
  m_autoScrape = 1.0;
  m_features.clear();
  QPainterPath house;
  house.addRect(QRectF(w * 0.12, h * 0.2, w * 0.36, h * 0.34));
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

void KaSplashScene::advance(double, double) {
}

bool KaSplashScene::pointerMoved(const QPointF& pos, bool) {
  m_pointerInside = m_rect.contains(pos);
  m_pointer = pos - m_rect.topLeft();
  m_hasLastScrape = false;
  return false;
}

void KaSplashScene::pointerLeft() {
  m_pointerInside = false;
  m_hasLastScrape = false;
}

bool KaSplashScene::uncovered(const QPointF&) const {
  return true;
}

int KaSplashScene::hoveredFeature() const {
  if (!m_pointerInside || !uncovered(m_pointer)) return -1;
  for (int i = 0; i < m_features.size(); ++i)
    if (m_features.at(i).hit.contains(m_pointer)) return i;
  return -1;
}

double KaSplashScene::revealedFraction() const {
  return m_features.isEmpty() ? 0.0 : 1.0;
}
