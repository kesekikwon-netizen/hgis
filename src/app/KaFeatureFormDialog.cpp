#include "KaFeatureFormDialog.h"
#include "core/LayerOps.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <qgsvectorlayer.h>

KaFeatureFormDialog::KaFeatureFormDialog(QgsVectorLayer* layer, QWidget* parent)
    : QDialog(parent) {
  setObjectName(QStringLiteral("kaFeatureForm"));
  setWindowTitle(QStringLiteral("이름·번호"));
  setMinimumWidth(360);
  auto* form = new QFormLayout(this);
  auto* tip = new QLabel(
      QStringLiteral("선택입니다. 취소해도 그린 도형은 그대로 남습니다."), this);
  tip->setWordWrap(true);
  form->addRow(tip);
  const auto fields = LayerOps::featureFormFields(layer);
  if (fields.nameIndex >= 0) {
    m_name = new QLineEdit(this);
    m_name->setPlaceholderText(QStringLiteral("예: 주거지, 조사명"));
    form->addRow(QStringLiteral("이름"), m_name);
  }
  if (fields.numberIndex >= 0) {
    m_number = new QLineEdit(this);
    m_number->setPlaceholderText(QStringLiteral("예: 1호"));
    form->addRow(QStringLiteral("번호"), m_number);
  }
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
  buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("저장"));
  buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("건너뛰기"));
  form->addRow(buttons);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

bool KaFeatureFormDialog::canOffer(const QgsVectorLayer* layer) {
  const auto fields = LayerOps::featureFormFields(layer);
  return fields.nameIndex >= 0 || fields.numberIndex >= 0;
}

QString KaFeatureFormDialog::nameText() const {
  return m_name ? m_name->text().trimmed() : QString();
}

QString KaFeatureFormDialog::numberText() const {
  return m_number ? m_number->text().trimmed() : QString();
}
