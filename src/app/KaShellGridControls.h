#pragma once

#include <QColor>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QTimer;
class QToolButton;

// Coordinate-grid values as the corner bar shows them. Plain values keep the widget free of
// QGIS; MainWindow copies them into KaCanvasGridOverlay::Config.
struct KaShellGridSettings {
  bool enabled = false;
  bool geographic = false;  // false: metre grid in the work CRS; true: 경위도 (DMS) grid
  double stepMeters = 20.0;
  double rotationDeg = 0.0;
  double lineWidth = 1.2;
  Qt::PenStyle penStyle = Qt::DashLine;
  QColor color = QColor(51, 65, 85, 210);
};

// Coordinate-grid controls for the map's bottom-right corner bar.
// The grid kind is an explicit choice (미터 / 경위도), never the Shift key, so changing the
// spacing, colour or line cannot switch the kind. Value edits are debounced into one update.
// When the grid is on, the details wrap into two short rows so the bar stays narrow and does
// not run into the scale bar on the left of a small laptop map.
class KaShellGridControls final : public QWidget {
  Q_OBJECT
public:
  static constexpr int kApplyDelayMs = 250;

  explicit KaShellGridControls(QWidget* parent = nullptr);

  KaShellGridSettings settings() const;
  // Spacing used for trench snapping even while the grid is hidden.
  double stepMeters() const;
  void setColor(const QColor& color);
  bool applyPending() const;

  QCheckBox* enabledCheck() const { return m_check; }
  QComboBox* kindCombo() const { return m_kind; }
  QDoubleSpinBox* stepSpin() const { return m_step; }
  QDoubleSpinBox* rotationSpin() const { return m_rotation; }
  QDoubleSpinBox* widthSpin() const { return m_width; }

signals:
  // announce is true when the grid was turned on/off or its kind changed (worth a status
  // line); false for debounced value edits, which should update quietly.
  void settingsChanged(bool announce);

private:
  void scheduleQuietApply();
  void emitNow(bool announce);
  void syncKindState();
  void syncColorButtons();

  QCheckBox* m_check = nullptr;
  QWidget* m_detail = nullptr;
  QComboBox* m_kind = nullptr;
  QDoubleSpinBox* m_step = nullptr;
  QDoubleSpinBox* m_rotation = nullptr;
  QDoubleSpinBox* m_width = nullptr;
  QComboBox* m_dash = nullptr;
  QVector<QPair<QToolButton*, QColor>> m_swatches;
  QTimer* m_apply = nullptr;
  QColor m_color = QColor(51, 65, 85, 210);
};
