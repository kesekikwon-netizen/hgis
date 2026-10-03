#pragma once

#include <QColor>
#include <QList>
#include <QPointer>
#include <QWidget>

class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QgsVectorLayer;

// The inspector's 스타일 tab: the chosen layer's colour (eight swatches) and line width (or point
// size) changed right in the tab and shown on the map at once, no extra window (user 2026-10-03
// 「바로 수정 편집할수있는거도 아니고」, docs/intent/2026-10-03-draw-dialog-and-inspector-tabs.md).
// The choice is remembered on the layer (LayerOps::applySimpleVectorStyle), so later restyles keep
// it. Without an editable layer it says how to pick one; a line map (reference/cadastral) points to
// the right-click 「면·외곽선 색」, which also keeps the full 「도형 색」 window (dashes, period colours).
class KaInspectorStyle : public QWidget {
  Q_OBJECT
 public:
  explicit KaInspectorStyle(QWidget* parent = nullptr);
  void setLayer(QgsVectorLayer* layer);

 signals:
  // After a colour or width went onto the layer: the window marks the survey unsaved.
  void styleApplied(QgsVectorLayer* layer);

 private:
  void readLayerStyle();  // the layer's look right now: another window may have changed it
  void pickColor(const QColor& color);
  void setWidth(double value);
  void apply();
  void markSwatches();
  bool isPoint() const;

  QPointer<QgsVectorLayer> m_layer;
  QLabel* m_sentence = nullptr;
  QWidget* m_controls = nullptr;
  QLabel* m_layerName = nullptr;
  QList<QPushButton*> m_swatches;
  QLabel* m_widthLabel = nullptr;
  QDoubleSpinBox* m_width = nullptr;
  QLabel* m_note = nullptr;
  QColor m_fill;
  QColor m_stroke;
  double m_widthMm = 1.2;
  double m_markerMm = 3.5;
  bool m_noFill = false;
  bool m_noStroke = false;
  bool m_dashed = false;
};
