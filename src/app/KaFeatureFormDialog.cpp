#include "KaFeatureFormDialog.h"
#include "core/FeatureNumbering.h"
#include "core/FeaturePresets.h"
#include "core/FeatureRecord.h"
#include "core/LayerOps.h"

#include <QComboBox>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <algorithm>
#include <qgis.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgsfields.h>
#include <qgsvectorlayer.h>

namespace {
constexpr const char* kFieldProperty = "kaRecordField";
constexpr const char* kLayerProperty = "kaRecordLayer";

bool isChoiceField(const QString& field) {
  return field == QLatin1String("kind") || field == QLatin1String("period");
}

QString placeholderFor(const QString& field) {
  if (field == QLatin1String("kind")) return QStringLiteral("예: 주거지, 수혈 — 목록에 없으면 직접 적으세요");
  if (field == QLatin1String("period")) return QStringLiteral("예: 청동기, 원삼국");
  if (field == QLatin1String("feature_no")) return QStringLiteral("예: 1호");
  return {};
}

QComboBox* makeChoiceCombo(QWidget* parent, const QgsVectorLayer* layer, const QString& field,
                           const QString& current) {
  auto* combo = new QComboBox(parent);
  combo->setEditable(true);
  combo->setInsertPolicy(QComboBox::NoInsert);
  combo->setMaxVisibleItems(14);
  combo->addItems(FeatureRecord::choices(layer, field));
  combo->setCurrentIndex(combo->findText(current));
  combo->setEditText(current);
  combo->lineEdit()->setPlaceholderText(placeholderFor(field));
  // A popup list instead of inline completion: typed text is never extended silently.
  if (QCompleter* completer = combo->completer()) {
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setFilterMode(Qt::MatchContains);
  }
  combo->setProperty(kFieldProperty, field);
  combo->setProperty(kLayerProperty,
                     QVariant::fromValue(QPointer<QObject>(const_cast<QgsVectorLayer*>(layer))));
  return combo;
}
}  // namespace

KaFeatureFormDialog::KaFeatureFormDialog(QgsVectorLayer* layer, QWidget* parent, QgsFeatureId featureId)
    : QDialog(parent), m_layer(layer), m_featureId(featureId) {
  setObjectName(QStringLiteral("kaFeatureForm"));
  setWindowTitle(QStringLiteral("이름·번호"));
  setMinimumWidth(380);
  auto* form = new QFormLayout(this);
  auto* tip = new QLabel(
      QStringLiteral("선택입니다. 취소해도 그린 도형은 그대로 남습니다."), this);
  tip->setWordWrap(true);
  form->addRow(tip);
  const auto fields = LayerOps::featureFormFields(layer);
  m_nameField = fields.nameField;
  if (fields.nameIndex >= 0 && m_nameField == QLatin1String("kind")) {
    m_kind = makeChoiceCombo(this, layer, QStringLiteral("kind"), QString());
    m_kind->setObjectName(QStringLiteral("kaFeatureFormKind"));
    form->addRow(QStringLiteral("종류"), m_kind);
  } else if (fields.nameIndex >= 0) {
    m_name = new QLineEdit(this);
    m_name->setPlaceholderText(QStringLiteral("예: 조사명"));
    form->addRow(QStringLiteral("이름"), m_name);
  }
  // 시대 is offered only when this form writes it (applyTo knows the feature).
  if (featureId != FID_NULL && layer && layer->fields().lookupField(QStringLiteral("period")) >= 0) {
    m_period = makeChoiceCombo(this, layer, QStringLiteral("period"), QString());
    m_period->setObjectName(QStringLiteral("kaFeatureFormPeriod"));
    form->addRow(QStringLiteral("시대"), m_period);
  }
  if (fields.numberIndex >= 0) {
    m_number = new QLineEdit(this);
    m_number->setObjectName(QStringLiteral("kaFeatureFormNumber"));
    m_number->setPlaceholderText(QStringLiteral("예: 1호"));
    form->addRow(QStringLiteral("번호"), m_number);
    m_numberNote = new QLabel(this);
    m_numberNote->setObjectName(QStringLiteral("kaFeatureFormNumberNote"));
    m_numberNote->setWordWrap(true);
    m_numberNote->setStyleSheet(QStringLiteral("color: #B45309;"));
    m_numberNote->hide();
    form->addRow(QString(), m_numberNote);
    // Numbers already used, per kind, read once.
    const FeatureNumbering::Fields nf = FeatureNumbering::fieldsOf(layer);
    m_numberTemplate = FeatureNumbering::defaultTemplate(nf.numberName.toLower());
    m_groupByKind = nf.kind >= 0;
    if (nf.number >= 0) {
      QgsFeatureRequest request;
      request.setFlags(Qgis::FeatureRequestFlag::NoGeometry);
      request.setSubsetOfAttributes(nf.kind >= 0 ? QgsAttributeList{nf.number, nf.kind} : QgsAttributeList{nf.number});
      QgsFeatureIterator it = layer->getFeatures(request);
      QgsFeature f;
      while (it.nextFeature(f)) {
        const QString number = f.attribute(nf.number).toString().trimmed();
        if (f.attribute(nf.number).isNull() || number.isEmpty()) continue;
        const QString kind = nf.kind >= 0 && !f.attribute(nf.kind).isNull() ? f.attribute(nf.kind).toString() : QString();
        m_numbersByKind[FeaturePresets::kindKey(kind)] << number;
      }
    }
    connect(m_number, &QLineEdit::textEdited, this, [this] {
      m_numberTyped = true;
      updateNumberHint();
    });
    if (m_kind) connect(m_kind, &QComboBox::currentTextChanged, this, [this] { updateNumberHint(); });
    updateNumberHint();
  }
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
  buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("저장"));
  buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("건너뛰기"));
  form->addRow(buttons);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void KaFeatureFormDialog::updateNumberHint() {
  if (!m_number) return;
  // 유구번호 runs per kind; other numbers (유물·단면·점) run over the whole layer.
  const QString kind = m_groupByKind && m_kind ? m_kind->currentText() : QString();
  const QStringList used = m_numbersByKind.value(FeaturePresets::kindKey(kind));
  // The next number of this kind fills the box until the user types one.
  if (!m_numberTyped) {
    const QSignalBlocker block(m_number);
    m_number->setText(FeatureNumbering::nextFrom(used, m_numberTemplate));
  }
  const QString wanted = FeatureNumbering::normalized(m_number->text());
  const bool repeated = !wanted.isEmpty() && std::any_of(used.cbegin(), used.cend(), [&](const QString& n) {
    return FeatureNumbering::normalized(n) == wanted;
  });
  m_numberNote->setText(repeated ? QStringLiteral("%1 이미 있는 번호입니다(%2). 그래도 저장할 수 있습니다.")
                                       .arg(m_groupByKind ? QStringLiteral("같은 종류에") : QStringLiteral("이 레이어에"),
                                            m_number->text().trimmed())
                                 : QString());
  m_numberNote->setVisible(repeated);
}

