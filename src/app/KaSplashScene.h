#pragma once

#include <QImage>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

class QPainter;
class QPixmap;

// The field-to-drawing scene of the startup splash: a surveyed plan covered with
// soil. Scraping the soil, automatically over time or with the mouse as a trowel,
// reveals the measured drawing underneath: "현장을 도면으로".
class KaSplashScene {
public:
  // The automatic trowel pass starts once the credits have been on screen for a
  // while and finishes before the end, so the drawing is complete without the mouse.
  static constexpr double kScrapeStart = 0.16;
  static constexpr double kScrapeLength = 0.52;
  static constexpr double kSlope = 0.6;  // the scrape line leans like a trowel stroke

  void setRect(const QRectF& plan, qreal devicePixelRatio);
  QRectF rect() const { return m_rect; }
  // phase is the 0..1 animation position; dt is the frame time in seconds.
  void advance(double phase, double dt);
  // The mouse works as a trowel. Returns true while the pointer is over the plan.
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
