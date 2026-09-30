#pragma once

#include "core/SurveyContourBuilder.h"

#include <QDialog>

#include <qgsrectangle.h>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;

class KaSurveyContourDialog : public QDialog {
  Q_OBJECT
public:
  explicit KaSurveyContourDialog(const QString& crsAuthId, QWidget* parent = nullptr);
  // Survey-area bounds in the work CRS; points far from it point to a wrong file CRS.
  void setReferenceExtent(const QgsRectangle& extent);
  // Reads the file once. Axis order and file CRS are then changed in memory only.
  void loadFile(const QString& path);
  SurveyContourJob job() const;

private:
  void browse();
  void reread();
  void rearrange();
  void showPreview();
  QVector<SurveyPoint> usedPoints() const;

  QString m_crsAuthId;
  QString m_path;
  QgsRectangle m_reference;
  SurveyReadReport m_fileOrder;
  SurveyReadReport m_report;
  bool m_otherAxisFits = false;
  bool m_intervalTouched = false;
  QDoubleSpinBox* m_interval = nullptr;
  QCheckBox* m_swap = nullptr;
  QCheckBox* m_excludeSuspicious = nullptr;
  QComboBox* m_fileCrs = nullptr;
  QDoubleSpinBox* m_maxEdge = nullptr;
  QCheckBox* m_breaklines = nullptr;
  QDoubleSpinBox* m_cell = nullptr;
  QLabel* m_file = nullptr;
  QLabel* m_preview = nullptr;
};
