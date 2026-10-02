#pragma once

#include <QDialog>
#include <QString>

class QLabel;
class QLineEdit;
class QPushButton;

// 「새 조사」 dialog: survey name and work origin (5186 중부 / 5187 동부).
//
// The pre-selected origin comes from the caller (the default 5187 stays); the dialog only
// explains which regions use which origin and, when the map already shows a place, adds a
// one-line recommendation. It never switches the origin on its own. The name is checked
// while typing, so an unusable file name is explained before the folder picker opens.
class KaNewSurveyDialog final : public QDialog {
public:
  // mapLongitude is the WGS84 longitude of what the map shows, or NaN when nothing
  // meaningful is known (home screen, whole-country view).
  KaNewSurveyDialog(const QString& currentWorkCrs, double mapLongitude, QWidget* parent = nullptr);

  QString surveyName() const;  // trimmed
  QString workCrs() const;     // "EPSG:5186" or "EPSG:5187"

  // Empty when the name can become a Windows file name as-is, otherwise the reason (Korean).
  static QString nameProblem(const QString& name);
  // Origin that fits a WGS84 longitude inside Korea, or empty when unknown / out of range.
  static QString suggestedWorkCrs(double longitude);
  // One line on which regions use 중부원점 5186 and 동부원점 5187.
  static QString regionHint();

private:
  void updateState();

  QLineEdit* m_name = nullptr;
  QLabel* m_nameError = nullptr;
  QPushButton* m_btn5186 = nullptr;
  QPushButton* m_btn5187 = nullptr;
  QPushButton* m_ok = nullptr;
  QLabel* m_suggestion = nullptr;
  QString m_suggested;
  double m_longitude = 0.0;
};
