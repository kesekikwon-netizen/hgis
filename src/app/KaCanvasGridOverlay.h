#pragma once

#include <qgsmapcanvasitem.h>
#include <qgspointxy.h>
#include <QColor>
#include <QFont>

class QgsMapCanvas;

// Viewport coordinate grid (QGIS decoration / CAD-style). Not a layer.
class KaCanvasGridOverlay : public QgsMapCanvasItem {
public:
  enum class Type { ProjectedMeters, GeographicDms };

  struct Config {
    bool enabled = false;
    Type type = Type::ProjectedMeters;
    bool labels = true;
    QColor color = QColor(51, 65, 85, 210);
    bool dashed = true;
    Qt::PenStyle penStyle = Qt::DashLine;
    double lineWidth = 1.2;
    int fontPt = 8;
    // 0 = auto from scale; otherwise metres (0.1 m and up for excavation grids).
    double stepMeters = 20.0;
    // Clockwise from CRS +X (east), degrees, about the grid origin. Same sense as trench azimuths.
    double rotationDeg = 0.0;
    // A grid line passes through this map point; (0, 0) is the CRS origin.
    double originX = 0.0;
    double originY = 0.0;
  };

  // More lines than this per direction are thinned to every n-th line, with a note on the map.
  static constexpr int kMaxLines = 80;

  explicit KaCanvasGridOverlay(QgsMapCanvas* canvas);
  void setConfig(const Config& c);
  Config config() const { return m_cfg; }
  void setEnabled(bool on);
  bool isEnabled() const { return m_cfg.enabled; }
  double stepMeters() const;
  QgsPointXY snapToGrid(const QgsPointXY& p) const;

  void updatePosition() override;

protected:
  void paint(QPainter* painter) override;

private:
  void paintProjected(QPainter* p);
  void paintGeographic(QPainter* p);
  Config m_cfg;
};
