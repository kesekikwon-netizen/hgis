// Editors of the 「선택한 유구」 card (split from KaFeatureCard.cpp): the form rows, the
// read-only 「자동 계산」 measure boxes and the two-line 메모 that commits on focus-out.
#include "KaFeatureCard.h"

#include "KaFeatureFormDialog.h"
#include "KaIcons.h"
#include "KaTheme.h"
#include "core/FeatureNumbering.h"
#include "core/LayerOps.h"

#include <QComboBox>
#include <QEvent>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

#include <qgsfields.h>
#include <qgsvectorlayer.h>

namespace {
constexpr int kBodyIndex = 3;  // title, header line, empty sentence, then the editors
constexpr int kNoteMinHeight = 56;
constexpr int kNoteMaxHeight = 72;

QString numberLabel(const QString& field) {
  if (field == QLatin1String("feature_no")) return QStringLiteral("유구번호");
  if (field == QLatin1String("artifact_no")) return QStringLiteral("유물번호");
  if (field == QLatin1String("section_id")) return QStringLiteral("단면번호");
  return QStringLiteral("점 이름");
}
}  // namespace

// [🔒 자동 계산 ............ 32.91 m²]: a box that looks like a field but never edits.
QWidget* KaFeatureCard::measureField(QLabel** valueOut) {
  auto* box = new QFrame(m_body);
  box->setObjectName(QStringLiteral("kaMeasureField"));
  box->setMinimumHeight(32);
  auto* line = new QHBoxLayout(box);
  line->setContentsMargins(8, 0, 8, 0);
  line->setSpacing(6);
  auto* lock = new QLabel(box);
  lock->setObjectName(QStringLiteral("kaMeasureLock"));
  lock->setPixmap(KaIcons::glyphPixmap(QStringLiteral("lock"), KaTheme::tokens().inkMuted, 12, devicePixelRatioF()));
  lock->setFixedSize(12, 12);
  auto* caption = new QLabel(QStringLiteral("자동 계산"), box);
  caption->setObjectName(QStringLiteral("kaMeasureAuto"));
  auto* value = new QLabel(box);
  value->setObjectName(QStringLiteral("kaMeasureValue"));
  value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  value->setTextInteractionFlags(Qt::TextSelectableByMouse);
  line->addWidget(lock, 0, Qt::AlignVCenter);
  line->addWidget(caption);
  line->addStretch(1);
  line->addWidget(value);
  *valueOut = value;
  return box;
}

