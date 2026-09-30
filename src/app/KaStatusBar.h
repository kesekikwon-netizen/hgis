#pragma once
#include <QStatusBar>

#include "core/EditBufferSummary.h"

class KaChip;
class QComboBox;
class QLabel;
class QLineEdit;
class QToolButton;

// Instrument panel along the bottom of the main window: live cursor position,
// map scale entry, work/upload CRS and the render switch. Subclasses QStatusBar
// so existing showMessage() call sites keep working unchanged.
class KaStatusBar : public QStatusBar {
  Q_OBJECT
public:
  explicit KaStatusBar(QWidget* parent = nullptr);

  QLineEdit* scaleEdit() const { return m_scaleEdit; }
  QComboBox* scaleCombo() const { return m_scaleCombo; }

  void setCoordinate(double x, double y);
  void clearCoordinate();
  void setWorkCrs(const QString& authId);
  void setUploadCrs(const QString& authId);
  void setRenderingEnabled(bool on);
  bool isRenderingEnabled() const;
  // Cursor position, scale and the render switch only mean something on the map
  // tab; the home page and the drawing tabs hide them.
  void setMapInstrumentsVisible(bool visible);
  // The drawing tab shares the scale field with the map (it edits the paper
  // scale there) but has no cursor position or render switch.
  void setInstrumentsVisible(bool mapReadout, bool scale);
  // Work/upload CRS belong to an open survey, not to the home page.
  void setCrsChipsVisible(bool visible);

  // Strata chrome: two display-only chips left of the readout. The snap chip follows the
  // map readout (map tab only) and stays hidden until the window reports a state; the
  // unsaved chip follows the CRS chips (open survey) and shows only while something is
  // unsaved. Neither chip changes anything when clicked.
  void setSnapState(bool on);
  bool snapState() const { return m_snapOn; }
  // features: unique edited feature ids over the edit buffers; projectDirty: the
  // project flag with no feature behind it. 0/false hides the chip.
  void setUnsavedCount(int features, bool projectDirty);
  void setUnsavedCount(const EditBufferSummary::Summary& summary) {
    setUnsavedCount(summary.features, summary.projectDirty);
  }
  KaChip* snapChip() const { return m_snapChip; }
  KaChip* unsavedChip() const { return m_unsavedChip; }

signals:
  void crsClicked();
  void renderingToggled(bool on);

private:
  void refreshCrsText();
  void syncChipVisibility();

  KaChip* m_snapChip = nullptr;
  KaChip* m_unsavedChip = nullptr;
  bool m_snapOn = false;
  bool m_snapKnown = false;
  bool m_unsavedWanted = false;
  bool m_mapReadoutVisible = true;
  bool m_surveyChipsVisible = true;
  QLabel* m_xy = nullptr;
  QLineEdit* m_scaleEdit = nullptr;
  QComboBox* m_scaleCombo = nullptr;
  QToolButton* m_crsButton = nullptr;
  QLabel* m_uploadChip = nullptr;
  QToolButton* m_renderButton = nullptr;
  QList<QWidget*> m_mapOnly;
  QList<QWidget*> m_scaleOnly;
  QList<QWidget*> m_surveyOnly;
  QString m_workCrs;
  QString m_uploadCrs;
};
