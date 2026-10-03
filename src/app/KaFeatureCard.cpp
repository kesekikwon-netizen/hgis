#include "KaFeatureCard.h"

#include "KaChip.h"
#include "KaIcons.h"
#include "core/FeatureNumbering.h"
#include "core/FeatureRecord.h"
#include "core/LayerOps.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>
#include <cmath>

#include <qgsfields.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayereditbuffer.h>

namespace {
// Values from the layer never overwrite a box the user is typing in.
void showText(QWidget* editor, const QString& text) {
  if (!editor) return;
  if (auto* plain = qobject_cast<QPlainTextEdit*>(editor)) {
    if (plain->hasFocus() || plain->toPlainText() == text) return;
    const QSignalBlocker block(plain);
    plain->setPlainText(text);
    return;
  }
  auto* combo = qobject_cast<QComboBox*>(editor);
  auto* edit = combo ? combo->lineEdit() : qobject_cast<QLineEdit*>(editor);
  if (!edit || edit->hasFocus()) return;
  const QSignalBlocker block(editor);
  if (combo) combo->setEditText(text);
  else edit->setText(text);
}

QString metres(double value, const char* unit) {
  return std::isnan(value) ? QStringLiteral("-") : QStringLiteral("%1 %2").arg(value, 0, 'f', 2).arg(QString::fromUtf8(unit));
}
}  // namespace

QString KaFeatureCard::titleFor(const QString& key) {
  if (key == QLatin1String("feature_poly") || key == QLatin1String("feature_line")) return QStringLiteral("선택한 유구");
  if (key == QLatin1String("artifact_point")) return QStringLiteral("선택한 유물");
  if (key == QLatin1String("control_points")) return QStringLiteral("선택한 기준점");
  if (key == QLatin1String("section_line")) return QStringLiteral("선택한 단면선");
  if (key == QLatin1String("survey_area")) return QStringLiteral("선택한 조사구역");
  return QStringLiteral("선택한 도형");
}

QString KaFeatureCard::layerKindFor(const QgsVectorLayer* layer) {
  const QString key = LayerOps::layerKeyOf(layer);
  if (key == QLatin1String("feature_poly")) return QStringLiteral("유구 면");
  if (key == QLatin1String("feature_line")) return QStringLiteral("유구 선");
  if (key == QLatin1String("survey_area")) return QStringLiteral("조사구역");
  if (key == QLatin1String("artifact_point")) return QStringLiteral("유물 위치");
  if (key == QLatin1String("control_points")) return QStringLiteral("기준점");
  if (key == QLatin1String("section_line")) return QStringLiteral("단면선");
  return layer ? layer->name() : QString();
}

KaFeatureCard::KaFeatureCard(QWidget* parent) : QFrame(parent) {
  setObjectName(QStringLiteral("kaFeatureCard"));
  setFrameShape(QFrame::StyledPanel);
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(12, 8, 12, 8);
  root->setSpacing(8);
  m_title = new QLabel(this);
  m_title->setObjectName(QStringLiteral("kaFeatureCardTitle"));
  QFont font = m_title->font();
  font.setBold(true);
  m_title->setFont(font);
  root->addWidget(m_title);
  // Header line: 「1호 주거지」 at the left, the layer kind 「유구 면」 muted at the right.
  m_header = new QWidget(this);
  auto* headerLine = new QHBoxLayout(m_header);
  headerLine->setContentsMargins(0, 0, 0, 0);
  headerLine->setSpacing(8);
  m_name = new QLabel(m_header);
  m_name->setObjectName(QStringLiteral("kaFeatureCardName"));
  QFont nameFont = m_name->font();
  nameFont.setPixelSize(15);
  nameFont.setBold(true);
  m_name->setFont(nameFont);
  m_layerKind = new QLabel(m_header);
  m_layerKind->setObjectName(QStringLiteral("kaFeatureCardKind"));
  headerLine->addWidget(m_name, 1);
  headerLine->addWidget(m_layerKind, 0, Qt::AlignRight | Qt::AlignVCenter);
  root->addWidget(m_header);
  m_empty = new QLabel(QStringLiteral("도형 하나를 고르면 기록이 여기에 나옵니다."), this);
  m_empty->setWordWrap(true);
  root->addWidget(m_empty);
  m_state = new KaChip(QString(), KaChip::Tone::Neutral, this);
  m_state->setObjectName(QStringLiteral("kaFeatureCardState"));
  root->addWidget(m_state, 0, Qt::AlignLeft);
  m_openForm = new QPushButton(QStringLiteral("조사카드 열기"), this);
  m_openForm->setObjectName(QStringLiteral("kaFeatureCardOpenForm"));
  m_openForm->setIcon(KaIcons::icon(QStringLiteral("note")));
  m_openForm->setIconSize(QSize(16, 16));
  m_openForm->setMinimumHeight(36);
  m_openForm->setToolTip(QStringLiteral("이 도형의 전체 조사카드(모든 칸)를 창으로 엽니다."));
  connect(m_openForm, &QPushButton::clicked, this, [this] {
    if (hasFeature()) emit formRequested(m_layer.data(), m_fid);
  });
  root->addWidget(m_openForm);
  clear();
}

