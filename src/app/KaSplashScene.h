#pragma once

#include <QImage>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

class QPainter;
class QPixmap;

// Finished survey plan on the startup splash. The plate is already uncovered;
// motion is a zoom on the splash, not a trowel scrape.
class KaSplashScene {
public:

  void setRect(const QRectF& plan, qreal devicePixelRatio);
  QRectF rect() const { return m_rect; }
  void advance(double phase, double dt);
  bool pointerMoved(const QPointF& pos, bool pressed);
  void pointerLeft();
  void paint(QPainter& painter, double phase, const QPixmap& trowel) const;
  double revealedFraction() const;
  bool interacted() const { return m_interacted; }

private:
  enum class Style { Outline, Dashed, Filled };
  struct Feature {
    QPainterPath shape;
    QPainterPath hit;
    QString name;
    QString detail;
    QPointF label;
    Style style = Style::Outline;
  };
  struct Grain {
    QPointF pos;
    QPointF velocity;
    double life = 1.0;
    int shade = 0;
  };

  void rebuild();
  void addFeature(const QPainterPath& shape, const QString& name, const QString& detail,
                  const QPointF& label, Style style);
  void erase(const QPainterPath& localPath);
  void spray(const QPointF& localPos, int count, double speed);
  bool uncovered(const QPointF& localPos) const;
  int hoveredFeature() const;
  void paintMarkers(QPainter& painter, double phase) const;
  void paintPointer(QPainter& painter, int hover, const QPixmap& trowel) const;
  double unitRandom();

  QRectF m_rect;
  qreal m_dpr = 1.0;
  QImage m_soil;
  QVector<Feature> m_features;
  QVector<Grain> m_grains;
  QPointF m_pointer;
  QPointF m_lastScrape;
  bool m_pointerInside = false;
  bool m_hasLastScrape = false;
  bool m_interacted = false;
  double m_autoScrape = 0.0;
  quint32 m_seed = 0x2545F491u;
};
