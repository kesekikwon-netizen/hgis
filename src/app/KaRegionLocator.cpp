#include "KaRegionLocator.h"
#include "core/KoreaRegionCatalog.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QEvent>
#include <QKeyEvent>
#include <QComboBox>
#include <QFont>
#include <QApplication>
#include <QMenu>
#include <QTimer>
#include <QScreen>
#include <QSignalBlocker>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

namespace {
QWidget* hostOutsideMenu(QWidget* widget) {
  QWidget* host = widget ? widget->window() : nullptr;
  while (host && (qobject_cast<QMenu*>(host) ||
                  host->windowFlags().testFlag(Qt::WindowType::Popup))) {
    QWidget* parent = host->parentWidget();
    if (!parent) break;
    host = parent->window();
  }
  return host ? host : widget;
}
}

KaRegionLocator::KaRegionLocator(QWidget* parent) : QWidget(parent) {
  setObjectName(QStringLiteral("regionLocator"));
  setMinimumWidth(204);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
  setToolTip(QStringLiteral("시·도를 누르면 시·동·번지로 찾습니다"));

  auto* grid = new QGridLayout(this);
  grid->setContentsMargins(4, 4, 4, 4);
  grid->setHorizontalSpacing(4);
  grid->setVerticalSpacing(4);
  m_group = new QButtonGroup(this);
  // 배타 그룹이면 한 번 누른 칩을 놓을 수가 없어 취소가 막힌다.
  // 규칙 하나로 통일: 팝업 열림 == 칩 눌림. 같은 칩을 다시 누르면 닫고 해제.
  m_group->setExclusive(false);

  // 칸은 겹치지 않는 격자 그대로 두고, 자리만 한반도 모양으로 놓는다(타일 지도).
  // 서울이 맨 위, 서해안은 왼쪽 열, 동해안은 오른쪽 열, 제주가 맨 아래다.
  // 예전에는 한 줄 여섯 칸에 흘려 넣어 지도 모양도 행정 순서도 아니었다.
  struct Tile { const char* name; int row; int col; };
  const Tile tiles[] = {
      {"서울", 0, 1},
      {"인천", 1, 0}, {"경기", 1, 1}, {"강원", 1, 2},
      {"충남", 2, 0}, {"세종", 2, 1}, {"충북", 2, 2}, {"경북", 2, 3},
      {"전북", 3, 0}, {"대전", 3, 1}, {"대구", 3, 2}, {"울산", 3, 3},
      {"광주", 4, 0}, {"전남", 4, 1}, {"경남", 4, 2}, {"부산", 4, 3},
      {"제주", 5, 1},
  };
  QStringList labels;
  for (const Tile& tile : tiles) labels.append(QString::fromUtf8(tile.name));
  for (int i = 0; i < labels.size(); ++i) {
    auto* b = new QToolButton(this);
    b->setObjectName(QStringLiteral("regionChip"));
    b->setText(labels.at(i));
    b->setCheckable(true);
    b->setToolButtonStyle(Qt::ToolButtonTextOnly);
    b->setAutoRaise(false);
    b->setFixedHeight(28);
    b->setMinimumWidth(42);
    b->setMaximumWidth(46);
    b->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    b->setCursor(Qt::PointingHandCursor);
    m_group->addButton(b, i);
    grid->addWidget(b, tiles[i].row, tiles[i].col);
    const QString sido = KoreaRegionCatalog::canonicalSido(labels.at(i));
    connect(b, &QToolButton::clicked, this, [this, b, sido]() {
      if (m_activeChip == b && m_popup && m_popup->isVisible()) {
        closePanel();
        return;
      }
      for (QAbstractButton* other : m_group->buttons())
        if (other != b) other->setChecked(false);
      b->setChecked(true);
      m_activeChip = b;
      openAddressPopup(sido);
      emit regionSelected(sido);
    });
  }
}

QSize KaRegionLocator::sizeHint() const {
  // 4열 6행 타일 지도: 칩 46×28, 간격 4, 여백 4.
  return layout() ? layout()->sizeHint().expandedTo(QSize(204, 196)) : QSize(204, 196);
}

void KaRegionLocator::closePanel() {
  if (m_popup) m_popup->hide();
  if (m_activeChip) m_activeChip->setChecked(false);
  m_activeChip = nullptr;
}

// Esc로 닫고, 팝업이 포커스를 잃어도(다른 창·지도를 누름) 닫는다.
bool KaRegionLocator::eventFilter(QObject* watched, QEvent* event) {
  if (m_popup && watched == m_popup) {
    if (event->type() == QEvent::KeyPress) {
      if (static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
        closePanel();
        return true;
      }
    } else if (event->type() == QEvent::WindowDeactivate) {
      if (m_suppressDeactivate) return false;
      closePanel();
    }
  }
  return QWidget::eventFilter(watched, event);
}

