#include "KaCrashGuard.h"
#include "KaCoordPointMapTool.h"

#include <qgsmapcanvas.h>
#include <qgsmapmouseevent.h>
#include <qgsrubberband.h>
#include <qgsvertexmarker.h>
#include <qgsmapcanvasitem.h>
#include <qgssnappingutils.h>
#include <qgspointlocator.h>
#include <qgsvectorlayer.h>
#include <qgsproject.h>
#include <qgssnappingconfig.h>
#include <qgscoordinatetransform.h>
#include <qgsfeaturerequest.h>
#include <qgsfeature.h>
#include <qgswkbtypes.h>

#include <QKeyEvent>
#include <QTimer>
#include <QPainter>
#include <cmath>

namespace {

class KaCoordCalloutItem : public QgsMapCanvasItem {
public:
  explicit KaCoordCalloutItem(QgsMapCanvas* canvas) : QgsMapCanvasItem(canvas) {}

  void setCallouts(const QVector<QgsPointXY>& pts, const QVector<QString>& letters,
                   const QVector<QString>& texts) {
    m_pts = pts;
    m_letters = letters;
    m_texts = texts;
    updatePosition();
    update();
  }

  void updatePosition() override {
    if (!mMapCanvas || m_pts.isEmpty()) return;
    QgsRectangle r(m_pts.first().x(), m_pts.first().y(), m_pts.first().x(), m_pts.first().y());
    for (const QgsPointXY& p : m_pts)
      r.combineExtentWith(p.x(), p.y());
    const double pad = std::max(mMapCanvas->mapUnitsPerPixel() * 90.0, 2.0);
    r.grow(pad);
    setRect(r);
  }

protected:
  void paint(QPainter* painter) override {
    if (!painter || !mMapCanvas) return;
    painter->setRenderHint(QPainter::Antialiasing, true);
    QVector<QRectF> used;
    for (int i = 0; i < m_pts.size(); ++i) {
      const QPointF tip = toCanvasCoordinates(m_pts.at(i)) - pos();
      const QString letter = i < m_letters.size() ? m_letters.at(i) : QString();
      const QString text = i < m_texts.size() ? m_texts.at(i) : QString();
      QFont boxFont(QStringLiteral("Malgun Gothic"), 8);
      painter->setFont(boxFont);
      const QFontMetrics fm(boxFont);
      const QRect tr = fm.tightBoundingRect(text);
      const qreal bw = tr.width() + 6.0;
      const qreal bh = tr.height() + 4.0;
      const qreal gap = 14.0;
      const QRectF cands[] = {
          QRectF(tip.x() + gap, tip.y() - bh - 6.0, bw, bh),
          QRectF(tip.x() + gap, tip.y() + 6.0, bw, bh),
          QRectF(tip.x() - gap - bw, tip.y() - bh - 6.0, bw, bh),
          QRectF(tip.x() - gap - bw, tip.y() + 6.0, bw, bh),
          QRectF(tip.x() - bw * 0.5, tip.y() - bh - gap, bw, bh),
          QRectF(tip.x() - bw * 0.5, tip.y() + gap, bw, bh),
      };
      QRectF box = cands[0];
      for (const QRectF& c : cands) {
        bool hit = false;
        for (const QRectF& u : used) {
          if (c.adjusted(-4, -4, 4, 4).intersects(u)) {
            hit = true;
            break;
          }
        }
        if (!hit) {
          box = c;
          break;
        }
      }
      used.append(box);
      const QPointF attach = (box.center().x() >= tip.x()) ? QPointF(box.left(), box.center().y())
                                                           : QPointF(box.right(), box.center().y());
      painter->setPen(QPen(QColor(28, 25, 23), 1.4));
      painter->drawLine(attach, tip);
      QPolygonF head;
      QPointF dir = tip - attach;
      const double len = std::hypot(dir.x(), dir.y());
      if (len > 1.0) {
        dir /= len;
        const QPointF n(-dir.y(), dir.x());
        const QPointF neck = tip - dir * 8.0;
        head << tip << (neck + n * 4.0) << (neck - n * 4.0);
        painter->setBrush(QColor(28, 25, 23));
        painter->drawPolygon(head);
      }
      painter->setBrush(QColor(255, 255, 255, 235));
      painter->setPen(QPen(QColor(28, 25, 23), 1.0));
      painter->drawRect(box);
      painter->setPen(QColor(28, 25, 23));
      painter->drawText(box.adjusted(3, 1, -2, -1), Qt::AlignLeft | Qt::AlignVCenter, text);
      painter->setFont(QFont(QStringLiteral("Malgun Gothic"), 9, QFont::Bold));
      painter->drawText(QRectF(tip.x() - 8, tip.y() - 18, 16, 14), Qt::AlignCenter, letter);
    }
  }

private:
  QVector<QgsPointXY> m_pts;
  QVector<QString> m_letters;
  QVector<QString> m_texts;
};

QVector<QgsPointXY> ringFromGeom(const QgsGeometry& geom, const QgsPointXY& near) {
  QVector<QgsPointXY> ring;
  if (geom.isEmpty()) return ring;
  const Qgis::WkbType wt = QgsWkbTypes::flatType(geom.wkbType());
  if (wt == Qgis::WkbType::Polygon || wt == Qgis::WkbType::Triangle) {
    const QgsPolygonXY poly = geom.asPolygon();
    if (!poly.isEmpty())
      ring = QVector<QgsPointXY>(poly.at(0).begin(), poly.at(0).end());
  } else if (wt == Qgis::WkbType::MultiPolygon) {
    const QgsMultiPolygonXY mp = geom.asMultiPolygon();
    int best = 0;
    double bestA = -1.0;
    for (int i = 0; i < mp.size(); ++i) {
      if (mp.at(i).isEmpty()) continue;
      const QgsGeometry part = QgsGeometry::fromPolygonXY(mp.at(i));
      const bool inside = part.contains(QgsGeometry::fromPointXY(near));
      const double a = part.area();
      if (inside || a > bestA) {
        bestA = a;
        best = i;
        if (inside) break;
      }
    }
    if (best >= 0 && best < mp.size() && !mp.at(best).isEmpty())
      ring = QVector<QgsPointXY>(mp.at(best).at(0).begin(), mp.at(best).at(0).end());
  } else if (wt == Qgis::WkbType::LineString) {
    const QgsPolylineXY ln = geom.asPolyline();
    ring = QVector<QgsPointXY>(ln.begin(), ln.end());
  }
  return ring;
}

}  // namespace

