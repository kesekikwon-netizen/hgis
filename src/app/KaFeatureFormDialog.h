#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;
class QgsVectorLayer;

// 그리기 직후 이름·번호. 취소해도 도형은 그대로 둔다. 그리기 중에는 띄우지 않는다.
class KaFeatureFormDialog : public QDialog {
  Q_OBJECT
public:
  explicit KaFeatureFormDialog(QgsVectorLayer* layer, QWidget* parent = nullptr);

  static bool canOffer(const QgsVectorLayer* layer);
  QString nameText() const;
  QString numberText() const;

private:
  QLineEdit* m_name = nullptr;
  QLineEdit* m_number = nullptr;
};
