#include "KaBeginnerRibbon.h"
#include "KaTheme.h"
#include <QDebug>

#include <algorithm>

#include <QAction>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QResizeEvent>
#include <QScopedValueRollback>
#include <QShowEvent>
#include <QSizePolicy>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int kNormalLook = 12;  // looks()[12]: tile 32 with the labels shown

int labelWidth(const QToolButton* button) {
  return QFontMetrics(button->font()).horizontalAdvance(button->text());
}

// The width of one chip at a look: its fixed width when the labels are hidden, otherwise as wide as the
// label plus the padding, never under the minimum or the tile plus 8 px. A label wider than the widest
// chip is cut there (its full wording is in the tooltip).
int chipWidth(const RibbonLook& look, int labelPx) {
  const auto& metrics = KaTheme::buttonMetrics();
  if (!look.labels) return look.chipWidth;
  return std::max({metrics.ribbonChipWidth, look.tile + 8,
                   std::min(labelPx, metrics.ribbonMaxLabelWidth) + metrics.ribbonLabelPadding});
}

// The chip follows its tile: tile + 6 px + the label line + 6 px, or tile + 12 px without the label.
int chipHeight(const RibbonLook& look, int lineHeight) {
  return look.tile + 12 + (look.labels ? lineHeight : 0);
}

void drawChip(QToolButton* button, const RibbonLook& look, int lineHeight) {
  button->setToolButtonStyle(look.labels ? Qt::ToolButtonTextUnderIcon : Qt::ToolButtonIconOnly);
  button->setIconSize(QSize(look.tile, look.tile));
  // The chip's own size; leftover window width is not given to these buttons. The style sheet does not
  // pin the size, so a repolish keeps it. https://doc.qt.io/qt-6.8/qwidget.html#setFixedSize
  button->setFixedSize(chipWidth(look, labelWidth(button)), chipHeight(look, lineHeight));
  button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

// With the labels hidden the tooltip is the only place a chip shows its name, so it always holds it. An
// action that sets its own tooltip later reaches the chip through setDefaultAction; this runs again then.
void keepNameInTip(QToolButton* button) {
  const QString text = button->text();
  const QString tip = button->toolTip();
  if (text.isEmpty() || tip.contains(text)) return;
  button->setToolTip(tip.isEmpty() ? text : text + QStringLiteral(" — ") + tip);
}

}  // namespace

KaBeginnerRibbon::KaBeginnerRibbon(QWidget* parent) : QWidget(parent), m_lookIndex(kNormalLook) {
  setObjectName(QStringLiteral("beginnerRibbon"));
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  m_row = new QHBoxLayout(this);
  const auto& metrics = KaTheme::buttonMetrics();
  m_row->setContentsMargins(metrics.ribbonGroupPad, metrics.buttonPadding,
                           metrics.ribbonGroupPad, metrics.buttonPadding);
  m_row->setSpacing(metrics.ribbonChipGap);
  m_row->setSizeConstraint(QLayout::SetNoConstraint);
  m_row->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  // Groups stay packed. addStretch takes leftover window width so chips
  // do not share it. https://doc.qt.io/qt-6.8/qboxlayout.html#addStretch
  m_row->addStretch(1);
}

QFrame* KaBeginnerRibbon::addGroup(const QString& id, const QString& caption) {
  if (m_groups.contains(id))
    return m_groups.value(id);
  auto* fr = new QFrame(this);
  fr->setObjectName(QStringLiteral("ribbonGroup"));
  fr->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  auto* vl = new QVBoxLayout(fr);
  const auto& metrics = KaTheme::buttonMetrics();
  vl->setContentsMargins(metrics.ribbonGroupPad, metrics.buttonPadding,
                        metrics.ribbonGroupPad, metrics.buttonPadding);
  vl->setSpacing(metrics.buttonPadding);
  auto* cap = new QLabel(caption, fr);
  cap->setObjectName(QStringLiteral("ribbonGroupCaption"));
  cap->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  cap->setWordWrap(false);
  cap->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  auto* btns = new QHBoxLayout();
  btns->setContentsMargins(0, 0, 0, 0);
  btns->setSpacing(metrics.ribbonChipGap);
  btns->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  vl->addWidget(cap, 0, Qt::AlignLeft);
  vl->addLayout(btns);
  m_row->insertWidget(groupInsertIndex(), fr, 0, Qt::AlignLeft | Qt::AlignVCenter);
  m_groups.insert(id, fr);
  m_btnRows.insert(id, btns);
  m_groupOrder.append(id);
  return fr;
}

QHBoxLayout* KaBeginnerRibbon::buttonRow(const QString& groupId) const {
  return m_btnRows.value(groupId, nullptr);
}

