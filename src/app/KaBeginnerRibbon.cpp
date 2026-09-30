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
#include <QMenu>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QSizePolicy>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>

KaBeginnerRibbon::KaBeginnerRibbon(QWidget* parent) : QWidget(parent) {
  setObjectName(QStringLiteral("beginnerRibbon"));
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  m_row = new QHBoxLayout(this);
  const auto& metrics = KaTheme::buttonMetrics();
  m_row->setContentsMargins(metrics.ribbonGroupPad, metrics.buttonPadding,
                           metrics.ribbonGroupPad, metrics.buttonPadding);
  m_row->setSpacing(metrics.ribbonChipGap);
  m_row->setSizeConstraint(QLayout::SetNoConstraint);
  m_row->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  m_overflow = new QToolButton(this);
  m_overflow->setObjectName(QStringLiteral("ribbonOverflow"));
  m_overflow->setText(QStringLiteral("더 많은 작업"));
  m_overflow->setFocusPolicy(Qt::TabFocus);
  m_overflow->setPopupMode(QToolButton::InstantPopup);
  m_overflow->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  m_overflowMenu = new QMenu(m_overflow);
  m_overflowMenu->setObjectName(QStringLiteral("ribbonOverflowMenu"));
  m_overflow->setMenu(m_overflowMenu);
  // Groups stay packed. addStretch takes leftover window width so chips
  // do not share it. https://doc.qt.io/qt-6.8/qboxlayout.html#addStretch
  m_row->addWidget(m_overflow, 0, Qt::AlignLeft | Qt::AlignVCenter);
  m_row->addStretch(1);
  m_overflow->hide();
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
  auto* menu = m_overflowMenu->addMenu(caption);
  menu->setObjectName(QStringLiteral("ribbonOverflowGroup_") + id);
  auto* scroll = new QScrollArea(menu);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  auto* host = new QWidgetAction(menu);
  host->setDefaultWidget(scroll);
  menu->addAction(host);
  menu->menuAction()->setVisible(false);
  m_groupMenus.insert(id, menu);
  m_groupScrolls.insert(id, scroll);
  connect(menu, &QMenu::aboutToShow, this, [scroll] {
    QTimer::singleShot(0, scroll, [scroll] {
      if (auto* content = scroll->widget()) {
        const auto controls = content->findChildren<QWidget*>();
        for (auto* control : controls) {
          if (control->isVisible() && control->isEnabled() && control->focusPolicy() != Qt::NoFocus) {
            control->setFocus(Qt::PopupFocusReason);
            break;
          }
        }
      }
    });
  });
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
  if (action)
    action->setText(twoLine(action->text()));
  b->setDefaultAction(action);
  applyTwoLine(b);
  b->installEventFilter(this);
  row->addWidget(b, 0, Qt::AlignLeft | Qt::AlignTop);
  connect(b, &QToolButton::clicked, m_overflowMenu, &QMenu::close);
  return b;
}

void KaBeginnerRibbon::addWidget(const QString& groupId, QWidget* widget) {
  QHBoxLayout* row = buttonRow(groupId);
  if (!row || !widget)
    return;
  if (auto* b = qobject_cast<QToolButton*>(widget)) {
    b->setText(twoLine(b->text()));
    applyTwoLine(b);
    b->installEventFilter(this);
    connect(b, &QToolButton::clicked, m_overflowMenu, &QMenu::close);
  }
  row->addWidget(widget, 0, Qt::AlignLeft | Qt::AlignTop);
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
  return std::max(0, m_row->count() - 2);
}

