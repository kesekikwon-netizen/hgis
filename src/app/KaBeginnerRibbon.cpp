#include "KaBeginnerRibbon.h"
#include "KaTheme.h"

#include <algorithm>

#include <QAction>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollArea>
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
  m_row->setContentsMargins(metrics.buttonSpacing, metrics.buttonPadding,
                           metrics.buttonSpacing, metrics.buttonPadding);
  m_row->setSpacing(metrics.buttonSpacing);
  m_row->setSizeConstraint(QLayout::SetNoConstraint);
  m_overflow = new QToolButton(this);
  m_overflow->setObjectName(QStringLiteral("ribbonOverflow"));
  m_overflow->setText(QStringLiteral("더 많은 작업"));
  m_overflow->setFocusPolicy(Qt::TabFocus);
  m_overflow->setPopupMode(QToolButton::InstantPopup);
  m_overflowMenu = new QMenu(m_overflow);
  m_overflowMenu->setObjectName(QStringLiteral("ribbonOverflowMenu"));
  m_overflow->setMenu(m_overflowMenu);
  m_row->addWidget(m_overflow);
  m_overflow->hide();
}

QFrame* KaBeginnerRibbon::addGroup(const QString& id, const QString& caption) {
  if (m_groups.contains(id))
    return m_groups.value(id);
  auto* fr = new QFrame(this);
  fr->setObjectName(QStringLiteral("ribbonGroup"));
  auto* vl = new QVBoxLayout(fr);
  const auto& metrics = KaTheme::buttonMetrics();
  vl->setContentsMargins(metrics.buttonSpacing, metrics.buttonPadding,
                        metrics.buttonSpacing, metrics.buttonPadding);
  vl->setSpacing(metrics.buttonPadding);
  auto* cap = new QLabel(caption, fr);
  cap->setObjectName(QStringLiteral("ribbonGroupCaption"));
  cap->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  cap->setWordWrap(false);
  auto* btns = new QHBoxLayout();
  btns->setContentsMargins(0, 0, 0, 0);
  btns->setSpacing(metrics.buttonSpacing);
  vl->addWidget(cap);
  vl->addLayout(btns, 1);
  m_row->insertWidget(m_row->count() - 1, fr, 0);
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
  row->addWidget(b);
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
    connect(b, &QToolButton::clicked, m_overflowMenu, &QMenu::close);
  }
  row->addWidget(widget);
}

QString KaBeginnerRibbon::twoLine(const QString& text) {
  const QString t = text.trimmed();
  if (t.isEmpty() || t.contains(QLatin1Char('\n')))
    return t;
  if (t.contains(QLatin1Char('('))) {
    const int paren = t.indexOf(QLatin1Char('('));
    if (paren > 0)
      return t.left(paren).trimmed() + QLatin1Char('\n') + t.mid(paren).trimmed();
  }
  if (t.size() <= 4)
    return t;
  int cut = t.lastIndexOf(QChar::Space, t.size() / 2 + 2);
  if (cut < 1)
    cut = t.indexOf(QChar::Space);
  if (cut < 1)
    cut = t.size() / 2;
  const QString a = t.left(cut).trimmed();
  const QString b = t.mid(cut).trimmed();
  if (a.isEmpty() || b.isEmpty())
    return t;
  return a + QLatin1Char('\n') + b;
}

void KaBeginnerRibbon::applyTwoLine(QToolButton* button) {
  if (!button)
    return;
  const auto& metrics = KaTheme::buttonMetrics();
  button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  button->setAutoRaise(false);
  button->setFocusPolicy(Qt::TabFocus);
  button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  button->setIconSize(QSize(metrics.ribbonIconSize, metrics.ribbonIconSize));
  QFont font = button->font();
  font.setPixelSize(metrics.ribbonFontSize);
  button->setFont(font);
  button->ensurePolished();
  const QFontMetrics fm(button->font());
  int textW = 0;
  const QString text = button->text();
  const QStringList lines = text.split(QLatin1Char('\n'));
  for (const QString& line : lines) {
    textW = std::max(textW, fm.horizontalAdvance(line));
  }
  const int contentWidth = std::max(textW, metrics.ribbonIconSize);
  const int w = std::max(metrics.ribbonMinWidth, contentWidth + 2 * metrics.buttonPadding + 2);
  button->setMinimumWidth(w);
  // Reserve two label lines for every button, including single-line labels.
  const int contentHeight = metrics.ribbonIconSize + 2 * fm.lineSpacing() +
                            2 * metrics.buttonPadding + 3 + metrics.buttonSpacing;
  button->setFixedHeight(std::max(metrics.ribbonHeight, contentHeight));
}

QFrame* KaBeginnerRibbon::group(const QString& id) const {
  return m_groups.value(id, nullptr);
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
  return QSize(m_overflow->sizeHint().width() + 16, sizeHint().height());
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
  QStringList priority;
  for (const auto& id : {QStringLiteral("survey"), QStringLiteral("record"), QStringLiteral("out")})
    if (m_groups.contains(id)) priority.append(id);
  for (const auto& id : m_groupOrder)
    if (!priority.contains(id)) priority.append(id);
  QStringList visible;
  for (const auto& id : priority) {
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
      m_row->insertWidget(m_row->count() - 1, frame);
      frame->show();
    } else {
      m_row->removeWidget(frame);
      if (scroll->widget() != frame) scroll->setWidget(frame);
      const QSize room = screen()->availableGeometry().size() - QSize(48, 100);
      scroll->setFixedSize(frame->sizeHint().expandedTo(QSize(120, 80)).boundedTo(room) + QSize(20, 20));
      frame->show();
    }
    m_groupMenus.value(id)->menuAction()->setVisible(!onRibbon);
  }
  m_overflow->setVisible(overflow);
  m_updatingOverflow = false;
}
