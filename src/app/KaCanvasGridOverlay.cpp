#include "KaCrashGuard.h"
#include "KaCanvasGridOverlay.h"
#include "core/CanvasGridMath.h"

#include <QPainter>
#include <cmath>

#include <qgscoordinatetransform.h>
#include <qgsmapcanvas.h>
#include <qgsproject.h>
#include <qgspointxy.h>

KaCanvasGridOverlay::KaCanvasGridOverlay(QgsMapCanvas* canvas)
    : QgsMapCanvasItem(canvas) {
  setZValue(1000);
}

void KaCanvasGridOverlay::setConfig(const Config& c) {
  m_cfg = c;
  setVisible(m_cfg.enabled);
  if (m_cfg.enabled)
    updatePosition();
  update();
  updateCanvas();
}

void KaCanvasGridOverlay::setEnabled(bool on) {
  m_cfg.enabled = on;
  setVisible(on);
  if (on)
    updatePosition();
  update();
  updateCanvas();
}

void KaCanvasGridOverlay::updatePosition() {
  if (!mMapCanvas)
    return;
  setRect(mMapCanvas->extent());
}

void KaCanvasGridOverlay::paint(QPainter* painter) {
  if (!painter || !mMapCanvas || !m_cfg.enabled)
    return;
  if (m_cfg.type == Type::GeographicDms)
    paintGeographic(painter);
  else
    paintProjected(painter);
}

double KaCanvasGridOverlay::stepMeters() const {
  if (!mMapCanvas)
    return m_cfg.stepMeters > 0.0 ? m_cfg.stepMeters : 20.0;
  if (m_cfg.stepMeters > 0.0)
    return m_cfg.stepMeters;
  const double mupp = std::max(mMapCanvas->mapUnitsPerPixel(), 1e-9);
  return CanvasGridMath::niceStepMeters(120.0 * mupp);
}

QgsPointXY KaCanvasGridOverlay::snapToGrid(const QgsPointXY& p) const {
  const double s = stepMeters();
  if (!(s > 1e-9))
    return p;
  const double rad = m_cfg.rotationDeg * 3.14159265358979323846 / 180.0;
  const double c = std::cos(rad);
  const double sn = std::sin(rad);
  const double u = p.x() * c + p.y() * sn;
  const double v = -p.x() * sn + p.y() * c;
  const double us = std::round(u / s) * s;
  const double vs = std::round(v / s) * s;
  return QgsPointXY(us * c - vs * sn, us * sn + vs * c);
}

void KaCanvasGridOverlay::paintProjected(QPainter* p) {
  const QgsRectangle ext = mMapCanvas->extent();
  const double step = stepMeters();
  if (!(step > 0.0))
    return;

  QPen pen(m_cfg.color, std::max(0.5, m_cfg.lineWidth));
  pen.setCosmetic(true);
  pen.setStyle(m_cfg.penStyle);
  p->setPen(pen);
  p->setBrush(Qt::NoBrush);

  const double rad = m_cfg.rotationDeg * 3.14159265358979323846 / 180.0;
  const double c = std::cos(rad);
  const double sn = std::sin(rad);
  auto toUV = [&](double x, double y) {
    return QgsPointXY(x * c + y * sn, -x * sn + y * c);
  };
  auto fromUV = [&](double u, double v) {
    return QgsPointXY(u * c - v * sn, u * sn + v * c);
  };
  double umin = 1e99, umax = -1e99, vmin = 1e99, vmax = -1e99;
  const QgsPointXY corners[4] = {
      QgsPointXY(ext.xMinimum(), ext.yMinimum()), QgsPointXY(ext.xMaximum(), ext.yMinimum()),
      QgsPointXY(ext.xMaximum(), ext.yMaximum()), QgsPointXY(ext.xMinimum(), ext.yMaximum())};
  for (const QgsPointXY& pt : corners) {
    const QgsPointXY uv = toUV(pt.x(), pt.y());
    umin = std::min(umin, uv.x());
    umax = std::max(umax, uv.x());
    vmin = std::min(vmin, uv.y());
    vmax = std::max(vmax, uv.y());
  }
  const double u0 = std::floor(umin / step) * step;
  const double v0 = std::floor(vmin / step) * step;
  const QPointF origin = pos();
  auto drawMapLine = [&](const QgsPointXY& a, const QgsPointXY& b) {
    p->drawLine(toCanvasCoordinates(a) - origin, toCanvasCoordinates(b) - origin);
  };

  int nx = 0;
  for (double u = u0; u <= umax + step * 0.5 && nx < 80; u += step, ++nx)
    drawMapLine(fromUV(u, vmin), fromUV(u, vmax));
  int ny = 0;
  for (double v = v0; v <= vmax + step * 0.5 && ny < 80; v += step, ++ny)
    drawMapLine(fromUV(umin, v), fromUV(umax, v));

  if (!m_cfg.labels)
    return;
  QFont f(QStringLiteral("Malgun Gothic"), m_cfg.fontPt);
  p->setFont(f);
  // 좌표 글씨도 격자 선 색을 따라간다. 다만 흐린 색이라도 읽히도록 불투명하게.
  QColor labelColor = m_cfg.color;
  labelColor.setAlpha(255);
  p->setPen(QPen(labelColor.darker(115), 0));
  auto fmt = [&](double v) {
    if (step >= 1000.0)
      return QStringLiteral("%1 km").arg(v / 1000.0, 0, 'f', 0);
    if (step >= 1.0)
      return QStringLiteral("%1 m").arg(v, 0, 'f', 0);
    return QStringLiteral("%1 m").arg(v, 0, 'f', 1);
  };
  nx = 0;
  for (double u = u0; u <= umax + step * 0.5 && nx < 80; u += step, ++nx) {
    const QPointF top = toCanvasCoordinates(fromUV(u, vmax)) - origin;
    p->drawText(QPointF(top.x() + 2, 12), fmt(u));
  }
  ny = 0;
  for (double v = v0; v <= vmax + step * 0.5 && ny < 80; v += step, ++ny) {
    const QPointF left = toCanvasCoordinates(fromUV(umin, v)) - origin;
    p->drawText(QPointF(4, left.y() - 2), fmt(v));
  }
}