KaCoordPointMapTool::KaCoordPointMapTool(QgsMapCanvas* canvas)
    : QgsMapTool(canvas) {
  setCursor(Qt::CrossCursor);
}

QgsMapTool::Flags KaCoordPointMapTool::flags() const {
  return QgsMapTool::ShowContextMenu;
}

void KaCoordPointMapTool::canvasReleaseEvent(QgsMapMouseEvent* e) {
  if (e) e->accept();
}

void KaCoordPointMapTool::clearVectorSelections() {
  if (!canvas()) return;
  for (QgsMapLayer* raw : canvas()->layers()) {
    if (auto* vl = qobject_cast<QgsVectorLayer*>(raw))
      vl->removeSelection();
  }
}

KaCoordPointMapTool::~KaCoordPointMapTool() {
  clearAll();
}

void KaCoordPointMapTool::activate() {
  QgsMapTool::activate();
  if (canvas())
    canvas()->setCurrentLayer(nullptr);
  clearVectorSelections();
  if (QgsProject* proj = QgsProject::instance()) {
    QgsSnappingConfig cfg = proj->snappingConfig();
    cfg.setEnabled(true);
    cfg.setMode(Qgis::SnappingMode::AllLayers);
    cfg.setTypeFlag(Qgis::SnappingType::Vertex | Qgis::SnappingType::Segment);
    cfg.setIntersectionSnapping(true);
    cfg.setSelfSnapping(true);
    cfg.setTolerance(16.0);
    cfg.setUnits(Qgis::MapToolUnit::Pixels);
    proj->setSnappingConfig(cfg);
    if (canvas() && canvas()->snappingUtils())
      canvas()->snappingUtils()->setConfig(cfg);
  }
  emit statusMessage(QStringLiteral("맵에서 꼭짓점을 찍으세요. 자석이 붙고 좌표와 테두리가 생깁니다."));
}

