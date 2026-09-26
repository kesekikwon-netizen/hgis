#include "KaBeginnerRibbon.h"
#include "KaTheme.h"

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
  const int textRoom = std::max(8, metrics.ribbonChipWidth - 8);
  while (font.pixelSize() > 9 &&
         QFontMetrics(font).boundingRect(text).width() > textRoom)
    font.setPixelSize(font.pixelSize() - 1);
  button->setFont(font);
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

void KaBeginnerRibbon::updateOverflow() {
  if (m_updatingOverflow) return;
  m_updatingOverflow = true;
  const bool overflow = sizeHint().width() > width();
  const auto margins = m_row->contentsMargins();
  int available = width() - margins.left() - margins.right();
  if (overflow) available -= m_overflow->sizeHint().width() + m_row->spacing();
  // 남길 묶음은 우선순위대로 고르고, 화면에는 addGroup 순서로 놓는다.
  const QStringList priority = m_keepPriority.isEmpty() ? m_groupOrder : m_keepPriority;
  QStringList visible;
  for (const auto& id : priority) {
    if (!m_groups.contains(id)) continue;
    const int needed = m_groups.value(id)->sizeHint().width() + m_row->spacing();
    if (!overflow || needed <= available) {
      visible.append(id);
      available -= needed;
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
