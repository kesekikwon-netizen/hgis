#pragma once

#include <QColor>
#include <QDialog>
#include <QString>
#include <QList>

class QDoubleSpinBox;
class QLineEdit;
class QPushButton;
class QgsMapLayer;
class QgsProject;
class QgsVectorLayer;

// Asked only before the first survey area: its name, outline colour and width. An area that is
// already there is continued without a window (user 2026-10-03, docs/intent/2026-10-03-draw-dialog-and-inspector-tabs.md).
class KaSurveyAreaDialog : public QDialog {
  Q_OBJECT

public:
  explicit KaSurveyAreaDialog(QWidget* parent = nullptr);
  ~KaSurveyAreaDialog() override = default;

  QString layerName() const;
  QColor strokeColor() const { return m_strokeColor; }
  QColor fillColor() const { return m_fillColor; }
  double strokeWidthMm() const;
  // Text colour with at least 4.5:1 contrast on a swatch (white, theme ink, then black).
  static QColor readableTextOn(const QColor& fill);
  static QgsVectorLayer* layerToContinue(QgsProject* project, const QString& surveyPath, QgsMapLayer* current);

private:
  void setupUi();
  void selectColorIndex(int index);
  void selectPresetWidth(double width);

  QLineEdit* m_nameEdit{nullptr};
  QList<QPushButton*> m_colorButtons;
  QList<QColor> m_paletteColors;
  QStringList m_colorNames;
  QColor m_strokeColor;
  QColor m_fillColor;

  QDoubleSpinBox* m_widthSpin{nullptr};
  QList<QPushButton*> m_widthPresetButtons;
};