void KaCanvasGridOverlay::paintGeographic(QPainter* p) {
  if (!QgsProject::instance())
    return;
  const QgsCoordinateReferenceSystem dest = mMapCanvas->mapSettings().destinationCrs();
  const QgsCoordinateReferenceSystem wgs(QStringLiteral("EPSG:4326"));
  QgsCoordinateTransform toWgs(dest, wgs, QgsProject::instance()->transformContext());
  QgsCoordinateTransform toMap(wgs, dest, QgsProject::instance()->transformContext());
  toWgs.setBallparkTransformsAreAppropriate(true);
  toMap.setBallparkTransformsAreAppropriate(true);

  const QgsRectangle ext = mMapCanvas->extent();
  QgsPointXY c[4] = {QgsPointXY(ext.xMinimum(), ext.yMinimum()),
                     QgsPointXY(ext.xMaximum(), ext.yMinimum()),
                     QgsPointXY(ext.xMaximum(), ext.yMaximum()),
                     QgsPointXY(ext.xMinimum(), ext.yMaximum())};
  double lon0 = 180, lon1 = -180, lat0 = 90, lat1 = -90;
  for (auto& pt : c) {
    try {
      pt = toWgs.transform(pt);
    } catch (...) {
      KaCrashGuard::logLine(QStringLiteral("[except] app/KaCanvasGridOverlay.cpp:165"));
      return;
    }
    lon0 = std::min(lon0, pt.x());
    lon1 = std::max(lon1, pt.x());
    lat0 = std::min(lat0, pt.y());
    lat1 = std::max(lat1, pt.y());
  }
  const double mupp = std::max(mMapCanvas->mapUnitsPerPixel(), 1e-9);
  const double span = std::max(lon1 - lon0, lat1 - lat0);
  const double step = CanvasGridMath::niceStepDegrees(span * (120.0 * mupp) / std::max(ext.width(), 1.0));

  QPen pen(m_cfg.color, std::max(0.5, m_cfg.lineWidth));
  pen.setCosmetic(true);
  pen.setStyle(m_cfg.penStyle == Qt::SolidLine ? Qt::SolidLine : Qt::DotLine);
  p->setPen(pen);
  const QPointF origin = pos();
  auto mapPt = [&](double lon, double lat) -> QPointF {
    try {
      return toCanvasCoordinates(toMap.transform(QgsPointXY(lon, lat))) - origin;
    } catch (...) {
      KaCrashGuard::logLine(QStringLiteral("[except] app/KaCanvasGridOverlay.cpp:185"));
      return QPointF();
    }
  };

  const double lonStart = std::floor(lon0 / step) * step;
  const double latStart = std::floor(lat0 / step) * step;
  int n = 0;
  for (double lon = lonStart; lon <= lon1 + step * 0.5 && n < 40; lon += step, ++n) {
    QPointF prev;
    bool have = false;
    for (int i = 0; i <= 12; ++i) {
      const double lat = lat0 + (lat1 - lat0) * (i / 12.0);
      const QPointF q = mapPt(lon, lat);
      if (have)
        p->drawLine(prev, q);
      prev = q;
      have = true;
    }
  }
  n = 0;
  for (double lat = latStart; lat <= lat1 + step * 0.5 && n < 40; lat += step, ++n) {
    QPointF prev;
    bool have = false;
    for (int i = 0; i <= 12; ++i) {
      const double lon = lon0 + (lon1 - lon0) * (i / 12.0);
      const QPointF q = mapPt(lon, lat);
      if (have)
        p->drawLine(prev, q);
      prev = q;
      have = true;
    }
  }
  if (!m_cfg.labels)
    return;
  QFont f(QStringLiteral("Malgun Gothic"), m_cfg.fontPt);
  p->setFont(f);
  // 좌표 글씨도 격자 선 색을 따라간다. 다만 흐린 색이라도 읽히도록 불투명하게.
  QColor labelColor = m_cfg.color;
  labelColor.setAlpha(255);
  p->setPen(QPen(labelColor.darker(115), 0));
  auto dms = [](double v, bool lat) {
    const bool neg = v < 0;
    v = std::abs(v);
    const int d = static_cast<int>(v);
    const double mf = (v - d) * 60.0;
    const int m = static_cast<int>(mf);
    const int s = static_cast<int>(std::lround((mf - m) * 60.0));
    const QChar hemi = lat ? (neg ? QLatin1Char('S') : QLatin1Char('N'))
                           : (neg ? QLatin1Char('W') : QLatin1Char('E'));
    return QStringLiteral("%1°%2'%3\"%4").arg(d).arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0')).arg(hemi);
  };
  n = 0;
  for (double lon = lonStart; lon <= lon1 + step * 0.5 && n < 40; lon += step, ++n)
    p->drawText(mapPt(lon, lat1) + QPointF(2, 12), dms(lon, false));
  n = 0;
  for (double lat = latStart; lat <= lat1 + step * 0.5 && n < 40; lat += step, ++n)
    p->drawText(mapPt(lon0, lat) + QPointF(4, -2), dms(lat, true));
}
