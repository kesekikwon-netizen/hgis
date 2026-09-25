#pragma once

#include <QColor>
#include <QDialog>
#include <QString>
#include <QList>

class QComboBox;
class QDoubleSpinBox;
class QFrame;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QgsProject;
class QgsVectorLayer;

class KaSurveyAreaDialog : public QDialog {
  Q_OBJECT

public:
  explicit KaSurveyAreaDialog(QWidget* parent = nullptr, QgsProject* project = nullptr,
                              const QString& gpkgPath = QString());
  ~KaSurveyAreaDialog() override = default;

  bool isNewLayer() const;
  QString layerName() const;
  QgsVectorLayer* selectedExistingLayer() const;
  QColor strokeColor() const { return m_strokeColor; }
  QColor fillColor() const { return m_fillColor; }
  double strokeWidthMm() const;

private slots:
  void onModeChanged();
  void onColorButtonClicked(int index);
  void onPresetWidthClicked(double width);
  void updatePreview();

private:
  void setupUi();
  void selectColorIndex(int index);

  QgsProject* m_project{nullptr};
  QString m_gpkgPath;
  QList<QgsVectorLayer*> m_existingLayers;

  QRadioButton* m_radioNew{nullptr};
  QRadioButton* m_radioExisting{nullptr};
  QLineEdit* m_nameEdit{nullptr};
  QComboBox* m_existingCombo{nullptr};

  // Colour and width style a new layer only; an existing area keeps its own style.
  QGroupBox* m_colorGroup{nullptr};
  QGroupBox* m_widthGroup{nullptr};
  QList<QPushButton*> m_colorButtons;
  QList<QColor> m_paletteColors;
  int m_selectedColorIndex{0};
  QColor m_strokeColor;
  QColor m_fillColor;

  QDoubleSpinBox* m_widthSpin{nullptr};
  QList<QPushButton*> m_widthPresetButtons;

  QFrame* m_previewBox{nullptr};
  QLabel* m_previewLabel{nullptr};
};