void KaBeginnerRibbon::applyTwoLine(QToolButton* button) {
  if (!button)
    return;
  button->setText(twoLine(button->text()));
  const auto& metrics = KaTheme::buttonMetrics();
  button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  button->setAutoRaise(false);
  button->setFocusPolicy(Qt::TabFocus);
  button->setIconSize(QSize(metrics.ribbonIconSize, metrics.ribbonIconSize));
  QFont font = button->font();
  font.setPixelSize(metrics.ribbonFontSize);
  const QString text = button->text();
  // The style sheet gives the label the chip's whole 56 px content box, so every
  // current label (widest: 「다른 이름」, 「제출 변환」 at 52 px) stays at 12 px.
  const int textRoom = std::max(8, metrics.ribbonChipWidth - 2);
  const auto overflows = [&] { return QFontMetrics(font).horizontalAdvance(text) > textRoom; };
  // One floor for every chip, so a row never mixes 12 px with 9 px labels.
  while (font.pixelSize() > metrics.ribbonMinFontSize && overflows())
    font.setPixelSize(font.pixelSize() - 1);
  button->setFont(font);
  // The application sheet sets every chip's font-size; a smaller size only sticks
  // as the chip's own sheet.
  if (font.pixelSize() < metrics.ribbonFontSize)
    button->setStyleSheet(QStringLiteral("font-size: %1px;").arg(font.pixelSize()));
  else if (button->styleSheet().startsWith(QLatin1String("font-size:")))
    button->setStyleSheet(QString());  // only the size this function set earlier
  // Screen readers already read the label (QAccessibleToolButton uses text()).
  // A label still too wide at the floor keeps its full wording in the tooltip.
  if (overflows() && !button->toolTip().contains(text))
    button->setToolTip(button->toolTip().isEmpty() ? text : text + QStringLiteral(" — ") + button->toolTip());
  button->ensurePolished();
  // One chip size for every ribbon action. Leftover window width is not
  // given to these buttons. https://doc.qt.io/qt-6.8/qwidget.html#setFixedSize
  button->setFixedSize(metrics.ribbonChipWidth, metrics.ribbonHeight);
  button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

QFrame* KaBeginnerRibbon::group(const QString& id) const {
  return m_groups.value(id, nullptr);
}

void KaBeginnerRibbon::setKeepPriority(const QStringList& ids) {
  m_keepPriority = ids;
  updateOverflow();
}

void KaBeginnerRibbon::setPinned(const QStringList& ids) {
  m_pinned = ids;
  updateOverflow();
}

QList<QToolButton*> KaBeginnerRibbon::tabButtons() const {
  QList<QToolButton*> out;
  for (const QString& id : m_groupOrder) {
    QHBoxLayout* row = m_btnRows.value(id);
    if (!row) continue;
    for (int i = 0; i < row->count(); ++i) {
      QLayoutItem* item = row->itemAt(i);
      auto* button = item ? qobject_cast<QToolButton*>(item->widget()) : nullptr;
      if (button && button->focusPolicy() != Qt::NoFocus) out.append(button);
    }
  }
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
  }
  return QWidget::eventFilter(watched, event);
}

QSize KaBeginnerRibbon::sizeHint() const {
  const auto margins = m_row->contentsMargins();
  int width = margins.left() + margins.right();
  int height = 0;
  for (const auto& id : m_groupOrder) {
    const QSize size = m_groups.value(id)->sizeHint();
    width += size.width() + m_row->spacing();
    height = std::max(height, size.height());
  }
  return QSize(width, height + margins.top() + margins.bottom());
}

QSize KaBeginnerRibbon::minimumSizeHint() const {
  // 최소 폭에 조사·내보내기·기록을 넣으면 툴바가 찾기 칸을 줄에서 뺀다.
  // 남는 폭은 Expanding 으로 받고, 접기는 updateOverflow 가 한다.
  const auto margins = m_row->contentsMargins();
  const int width = margins.left() + margins.right() + m_overflow->sizeHint().width();
  return QSize(std::max(width, m_overflow->sizeHint().width() + 16), sizeHint().height());
}

void KaBeginnerRibbon::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  updateOverflow();
}

void KaBeginnerRibbon::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  updateOverflow();
}

QStringList KaBeginnerRibbon::planGroups(const QStringList& priority, const QStringList& pinned,
                                         const QHash<QString, int>& widths, int available,
                                         int overflowWidth) {
  static const QStringList keep = {QStringLiteral("survey"), QStringLiteral("out"), QStringLiteral("record")};
  int total = 0;
  for (const QString& id : priority) total += widths.value(id);
  if (total <= available) return priority;
  // Some group folds, so 「더 많은 작업」 needs room first; a clipped overflow
  // button would hide every folded group.
  int room = available - overflowWidth;
  QStringList visible;
  for (const QString& id : priority) {
    if (widths.value(id) <= room) {
      visible.append(id);
      room -= widths.value(id);
    }
  }
  const auto place = [&](const QString& id, auto&& mayFold) {
    const int need = widths.value(id);
    if (visible.contains(id) || !widths.contains(id)) return;
    int reclaim = 0;
    for (const QString& other : visible)
      if (mayFold(other)) reclaim += widths.value(other);
    // Folding other groups must actually make room; otherwise fold nothing.
    if (need > room + reclaim) return;
    for (int i = int(visible.size()) - 1; i >= 0 && need > room; --i) {
      if (!mayFold(visible.at(i))) continue;
      room += widths.value(visible.at(i));
      visible.removeAt(i);
    }
    visible.append(id);
    room -= need;
  };
  for (const QString& id : priority)
    if (keep.contains(id)) place(id, [&](const QString& other) { return !keep.contains(other); });
  for (const QString& id : pinned)
    place(id, [&](const QString& other) { return !keep.contains(other) && !pinned.contains(other); });
  // Give leftover room back to smaller groups that were folded on the way.
  for (const QString& id : priority) {
    if (!visible.contains(id) && widths.value(id) <= room) {
      visible.append(id);
      room -= widths.value(id);
    }
  }
  QStringList ordered;
  for (const QString& id : priority)
    if (visible.contains(id)) ordered.append(id);
  return ordered;
}

