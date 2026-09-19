#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;

// 그리기 막대의 자석 한 곳. 켬/끔·픽셀 허용치·현재 레이어/모든 조사 레이어.
class KaSnapSettingsWidget : public QWidget {
  Q_OBJECT
public:
  explicit KaSnapSettingsWidget(QWidget* parent = nullptr);
  void syncFromProject();

signals:
  void settingsChanged();

private:
  void writeToProject();

  QCheckBox* m_on = nullptr;
  QCheckBox* m_topo = nullptr;
  QDoubleSpinBox* m_tolerance = nullptr;
  QComboBox* m_target = nullptr;
  bool m_syncing = false;
};