void KaCoordPointMapTool::deactivate() {
  if (m_snapMark) {
    delete m_snapMark;
    m_snapMark = nullptr;
  }
  QgsMapTool::deactivate();
}

void KaCoordPointMapTool::clearAll() {
  destroyFrame();
  if (m_snapMark) {
    delete m_snapMark;
    m_snapMark = nullptr;
  }
  delete static_cast<KaCoordCalloutItem*>(m_labels);
  m_labels = nullptr;
  m_points.clear();
  m_letters.clear();
  m_texts.clear();
  m_frameGeom = QgsGeometry();
}

void KaCoordPointMapTool::destroyFrame() {
  delete m_frame;
  m_frame = nullptr;
}

bool KaCoordPointMapTool::mapPointFromEvent(QgsMapMouseEvent* e, QgsPointXY* out, QgsGeometry* hitGeom) {
  if (!e || !out || !canvas()) return false;
  if (hitGeom) *hitGeom = QgsGeometry();
  try {
    *out = e->mapPoint();
  } catch (...) {
    KaCrashGuard::logLine(QStringLiteral("[except] app/KaCoordPointMapTool.cpp:230"));
    *out = toMapCoordinates(e->pos());
  }
  bool snapped = false;
  if (canvas()->snappingUtils()) {
    const QgsPointLocator::Match hit = canvas()->snappingUtils()->snapToMap(e->pos());
    if (hit.isValid()) {
      *out = hit.point();
      snapped = true;
    }
  }
  if (!m_snapMark && canvas()) {
    m_snapMark = new QgsVertexMarker(canvas());
    m_snapMark->setIconType(QgsVertexMarker::ICON_CROSS);
    m_snapMark->setIconSize(22);
    m_snapMark->setPenWidth(3);
    m_snapMark->setColor(QColor(250, 204, 21));
    m_snapMark->setFillColor(QColor(250, 204, 21));
  }
  if (m_snapMark) {
    m_snapMark->setCenter(*out);
    m_snapMark->setVisible(snapped);
  }
  return !(std::isnan(out->x()) || std::isnan(out->y()));
}

void KaCoordPointMapTool::canvasMoveEvent(QgsMapMouseEvent* e) {
  QgsPointXY pt;
  QgsGeometry dummy;
  mapPointFromEvent(e, &pt, &dummy);
}

void KaCoordPointMapTool::canvasPressEvent(QgsMapMouseEvent* e) {
  if (!e) return;
  e->accept();
  clearVectorSelections();
  if (e->button() == Qt::RightButton) {
    emit statusMessage(QStringLiteral("좌표점 찍기를 끝내려면 다른 도구를 누르세요."));
    return;
  }
  if (e->button() != Qt::LeftButton) return;
  QgsPointXY pt;
  QgsGeometry geom;
  if (!mapPointFromEvent(e, &pt, &geom)) return;
  clearVectorSelections();
  addCallout(pt, geom);
  QTimer::singleShot(0, this, [this]() { clearVectorSelections(); });
}

void KaCoordPointMapTool::keyPressEvent(QKeyEvent* e) {
  if (e && (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Delete)) {
    clearAll();
    emit statusMessage(QStringLiteral("좌표점을 지웠습니다."));
    e->accept();
    return;
  }
  QgsMapTool::keyPressEvent(e);
}

void KaCoordPointMapTool::addCallout(const QgsPointXY& mapPt, const QgsGeometry& hitGeom) {
  const QString letter = QString(QChar(static_cast<char>('A' + (m_points.size() % 26))));
  const QString text = QStringLiteral("X=%1\nY=%2").arg(mapPt.y(), 0, 'f', 3).arg(mapPt.x(), 0, 'f', 3);
  m_points.append(mapPt);
  m_letters.append(letter);
  m_texts.append(text);
  if (!m_labels && canvas())
    m_labels = new KaCoordCalloutItem(canvas());
  if (auto* labels = dynamic_cast<KaCoordCalloutItem*>(m_labels))
    labels->setCallouts(m_points, m_letters, m_texts);
  Q_UNUSED(hitGeom);
  emit statusMessage(QStringLiteral("%1점  X=%2  Y=%3")
                         .arg(letter)
                         .arg(mapPt.y(), 0, 'f', 3)
                         .arg(mapPt.x(), 0, 'f', 3));
}