void KaFeatureCard::buildEditors() {
  dropEditors();
  m_body = new QWidget(this);
  m_form = new QFormLayout(m_body);
  m_form->setContentsMargins(0, 0, 0, 0);
  m_form->setVerticalSpacing(8);
  static_cast<QVBoxLayout*>(layout())->insertWidget(kBodyIndex, m_body);
  const QgsFields fields = m_layer->fields();
  const auto has = [&fields](const char* name) { return fields.lookupField(QString::fromLatin1(name)) >= 0; };
  const auto watch = [this](const QString& field, QWidget* editor) {
    if (auto* combo = qobject_cast<QComboBox*>(editor)) {
      connect(combo, &QComboBox::activated, this, [this, field, editor] { commitEditor(field, editor); });
      connect(combo->lineEdit(), &QLineEdit::editingFinished, this, [this, field, editor] { commitEditor(field, editor); });
    } else if (auto* edit = qobject_cast<QLineEdit*>(editor)) {
      connect(edit, &QLineEdit::editingFinished, this, [this, field, editor] { commitEditor(field, editor); });
    }
    auto* combo = qobject_cast<QComboBox*>(editor);
    if (auto* line = combo ? combo->lineEdit() : qobject_cast<QLineEdit*>(editor))
      connect(line, &QLineEdit::returnPressed, this, &KaFeatureCard::valueEntered);  // Enter: this box only
  };
  m_numberField = FeatureNumbering::fieldsOf(m_layer).numberName;
  if (!m_numberField.isEmpty()) {
    auto* row = new QWidget(m_body);
    auto* line = new QHBoxLayout(row);
    line->setContentsMargins(0, 0, 0, 0);
    m_number = new QLineEdit(row);
    m_number->setObjectName(QStringLiteral("kaFeatureCardNumber"));
    m_nextNumber = new QToolButton(row);
    m_nextNumber->setText(QStringLiteral("다음 번호"));
    line->addWidget(m_number, 1);
    line->addWidget(m_nextNumber);
    m_form->addRow(numberLabel(m_numberField), row);
    m_numberNote = new QLabel(m_body);
    m_numberNote->setWordWrap(true);
    m_numberNote->setStyleSheet(QStringLiteral("color: %1;").arg(KaTheme::tokens().warn.name()));
    m_form->addRow(QString(), m_numberNote);
    watch(m_numberField, m_number);
    connect(m_nextNumber, &QToolButton::clicked, this, [this] {
      const QString next = FeatureNumbering::suggestNext(m_layer, KaFeatureFormDialog::valueEditorText(m_kind));
      if (!next.isEmpty()) setValue(m_numberField, next);
    });
  }
  const auto addTextRow = [&](const char* field, const char* label) {
    auto* edit = new QLineEdit(m_body);
    m_form->addRow(QString::fromUtf8(label), edit);
    watch(QString::fromLatin1(field), edit);
    m_textEdits.append({QString::fromLatin1(field), edit});
    return edit;
  };
  // A survey area has no number, kind or period: its record is 조사명·유적명 (user 2026-10-03).
  const std::pair<const char*, const char*> nameRows[] = {{"survey_name", "조사명"}, {"site_name", "유적명"}, {"name", "이름"}};
  if (LayerOps::layerKeyOf(m_layer) == QLatin1String("survey_area"))
    for (const auto& [field, label] : nameRows)
      if (has(field)) addTextRow(field, label);
  if (has("kind")) {
    m_kind = KaFeatureFormDialog::createValueEditor(m_body, m_layer, QStringLiteral("kind"), QString());
    m_form->addRow(QStringLiteral("종류"), m_kind);
    watch(QStringLiteral("kind"), m_kind);
  }
  if (has("period")) {
    m_period = KaFeatureFormDialog::createValueEditor(m_body, m_layer, QStringLiteral("period"), QString());
    m_form->addRow(QStringLiteral("시대"), m_period);
    watch(QStringLiteral("period"), m_period);
  }
  // 조사 상태·조사자·조사일 too, without opening the 조사카드 (user 2026-10-03, answer 「진행」).
  if (has("status")) addTextRow("status", "조사 상태");
  if (has("surveyor")) addTextRow("surveyor", "조사자");
  if (has("surv_date")) addTextRow("surv_date", "조사일")->setPlaceholderText(QStringLiteral("예: 2026-10-03"));
  const Qgis::GeometryType gt = m_layer->geometryType();
  if (gt == Qgis::GeometryType::Polygon) m_form->addRow(QStringLiteral("면적"), measureField(&m_area));
  if (gt == Qgis::GeometryType::Polygon || gt == Qgis::GeometryType::Line)
    m_form->addRow(gt == Qgis::GeometryType::Line ? QStringLiteral("길이") : QStringLiteral("둘레"),
                   measureField(&m_perimeter));
  if (has("note")) {
    m_note = new QPlainTextEdit(m_body);
    m_note->setObjectName(QStringLiteral("kaFeatureCardNote"));
    m_note->setTabChangesFocus(true);
    m_note->setMinimumHeight(kNoteMinHeight);
    m_note->setMaximumHeight(kNoteMaxHeight);
    m_note->setPlaceholderText(QStringLiteral("현장 메모 (두 줄)"));
    m_note->installEventFilter(this);  // commits when the focus leaves the box
    m_form->addRow(QStringLiteral("메모"), m_note);
  }
  m_readOnly = LayerOps::isReferenceLayer(m_layer) || LayerOps::isCadastralLayer(m_layer);
  for (QWidget* editor : {static_cast<QWidget*>(m_number), static_cast<QWidget*>(m_nextNumber), m_kind, m_period,
                          static_cast<QWidget*>(m_note)})
    if (editor) editor->setEnabled(!m_readOnly);
  for (const auto& [field, edit] : std::as_const(m_textEdits)) edit->setEnabled(!m_readOnly);
}

bool KaFeatureCard::eventFilter(QObject* watched, QEvent* event) {
  if (m_note && watched == m_note && event->type() == QEvent::FocusOut) commitEditor(QStringLiteral("note"), m_note);
  return QFrame::eventFilter(watched, event);
}