bool KaFeatureFormDialog::canOffer(const QgsVectorLayer* layer) {
  const auto fields = LayerOps::featureFormFields(layer);
  return fields.nameIndex >= 0 || fields.numberIndex >= 0;
}

QString KaFeatureFormDialog::nameText() const {
  if (m_kind) return valueEditorText(m_kind);
  return m_name ? m_name->text().trimmed() : QString();
}

QString KaFeatureFormDialog::numberText() const {
  return m_number ? m_number->text().trimmed() : QString();
}

QString KaFeatureFormDialog::periodText() const {
  return m_period ? valueEditorText(m_period) : QString();
}

bool KaFeatureFormDialog::applyTo(QgsVectorLayer* layer, QString* errorOut) const {
  if (!layer || m_featureId == FID_NULL) {
    if (errorOut) *errorOut = QStringLiteral("도형을 찾지 못했습니다.");
    return false;
  }
  if (!LayerOps::applyFeatureFormValues(layer, static_cast<qint64>(m_featureId), nameText(), numberText(), errorOut))
    return false;
  const int periodIndex = layer->fields().lookupField(QStringLiteral("period"));
  const QString period = periodText();
  if (m_period && periodIndex >= 0 && !period.isEmpty() &&
      !layer->changeAttributeValue(m_featureId, periodIndex, period)) {
    if (errorOut) *errorOut = QStringLiteral("시대를 쓰지 못했습니다.");
    return false;
  }
  FeatureRecord::touch(layer, m_featureId);
  return true;
}

QWidget* KaFeatureFormDialog::createValueEditor(QWidget* parent, const QgsVectorLayer* layer, const QString& field,
                                                const QString& current) {
  if (isChoiceField(field)) return makeChoiceCombo(parent, layer, field, current);
  auto* edit = new QLineEdit(parent);
  edit->setText(current);
  edit->setPlaceholderText(placeholderFor(field));
  return edit;
}

QString KaFeatureFormDialog::valueEditorText(const QWidget* editor) {
  if (const auto* combo = qobject_cast<const QComboBox*>(editor)) {
    const QString text = combo->currentText().trimmed();
    const QString field = combo->property(kFieldProperty).toString();
    const QPointer<QObject> layer = combo->property(kLayerProperty).value<QPointer<QObject>>();
    if (text.isEmpty() || field.isEmpty()) return text;
    return FeatureRecord::canonicalValue(qobject_cast<const QgsVectorLayer*>(layer.data()), field, text);
  }
  if (const auto* edit = qobject_cast<const QLineEdit*>(editor)) return edit->text().trimmed();
  return {};
}
