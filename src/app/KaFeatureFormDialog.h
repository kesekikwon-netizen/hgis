#pragma once

#include <QDialog>
#include <QHash>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <qgsfeatureid.h>

class QComboBox;
class QLabel;
class QLineEdit;
class QWidget;
class QgsVectorLayer;

// 그리기 직후 이름·번호. 취소해도 도형은 그대로 둔다. 그리기 중에는 띄우지 않는다.
// Kind and period are combos of the presets plus the survey's own values (free text
// allowed); the number starts at the next number of that kind and a repeated number
// is only pointed out, never refused.
class KaFeatureFormDialog : public QDialog {
  Q_OBJECT
public:
  // featureId: the feature just drawn. With it the form also offers 시대 and applyTo()
  // writes every value; without it only name/number are read by the caller.
  explicit KaFeatureFormDialog(QgsVectorLayer* layer, QWidget* parent = nullptr,
                               QgsFeatureId featureId = FID_NULL);

  static bool canOffer(const QgsVectorLayer* layer);
  QString nameText() const;
  QString numberText() const;
  QString periodText() const;
  // Writes name, number and period (when offered) into the edit buffer and stamps
  // updated_at. Never commits. Call inside LayerOps::runEditCommand.
  bool applyTo(QgsVectorLayer* layer, QString* errorOut = nullptr) const;

  // Editors for a record field, shared with the attribute form and the feature card:
  // kind/period -> editable combo (presets + the survey's values), others -> line edit.
  static QWidget* createValueEditor(QWidget* parent, const QgsVectorLayer* layer, const QString& field,
                                    const QString& current);
  // Trimmed text of such an editor; kind/period come back in the survey's spelling.
  static QString valueEditorText(const QWidget* editor);

private:
  void updateNumberHint();

  QPointer<QgsVectorLayer> m_layer;
  QgsFeatureId m_featureId = FID_NULL;
  QString m_nameField;
  QLineEdit* m_name = nullptr;
  QComboBox* m_kind = nullptr;
  QComboBox* m_period = nullptr;
  QLineEdit* m_number = nullptr;
  QLabel* m_numberNote = nullptr;
  bool m_numberTyped = false;
  bool m_groupByKind = false;  // feature_no: numbers run per kind
  // kind key -> numbers already used for that kind (read once when the form opens)
  QHash<QString, QStringList> m_numbersByKind;
  QString m_numberTemplate;
};