void KaRegionLocator::openAddressPopup(const QString& sido) {
  m_sido = sido;
  const bool fromMenu = qobject_cast<QMenu*>(window()) != nullptr;
  QWidget* host = hostOutsideMenu(this);
  if (!m_popup) {
    m_popup = new QFrame(host, Qt::Tool | Qt::FramelessWindowHint);
    m_popup->setObjectName(QStringLiteral("regionAddressPopup"));
    // A transient grab window steals the Windows IME; Tool is a real window so Hangul composes.
    m_popup->setAttribute(Qt::WA_InputMethodEnabled, true);
    m_popup->setAutoFillBackground(true);
    auto* col = new QVBoxLayout(m_popup);
    col->setContentsMargins(12, 10, 12, 10);
    col->setSpacing(8);
    m_sidoLabel = new QLabel(m_popup);
    m_sidoLabel->setObjectName(QStringLiteral("regionSidoTitle"));
    auto* row = new QGridLayout();
    m_addressLayout = row;
    row->setSpacing(6);
    const QFont hangul(QStringLiteral("Malgun Gothic"), 10);
    m_city = new QComboBox(m_popup);
    m_city->setMinimumWidth(120);
    m_city->setFont(hangul);
    m_dong = new QComboBox(m_popup);
    m_dong->setObjectName(QStringLiteral("regionDong"));
    m_dong->setEditable(true);
    m_dong->setInsertPolicy(QComboBox::NoInsert);
    m_dong->setCompleter(nullptr);
    m_dong->setMinimumWidth(110);
    m_dong->setFont(hangul);
    m_dong->setAttribute(Qt::WA_InputMethodEnabled, true);
    m_dong->setInputMethodHints(Qt::ImhNone);
    if (QLineEdit* dongEdit = m_dong->lineEdit()) {
      dongEdit->setPlaceholderText(QStringLiteral("법정동·읍·면·리"));
      dongEdit->setAttribute(Qt::WA_InputMethodEnabled, true);
      dongEdit->setInputMethodHints(Qt::ImhNone);
      dongEdit->setFont(hangul);
    }
    m_lot = new QLineEdit(m_popup);
    m_lot->setPlaceholderText(QStringLiteral("산 12-3"));
    m_lot->setToolTip(QStringLiteral("번지 (예: 12, 12-3, 산 12-3). 산번지는 ‘산’을 함께 입력하세요."));
    m_lot->setMaximumWidth(120);
    m_lot->setFont(hangul);
    m_lot->setAttribute(Qt::WA_InputMethodEnabled, true);
    m_lot->setInputMethodHints(Qt::ImhNone);
    auto* go = new QPushButton(QStringLiteral("찾기"), m_popup);
    go->setDefault(true);
    // 잘못 눌렀을 때 빠져나갈 길. Esc·바깥 클릭·같은 칩 다시 누르기와 같은 동작.
    auto* cancel = new QPushButton(QStringLiteral("취소"), m_popup);
    cancel->setObjectName(QStringLiteral("regionCancel"));
    cancel->setAutoDefault(false);
    m_addressControls = {m_city, m_dong, m_lot, go, cancel};
    row->addWidget(m_city, 0, 0);
    row->addWidget(m_dong, 0, 1);
    row->addWidget(m_lot, 0, 2);
    row->addWidget(go, 0, 3);
    row->addWidget(cancel, 0, 4);
    auto* titleRow = new QHBoxLayout();
    titleRow->setSpacing(6);
    titleRow->addWidget(m_sidoLabel, 1);
    auto* closeX = new QToolButton(m_popup);
    closeX->setObjectName(QStringLiteral("regionClose"));
    closeX->setText(QStringLiteral("✕"));
    closeX->setToolTip(QStringLiteral("닫기 (Esc)"));
    closeX->setCursor(Qt::PointingHandCursor);
    closeX->setAutoRaise(true);
    titleRow->addWidget(closeX, 0);
    col->addLayout(titleRow);
    col->addLayout(row);
    auto* hint = new QLabel(QStringLiteral("번지 입력 시 정확한 지번만 찾습니다. 읍·면은 리까지 입력하세요.\n예: 고아읍 봉한리 / 산 12-3"), m_popup);
    hint->setWordWrap(true);
    col->addWidget(hint);
    m_popup->installEventFilter(this);
    connect(closeX, &QToolButton::clicked, this, &KaRegionLocator::closePanel);
    connect(cancel, &QPushButton::clicked, this, &KaRegionLocator::closePanel);
    connect(go, &QPushButton::clicked, this, &KaRegionLocator::emitSearch);
    connect(m_lot, &QLineEdit::returnPressed, this, &KaRegionLocator::emitSearch);
    connect(m_dong->lineEdit(), &QLineEdit::returnPressed, this, &KaRegionLocator::emitSearch);
    connect(m_city, &QComboBox::currentIndexChanged, this, &KaRegionLocator::fillDongs);
  } else if (host && m_popup->parentWidget() != host) {
    m_popup->setParent(host, Qt::Tool | Qt::FramelessWindowHint);
  }
  m_sidoLabel->setText(sido);
  const QSignalBlocker block(m_city);
  m_city->clear();
  m_city->addItem(QStringLiteral("시·군·구"));
  for (const QString& c : KoreaRegionCatalog::citiesOf(sido)) m_city->addItem(c);
  fillDongs();
  m_lot->clear();
  const QRect anchor(mapToGlobal(QPoint(0, 0)), size());
  const QPoint screenPoint = m_activeChip
      ? m_activeChip->mapToGlobal(m_activeChip->rect().center()) : anchor.center();
  QScreen* popupScreen = QGuiApplication::screenAt(screenPoint);
  if (!popupScreen) popupScreen = screen();
  if (popupScreen) placeAddressPopup(anchor, popupScreen->availableGeometry());
  m_popup->show();
  m_popup->raise();
  if (fromMenu) {
    // The overflow menu is a popup. Leaving it open grabs the mouse and, on
    // deactivate, hides a child tool window. Close it after the popup has a
    // host outside the menu.
    m_suppressDeactivate = true;
    while (QWidget* popup = QApplication::activePopupWidget())
      popup->close();
    QTimer::singleShot(0, this, [this] {
      m_suppressDeactivate = false;
      if (!m_popup || !m_popup->isVisible()) return;
      m_popup->activateWindow();
      if (m_dong) m_dong->setFocus();
    });
  } else {
    m_popup->activateWindow();
    m_dong->setFocus();
  }
}