KaFeatureCard::~KaFeatureCard() {
  for (const QMetaObject::Connection& c : std::as_const(m_connections)) disconnect(c);
}

QgsVectorLayer* KaFeatureCard::layer() const { return m_layer.data(); }

bool KaFeatureCard::hasFeature() const { return m_layer && m_fid != FID_NULL; }

void KaFeatureCard::setTitleVisible(bool visible) { m_title->setVisible(visible); }

void KaFeatureCard::clear() {
  for (const QMetaObject::Connection& c : std::as_const(m_connections)) disconnect(c);
  m_connections.clear();
  m_layer = nullptr;
  m_fid = FID_NULL;
  dropEditors();
  m_title->setText(QStringLiteral("선택한 유구"));
  m_header->hide();
  m_empty->show();
  m_state->clear();
  m_state->hide();
  m_openForm->hide();
  emit featureChanged();
}

void KaFeatureCard::showSelection(QgsVectorLayer* layer) {
  if (!layer || layer->selectedFeatureCount() != 1) {
    clear();
    return;
  }
  setFeature(layer, *layer->selectedFeatureIds().cbegin());
  m_followsSelection = true;
}

void KaFeatureCard::setFeature(QgsVectorLayer* layer, QgsFeatureId fid) {
  m_followsSelection = false;
  if (!layer || !layer->isValid() || fid == FID_NULL || !layer->getFeature(fid).isValid()) {
    clear();
    return;
  }
  if (layer == m_layer && fid == m_fid && m_body) {
    refresh();  // same feature again: keep the editors (and anything being typed)
    return;
  }
  if (layer != m_layer) {
    clear();
    watchLayer(layer);
  }
  m_layer = layer;
  m_fid = fid;
  buildEditors();  // fresh choices: values typed on other features show up in the lists
  m_title->setText(titleFor(LayerOps::layerKeyOf(layer)));
  m_layerKind->setText(layerKindFor(layer));
  m_header->show();
  m_empty->hide();
  m_state->show();
  m_openForm->show();
  refresh();
  emit featureChanged();
}

void KaFeatureCard::watchLayer(QgsVectorLayer* layer) {
  const auto same = [this](QgsFeatureId fid) { return fid == m_fid; };
  m_connections << connect(layer, &QgsVectorLayer::attributeValueChanged, this,
                           [this, same](QgsFeatureId fid, int, const QVariant&) { if (same(fid)) refresh(); });
  m_connections << connect(layer, &QgsVectorLayer::geometryChanged, this,
                           [this, same](QgsFeatureId fid, const QgsGeometry&) { if (same(fid)) refresh(); });
  m_connections << connect(layer, &QgsVectorLayer::featureDeleted, this,
                           [this, same](QgsFeatureId fid) { if (same(fid)) clear(); });
  // After 저장 or 되돌리기 a new feature may have another id: refresh() clears then.
  m_connections << connect(layer, &QgsVectorLayer::afterCommitChanges, this, [this] { refresh(); });
  m_connections << connect(layer, &QgsVectorLayer::afterRollBack, this, [this] { refresh(); });
  m_connections << connect(layer, &QgsMapLayer::willBeDeleted, this, [this] { clear(); });
}

