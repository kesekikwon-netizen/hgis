#pragma once

#include "core/SurveyContourBuilder.h"

#include <QDialog>

class QDoubleSpinBox;
class QLabel;

class KaSurveyContourDialog : public QDialog {
  Q_OBJECT
public:
  explicit KaSurveyContourDialog(const QString& crsAuthId, QWidget* parent = nullptr);
  SurveyContourJob job() const;

private:
  void browse();
  void loadFile(const QString& path);
  void reread();
  void showPreview();

  QString m_crsAuthId;
  QString m_path;
  SurveyReadReport m_report;
  bool m_intervalTouched = false;
  QDoubleSpinBox* m_interval = nullptr;
  QLabel* m_file = nullptr;
  QLabel* m_preview = nullptr;
};