QToolButton* KaBeginnerRibbon::addAction(const QString& groupId, QAction* action) {
  QHBoxLayout* row = buttonRow(groupId);
  if (!row || !action)
    return nullptr;
  auto* b = new QToolButton(m_groups.value(groupId));
  action->setText(twoLine(action->text()));
  b->setDefaultAction(action);
  applyTwoLine(b);
  b->installEventFilter(this);
  row->addWidget(b, 0, Qt::AlignLeft | Qt::AlignTop);
  applyLook(m_lookIndex, true);
  return b;
}

void KaBeginnerRibbon::addWidget(const QString& groupId, QWidget* widget) {
  QHBoxLayout* row = buttonRow(groupId);
  if (!row || !widget)
    return;
  auto* b = qobject_cast<QToolButton*>(widget);
  if (b) {
    b->setText(twoLine(b->text()));
    applyTwoLine(b);
    b->installEventFilter(this);
  }
  row->addWidget(widget, 0, Qt::AlignLeft | Qt::AlignTop);
  if (b) applyLook(m_lookIndex, true);
  else updateGeometry();
}

QString KaBeginnerRibbon::twoLine(const QString& text) {
  QString t = text.trimmed();
  t.replace(QLatin1Char('\n'), QLatin1Char(' '));
  t.replace(QLatin1Char('\r'), QLatin1Char(' '));
  while (t.contains(QLatin1String("  ")))
    t.replace(QLatin1String("  "), QStringLiteral(" "));
  return t.trimmed();
}

int KaBeginnerRibbon::groupInsertIndex() const {
  return std::max(0, m_row->count() - 1);  // before the closing stretch
}

void KaBeginnerRibbon::applyTwoLine(QToolButton* button) {
  if (!button)
    return;
  button->setText(twoLine(button->text()));
  const auto& metrics = KaTheme::buttonMetrics();
  // Auto-raise only so Qt asks the icon for its hover look (the tile's hover fill, QIcon::Active);
  // the style sheet paints no raised face for a chip either way.
  button->setAutoRaise(true);
  button->setFocusPolicy(Qt::TabFocus);
  // Polish first: the sheet's family and weight are what the label is drawn (and measured) in.
  button->ensurePolished();
  QFont font = button->font();
  font.setPixelSize(metrics.ribbonFontSize);
  const QString text = button->text();
  // Every current label (widest: 「다른 이름」, 「검수·제출」, about 57 px at 13 px) keeps full size. Only a
  // label wider than ribbonMaxLabelWidth shrinks, to one floor for every chip.
  const auto overflows = [&] { return QFontMetrics(font).horizontalAdvance(text) > metrics.ribbonMaxLabelWidth; };
  while (font.pixelSize() > metrics.ribbonMinFontSize && overflows())
    font.setPixelSize(font.pixelSize() - 1);
  button->setFont(font);
  // The application sheet sets every chip's font-size; a smaller size only sticks
  // as the chip's own sheet.
  if (font.pixelSize() < metrics.ribbonFontSize)
    button->setStyleSheet(QStringLiteral("font-size: %1px;").arg(font.pixelSize()));
  else if (button->styleSheet().startsWith(QLatin1String("font-size:")))
    button->setStyleSheet(QString());  // only the size this function set earlier
  keepNameInTip(button);
  button->ensurePolished();
  drawChip(button, looks().at(kNormalLook), QFontMetrics(button->font()).height());
}

QFrame* KaBeginnerRibbon::group(const QString& id) const {
  return m_groups.value(id, nullptr);
}

QList<QToolButton*> KaBeginnerRibbon::chips() const {
  QList<QToolButton*> out;
  for (const QString& id : m_groupOrder) {
    QHBoxLayout* row = m_btnRows.value(id);
    if (!row) continue;
    for (int i = 0; i < row->count(); ++i) {
      QLayoutItem* item = row->itemAt(i);
      if (auto* button = item ? qobject_cast<QToolButton*>(item->widget()) : nullptr) out.append(button);
    }
  }
  return out;
}

QList<QToolButton*> KaBeginnerRibbon::tabButtons() const {
  QList<QToolButton*> out;
  for (QToolButton* button : chips())
    if (button->focusPolicy() != Qt::NoFocus) out.append(button);
  return out;
}

void KaBeginnerRibbon::applyTabOrder() {
  const QList<QToolButton*> buttons = tabButtons();
  for (int i = 0; i + 1 < buttons.size(); ++i)
    QWidget::setTabOrder(buttons.at(i), buttons.at(i + 1));
}

bool KaBeginnerRibbon::eventFilter(QObject* watched, QEvent* event) {
  if (event && event->type() == QEvent::KeyPress) {
    const auto* key = static_cast<const QKeyEvent*>(event);
    if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
      if (auto* button = qobject_cast<QToolButton*>(watched); button && button->isEnabled()) {
        button->animateClick();
        return true;
      }
    }
  } else if (event && event->type() == QEvent::ToolTipChange) {
    if (auto* button = qobject_cast<QToolButton*>(watched)) keepNameInTip(button);
  }
  return QWidget::eventFilter(watched, event);
}