void KaBeginnerRibbon::updateOverflow() {
  if (m_updatingOverflow) return;
  m_updatingOverflow = true;
  const auto margins = m_row->contentsMargins();
  // 남길 묶음은 우선순위대로 고르고, 화면에는 addGroup 순서로 놓는다.
  QStringList priority = m_keepPriority.isEmpty() ? m_groupOrder : m_keepPriority;
  for (const QString& id : m_groupOrder)
    if (!priority.contains(id)) priority.append(id);
  QHash<QString, int> widths;
  for (const QString& id : priority)
    if (m_groups.contains(id)) widths.insert(id, m_groups.value(id)->sizeHint().width() + m_row->spacing());
  priority.removeIf([&](const QString& id) { return !widths.contains(id); });
  const QStringList visible =
      planGroups(priority, m_pinned, widths, width() - margins.left() - margins.right(),
                 m_overflow->sizeHint().width() + m_row->spacing());
  const bool overflow = visible.size() < priority.size();
  // One session-log line per fold change: which groups folded and the widths behind it,
  // so a 1920 screen that folds 정합·기타 can be measured instead of guessed.
  if (overflow && width() > 200) {  // skip the pre-layout pass (width 100)
    QStringList parts;
    int need = 0;
    for (const QString& id : priority) {
      parts << QStringLiteral("%1=%2").arg(id).arg(widths.value(id));
      need += widths.value(id);
    }
    const QString line = QStringLiteral("[ribbon] 접힘 · 창 %1 · 가용 %2 · 필요 %3 · 더보기 %4 · 표시 [%5] · %6")
        .arg(width()).arg(width() - margins.left() - margins.right()).arg(need)
        .arg(m_overflow->sizeHint().width() + m_row->spacing())
        .arg(visible.join(QLatin1Char(' ')), parts.join(QLatin1Char(' ')));
    if (line != m_lastFoldLog) {
      m_lastFoldLog = line;
      qWarning().noquote() << line;  // the app's message handler files it under [qt/warn]; tests just print it
    }
  }
  for (const auto& id : m_groupOrder) {
    auto* frame = m_groups.value(id);
    auto* scroll = m_groupScrolls.value(id);
    const bool onRibbon = visible.contains(id);
    if (onRibbon) {
      if (scroll->widget()) {
        scroll->takeWidget(); // QWidgetAction continues to own only the empty scroll host.
        frame->setParent(this);
      }
      m_row->removeWidget(frame);
      m_row->insertWidget(groupInsertIndex(), frame, 0, Qt::AlignLeft | Qt::AlignVCenter);
      frame->show();
    } else {
      m_row->removeWidget(frame);
      frame->layout()->activate();
      const QSize content = frame->sizeHint().expandedTo(frame->minimumSizeHint()).expandedTo(QSize(120, 80));
      if (scroll->widget() != frame) scroll->setWidget(frame);
      // A resizable scroll area shrinks the group, then its own bars cover the
      // last chip row. Keep the group's real size and scroll only past the screen.
      scroll->setWidgetResizable(false);
      scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
      scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
      QSize room(1600, 900);
      if (QScreen* display = screen())
        room = display->availableGeometry().size() - QSize(64, 140);
      QSize view = content;
      if (view.width() > room.width()) {
        view.setWidth(std::max(120, room.width()));
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
      }
      if (view.height() > room.height()) {
        view.setHeight(std::max(80, room.height()));
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
      }
      if (scroll->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOn)
        view.rwidth() += scroll->verticalScrollBar()->sizeHint().width();
      if (scroll->horizontalScrollBarPolicy() == Qt::ScrollBarAlwaysOn)
        view.rheight() += scroll->horizontalScrollBar()->sizeHint().height();
      scroll->setFixedSize(view);
      frame->resize(content);
      frame->show();
    }
    m_groupMenus.value(id)->menuAction()->setVisible(!onRibbon);
  }
  m_overflow->setVisible(overflow);
  m_updatingOverflow = false;
}