void KaFeatureCard::dropEditors() {
  if (m_body) {
    // Deleted later: this may run inside a signal of one of these editors.
    for (QWidget* child : m_body->findChildren<QWidget*>()) disconnect(child, nullptr, this, nullptr);
    layout()->removeWidget(m_body);
    m_body->hide();
    m_body->deleteLater();
  }
  m_body = nullptr;
  m_form = nullptr;
  m_number = nullptr;
  m_nextNumber = nullptr;
  m_numberNote = nullptr;
  m_kind = m_period = nullptr;
  m_note = nullptr;
  m_nameEdits.clear();
  m_area = m_perimeter = nullptr;
}

void KaFeatureCard::setState(const QString& text, bool changed, bool failed) {
  m_state->setText(text);
  m_state->setTone(failed ? KaChip::Tone::Danger : changed ? KaChip::Tone::Warn : KaChip::Tone::Neutral);
  m_state->setToolTip(changed ? QStringLiteral("「저장」 Ctrl+S로 파일에 씁니다.") : QString());
}

void KaFeatureCard::refresh() {
  if (!hasFeature()) return;
  const QgsFeature f = m_layer->getFeature(m_fid);
  if (!f.isValid()) {
    clear();
    return;
  }
  const auto value = [&f](const QString& field) {
    const int index = f.fields().lookupField(field);
    const QVariant v = index >= 0 ? f.attribute(index) : QVariant();
    return v.isNull() ? QString() : v.toString().trimmed();
  };
  const QString kind = value(QStringLiteral("kind"));
  const QString number = m_numberField.isEmpty() ? QString() : value(m_numberField);
  QString header = number.isEmpty() ? kind : kind.isEmpty() ? number : number + QLatin1Char(' ') + kind;
  for (const auto& [field, edit] : std::as_const(m_nameEdits)) {
    showText(edit, value(field));
    if (header.isEmpty()) header = value(field);  // a survey area is named by its 조사명
  }
  m_name->setText(header.isEmpty() ? QStringLiteral("이름 없음") : header);
  showText(m_number, number);
  showText(m_kind, kind);
  showText(m_period, value(QStringLiteral("period")));
  showText(m_note, value(QStringLiteral("note")));
  const FeatureRecord::Measures m = FeatureRecord::measure(m_layer, f);
  if (m_area) m_area->setText(metres(m.area, "m²"));
  if (m_perimeter) m_perimeter->setText(metres(std::isnan(m.length) ? m.perimeter : m.length, "m"));
  if (m_numberNote) {
    const int others = static_cast<int>(FeatureNumbering::sameNumber(m_layer, kind, number, m_fid).size());
    m_numberNote->setText(others > 0 ? QStringLiteral("같은 종류에 같은 번호가 %1개 더 있습니다. 제출 전 검수에서도 알려 드립니다.")
                                           .arg(others)
                                     : QString());
    m_numberNote->setVisible(others > 0);
  }
  if (m_nextNumber) {
    const QString next = FeatureNumbering::suggestNext(m_layer, kind);
    m_nextNumber->setEnabled(m_number && m_number->isEnabled() && !next.isEmpty());
    m_nextNumber->setToolTip(next.isEmpty() ? QString() : QStringLiteral("%1 넣기").arg(next));
  }
  const QgsVectorLayerEditBuffer* buffer = m_layer->editBuffer();
  const bool changed = buffer && (buffer->changedAttributeValues().contains(m_fid) ||
                                  buffer->changedGeometries().contains(m_fid) || buffer->addedFeatures().contains(m_fid));
  setState(changed ? QStringLiteral("변경됨 · 아직 저장 안 됨") : QStringLiteral("저장된 값입니다"), changed);
}