void KaRegionLocator::placeAddressPopup(const QRect& anchor, const QRect& available) {
  if (!m_popup || !m_addressLayout || available.isEmpty()) return;
  // Qt widget/global/screen geometries all use logical pixels. Do not multiply
  // by DPR: that pushes tools off-screen on mixed-DPI and negative-origin screens.
  const QRect bounds = available.adjusted(4, 4, -4, -4);
  for (int columns : {6, 3, 2, 1}) {
    for (QWidget* control : m_addressControls) m_addressLayout->removeWidget(control);
    for (int i = 0; i < m_addressControls.size(); ++i)
      m_addressLayout->addWidget(m_addressControls.at(i), i / columns, i % columns);
    m_addressLayout->invalidate();
    m_popup->layout()->activate();
    if (m_popup->minimumSizeHint().width() <= bounds.width()) break;
  }
  m_popup->resize(m_popup->sizeHint().boundedTo(bounds.size()));
  const QSize popupSize = m_popup->size();
  int y = anchor.bottom() + 1;
  if (y + popupSize.height() > bounds.bottom() + 1)
    y = anchor.top() - popupSize.height();
  const int x = std::clamp(anchor.left(), bounds.left(),
                           std::max(bounds.left(), bounds.right() + 1 - popupSize.width()));
  y = std::clamp(y, bounds.top(),
                 std::max(bounds.top(), bounds.bottom() + 1 - popupSize.height()));
  m_popup->move(x, y);
}

void KaRegionLocator::fillDongs() {
  if (!m_dong || !m_city) return;
  const QString city = m_city->currentIndex() > 0 ? m_city->currentText() : QString();
  m_dong->clear();
  m_dong->addItem(QStringLiteral("동·읍·면"));
  for (const QString& d : KoreaRegionCatalog::dongsOf(m_sido, city)) m_dong->addItem(d);
  if (m_dong->lineEdit()) m_dong->lineEdit()->clear();
}

void KaRegionLocator::emitSearch() {
  const QString city = (m_city && m_city->currentIndex() > 0) ? m_city->currentText() : QString();
  QString dong;
  if (m_dong) {
    dong = m_dong->currentText().trimmed();
    if (dong == QStringLiteral("동·읍·면")) dong.clear();
  }
  const QString q = KoreaRegionCatalog::composeAddress(
      m_sido, city, dong, m_lot ? m_lot->text() : QString());
  if (q.isEmpty()) return;
  closePanel();
  if (m_lot && !m_lot->text().trimmed().isEmpty()) emit parcelSearchRequested(q);
  else emit searchRequested(q);
}
