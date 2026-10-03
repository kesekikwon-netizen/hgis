// Writing side of the 「선택한 유구」 card (split from KaFeatureCard.cpp to keep each file
// small): one value at a time into the layer's edit buffer, never a commit.
#include "KaFeatureCard.h"

#include "KaChip.h"
#include "KaFeatureFormDialog.h"
#include "core/FeatureRecord.h"
#include "core/LayerOps.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>

#include <qgsfield.h>
#include <qgsfields.h>
#include <qgsvectorlayer.h>

namespace {
QString shownText(const QWidget* widget) {
  if (!widget) return {};
  if (const auto* combo = qobject_cast<const QComboBox*>(widget)) return combo->currentText().trimmed();
  if (const auto* edit = qobject_cast<const QLineEdit*>(widget)) return edit->text().trimmed();
  if (const auto* plain = qobject_cast<const QPlainTextEdit*>(widget)) return plain->toPlainText().trimmed();
  if (const auto* label = qobject_cast<const QLabel*>(widget)) return label->text();
  return {};
}

QString storedText(const QVariant& value) { return value.isNull() ? QString() : value.toString().trimmed(); }
}  // namespace

QString KaFeatureCard::rowText(const QString& row) const {
  if (row == QLatin1String("title")) return shownText(m_title);
  if (row == QLatin1String("header")) return m_header && !m_header->isHidden() ? shownText(m_name) : QString();
  if (row == QLatin1String("layer_kind")) return m_header && !m_header->isHidden() ? shownText(m_layerKind) : QString();
  if (row == QLatin1String("number")) return shownText(m_number);
  if (row == QLatin1String("kind")) return shownText(m_kind);
  if (row == QLatin1String("period")) return shownText(m_period);
  if (row == QLatin1String("note")) return shownText(m_note);
  if (row == QLatin1String("area")) return shownText(m_area);
  if (row == QLatin1String("perimeter")) return shownText(m_perimeter);
  if (row == QLatin1String("state")) return shownText(m_state);
  if (row == QLatin1String("note_number")) return m_numberNote && !m_numberNote->isHidden() ? m_numberNote->text() : QString();
  for (const auto& [field, edit] : m_nameEdits)
    if (row == field) return shownText(edit);
  return {};
}

bool KaFeatureCard::setValue(const QString& field, const QString& text, QString* errorOut) {
  const auto fail = [errorOut](const QString& message) {
    if (errorOut) *errorOut = message;
    return false;
  };
  if (!hasFeature()) return fail(QStringLiteral("고른 도형이 없습니다."));
  QgsVectorLayer* layer = m_layer.data();
  const int index = layer->fields().lookupField(field);
  if (index < 0) return fail(QStringLiteral("이 레이어에는 「%1」 칸이 없습니다.").arg(field));
  const QgsFeatureId fid = m_fid;
  const QgsFeature before = layer->getFeature(fid);
  if (!before.isValid()) return fail(QStringLiteral("도형을 찾지 못했습니다."));

  const QgsField def = layer->fields().at(index);
  const QString tidy = text.trimmed();
  QVariant value;
  if (def.isNumeric()) {
    bool ok = true;
    const double number = tidy.isEmpty() ? 0.0 : tidy.toDouble(&ok);
    if (!ok) return fail(QStringLiteral("숫자만 넣을 수 있습니다(소수점은 .): %1").arg(tidy));
    value = tidy.isEmpty() ? QVariant(QMetaType(def.type())) : QVariant(number);
  } else if (field == QLatin1String("kind") || field == QLatin1String("period")) {
    value = tidy.isEmpty() ? QString() : FeatureRecord::canonicalValue(layer, field, tidy);
  } else {
    value = tidy;
  }
  const QVariant current = before.attribute(index);
  if (storedText(current) == storedText(value)) return true;  // unchanged: no undo step

  // One undoable edit command; 저장(Ctrl+S) stays the only write to the file.
  if (!LayerOps::runEditCommand(layer, QStringLiteral("기록 고치기"), [&]() {
        if (!layer->changeAttributeValue(fid, index, value, current)) return false;
        FeatureRecord::touch(layer, fid);  // updated_at when the survey has it
        return true;
      }, errorOut))
    return false;
  emit edited(layer, fid, before);
  refresh();
  return true;
}

void KaFeatureCard::commitEditor(const QString& field, QWidget* editor) {
  // m_readOnly, not editor->isEnabled(): when the card is locked for drawing, Qt disables the
  // editor before its FocusOut arrives, and that last typed value must still reach the buffer.
  if (!hasFeature() || !editor || m_readOnly) return;
  // Kind/period come back in the survey's own spelling (combo); others as typed.
  const auto* plain = qobject_cast<const QPlainTextEdit*>(editor);
  const QString text = plain ? plain->toPlainText().trimmed() : KaFeatureFormDialog::valueEditorText(editor);
  QString error;
  if (!setValue(field, text, &error) && m_state)
    setState(QStringLiteral("바꾸지 못했습니다: %1").arg(error), true, true);
}