QList<RibbonLook> KaBeginnerRibbon::looks() {
  static const QList<RibbonLook> table = [] {
    QList<RibbonLook> out;
    for (int tile = 56; tile >= 34; tile -= 2) out.append({tile, qRound(tile * 18 / 32.0), 0, true});
    out.append({32, 18, 0, true});
    out.append({32, 18, 40, false});
    out.append({24, 14, 30, false});
    out.append({20, 12, 26, false});  // the spec table says 12 here (18/32 of 20 would round to 11)
    return out;
  }();
  return table;
}

int KaBeginnerRibbon::chooseLook(const QList<int>& widths, int available) {
  for (int i = 0; i < widths.size(); ++i)
    if (widths.at(i) <= available) return i;
  return std::max(0, int(widths.size()) - 1);
}

RibbonLook KaBeginnerRibbon::look() const {
  return looks().at(m_lookIndex);
}

// Per group: the wider of its name and its chips side by side, plus the group's own margins and edge.
// The chips are measured, not laid out, so every size is planned without touching a widget.
QList<int> KaBeginnerRibbon::lookWidths() const {
  const QList<RibbonLook> all = looks();
  const auto margins = m_row->contentsMargins();
  QList<int> widths(all.size(), margins.left() + margins.right());
  for (const QString& id : m_groupOrder) {
    QFrame* frame = m_groups.value(id);
    QHBoxLayout* row = m_btnRows.value(id);
    frame->ensurePolished();  // the sheet's edge on the group counts
    QList<int> chipsAt(all.size(), 0);
    int count = 0;
    for (int i = 0; i < row->count(); ++i) {
      QWidget* widget = row->itemAt(i)->widget();
      if (!widget || (widget->isHidden() && widget->testAttribute(Qt::WA_WState_ExplicitShowHide))) continue;
      ++count;
      const auto* chip = qobject_cast<QToolButton*>(widget);
      const int label = chip ? labelWidth(chip) : 0;
      for (int k = 0; k < all.size(); ++k)
        chipsAt[k] += chip ? chipWidth(all.at(k), label) : widget->sizeHint().width();
    }
    const auto* caption = frame->findChild<QLabel*>(QStringLiteral("ribbonGroupCaption"));
    const int captionWidth = caption ? caption->sizeHint().width() : 0;
    // What the group adds around its widest part: its margins and the sheet's edge.
    const int chrome = frame->sizeHint().width() - std::max(captionWidth, row->sizeHint().width());
    for (int k = 0; k < all.size(); ++k)
      widths[k] += std::max(captionWidth, chipsAt.at(k) + std::max(0, count - 1) * row->spacing()) + chrome +
                   m_row->spacing();
  }
  return widths;
}

int KaBeginnerRibbon::lineHeight() const {
  int line = 0;
  for (const QToolButton* chip : chips()) line = std::max(line, QFontMetrics(chip->font()).height());
  return line;
}

int KaBeginnerRibbon::rowHeight() const {
  const auto margins = m_row->contentsMargins();
  int height = 0;
  for (const QString& id : m_groupOrder) height = std::max(height, m_groups.value(id)->sizeHint().height());
  return height + margins.top() + margins.bottom();
}

QSize KaBeginnerRibbon::sizeHint() const {
  return QSize(lookWidths().value(m_lookIndex), rowHeight());
}

QSize KaBeginnerRibbon::minimumSizeHint() const {
  return QSize(lookWidths().last(), rowHeight());
}

void KaBeginnerRibbon::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  updateLook();
}

void KaBeginnerRibbon::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  updateLook();
}

void KaBeginnerRibbon::updateLook() {
  if (m_updatingLook) return;
  const QScopedValueRollback<bool> busy(m_updatingLook, true);
  applyLook(chooseLook(lookWidths(), width()));
}

void KaBeginnerRibbon::applyLook(int index, bool force) {
  const int line = lineHeight();
  int labels = 0;
  for (const QToolButton* chip : chips()) labels += labelWidth(chip);
  if (index == m_lookIndex && line == m_appliedLine && labels == m_appliedLabels && !force) return;
  m_lookIndex = index;
  m_appliedLine = line;
  m_appliedLabels = labels;
  const RibbonLook now = looks().at(index);
  for (QToolButton* chip : chips()) drawChip(chip, now, line);
  updateGeometry();
  // One session-log line per change of the chosen size (skip a hidden ribbon and the pre-layout pass, 100 px wide),
  // so a screen that shows icons too small or labels hidden can be measured instead of guessed.
  if (index != m_loggedLook && isVisible() && width() > 200) {
    m_loggedLook = index;
    qWarning().noquote()  // the app's message handler files it under [qt/warn]; tests just print it
        << QStringLiteral("[ribbon] 크기 타일 %1 · 글자 %2 · 창 %3 · 가용 %4")
               .arg(now.tile)
               .arg(now.labels ? QStringLiteral("보임") : QStringLiteral("숨김"))
               .arg(window()->width())
               .arg(width());
  }
}
