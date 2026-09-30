#include "KaCrashGuard.h"
#include "core/KaLogExcept.h"
#include "KaCanvasGridOverlay.h"
#include "core/CanvasGridMath.h"

#include <QPainter>
#include <algorithm>
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
  double x = p.x();
  double y = p.y();
  CanvasGridMath::snapToNode({m_cfg.originX, m_cfg.originY, m_cfg.rotationDeg}, stepMeters(), &x, &y);
  return QgsPointXY(x, y);
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

  const CanvasGridMath::Frame frame{m_cfg.originX, m_cfg.originY, m_cfg.rotationDeg};
  auto fromUV = [&frame](double u, double v) {
    double x = 0.0, y = 0.0;
    CanvasGridMath::toMap(frame, u, v, &x, &y);
    return QgsPointXY(x, y);
  };
  double umin = 1e99, umax = -1e99, vmin = 1e99, vmax = -1e99;
  const QgsPointXY corners[4] = {
      QgsPointXY(ext.xMinimum(), ext.yMinimum()), QgsPointXY(ext.xMaximum(), ext.yMinimum()),
      QgsPointXY(ext.xMaximum(), ext.yMaximum()), QgsPointXY(ext.xMinimum(), ext.yMaximum())};
  for (const QgsPointXY& pt : corners) {
    double u = 0.0, v = 0.0;
    CanvasGridMath::toGrid(frame, pt.x(), pt.y(), &u, &v);
    umin = std::min(umin, u);
    umax = std::max(umax, u);
    vmin = std::min(vmin, v);
    vmax = std::max(vmax, v);
  }
  // Too many lines are thinned to every n-th line on the same grid, never cut off at one side.
  const int stride = std::max(CanvasGridMath::lineStride(umax - umin, step, kMaxLines),
                              CanvasGridMath::lineStride(vmax - vmin, step, kMaxLines));
  const double drawStep = step * stride;
  const double u0 = std::floor(umin / drawStep) * drawStep;
  const double v0 = std::floor(vmin / drawStep) * drawStep;
  const int cap = kMaxLines + 2;
  const QPointF origin = pos();
  auto drawMapLine = [&](const QgsPointXY& a, const QgsPointXY& b) {
    p->drawLine(toCanvasCoordinates(a) - origin, toCanvasCoordinates(b) - origin);
  };

  int nx = 0;
  for (double u = u0; u <= umax + drawStep * 0.5 && nx < cap; u += drawStep, ++nx)
    drawMapLine(fromUV(u, vmin), fromUV(u, vmax));
  int ny = 0;
  for (double v = v0; v <= vmax + drawStep * 0.5 && ny < cap; v += drawStep, ++ny)
    drawMapLine(fromUV(umin, v), fromUV(umax, v));

  if (!m_cfg.labels && stride == 1)
    return;
  QFont f(QStringLiteral("Malgun Gothic"), m_cfg.fontPt);
  p->setFont(f);
  // 좌표 글씨도 격자 선 색을 따라간다. 다만 흐린 색이라도 읽히도록 불투명하게.
  QColor labelColor = m_cfg.color;
  labelColor.setAlpha(255);
  p->setPen(QPen(labelColor.darker(115), 0));
  if (stride > 1)
    p->drawText(QRectF(0, 16, mMapCanvas->width(), 16), Qt::AlignHCenter,
                QStringLiteral("격자가 촘촘해 %1 m마다 그립니다")
                    .arg(drawStep, 0, 'f', CanvasGridMath::labelDecimals(drawStep)));
  if (!m_cfg.labels)
    return;
  // Unturned lines are real map coordinates. A turned grid has no single easting per line,
  // so its labels are distances from the grid origin.
  const bool turned = CanvasGridMath::isTurned(frame);
  auto fmt = [&](double gridValue, double originValue) {
    if (turned) {
      if (std::abs(gridValue) < drawStep * 1e-6)
        return QStringLiteral("원점");
      const int decimals = CanvasGridMath::labelDecimals(drawStep);
      return QStringLiteral("%1%2 m").arg(gridValue > 0 ? QStringLiteral("+") : QStringLiteral("−"))
          .arg(std::abs(gridValue), 0, 'f', decimals);
    }
    const double value = gridValue + originValue;
    if (drawStep >= 1000.0 && CanvasGridMath::labelDecimals(originValue) == 0)
      return QStringLiteral("%1 km").arg(value / 1000.0, 0, 'f', CanvasGridMath::labelDecimals(drawStep / 1000.0, originValue / 1000.0));
    return QStringLiteral("%1 m").arg(value, 0, 'f', CanvasGridMath::labelDecimals(drawStep, originValue));
  };
  nx = 0;
  for (double u = u0; u <= umax + drawStep * 0.5 && nx < cap; u += drawStep, ++nx) {
    const QPointF top = toCanvasCoordinates(fromUV(u, vmax)) - origin;
    p->drawText(QPointF(top.x() + 2, 12), fmt(u, m_cfg.originX));
  }
  ny = 0;
  for (double v = v0; v <= vmax + drawStep * 0.5 && ny < cap; v += drawStep, ++ny) {
    const QPointF left = toCanvasCoordinates(fromUV(umin, v)) - origin;
    p->drawText(QPointF(4, left.y() - 2), fmt(v, m_cfg.originY));
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
      KA_LOG_EXCEPT();
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
      KA_LOG_EXCEPT();
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
