#include "KaAppBar.h"
#include "KaIcons.h"
#include "KaTheme.h"
#include "core/AddressQuery.h"

#include <QAction>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMenu>
#include <QRegularExpression>
#include <QSizePolicy>
#include <QToolButton>
#include <QWidgetAction>

KaAppBar::KaAppBar(QWidget* parent) : QWidget(parent) {
  setObjectName(QStringLiteral("appBar"));
  // 리본이 남는 폭을 가져가야 1280 에서 조사·기록·내보내기가 남는다.
  // Maximum 은 sizeHint 를 넘기지 않는다. https://doc.qt.io/qt-6/qsizepolicy.html
  setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  auto* row = new QHBoxLayout(this);
  row->setContentsMargins(8, 0, 8, 0);  // 12 -> 8: part of the 1920 ribbon budget (see the search field below)
  row->setSpacing(8);

  m_region = new QToolButton(this);
  m_region->setObjectName(QStringLiteral("appBarRegion"));
  m_region->setText(QStringLiteral("지역"));
  m_region->setToolTip(QStringLiteral("시·도를 고른 뒤 시·군·동과 지번으로 찾아갑니다"));
  m_region->setAccessibleName(QStringLiteral("지역 고르기"));
  m_region->setPopupMode(QToolButton::InstantPopup);
  // 목업: 「지역」 앞 지도 핀, 검색칸 앞 돋보기(16px).
  m_region->setIcon(KaIcons::icon(QStringLiteral("gps"), KaTheme::tokens().inkMuted));
  m_region->setIconSize(QSize(16, 16));
  m_region->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  m_region->hide();  // shown once the province chips are handed over
  row->addWidget(m_region);

  m_search = new QLineEdit(this);
  m_search->setObjectName(QStringLiteral("appBarSearch"));
  m_search->setPlaceholderText(QStringLiteral("도로명·지번 (예: 하회종가길 40)  Ctrl+F"));
  m_search->addAction(KaIcons::icon(QStringLiteral("search"), KaTheme::tokens().inkMuted), QLineEdit::LeadingPosition);
  m_search->setClearButtonEnabled(true);
  // The placeholder vanishes once typing starts; the name stays for screen readers.
  m_search->setAccessibleName(QStringLiteral("주소·지번 찾기"));
  // 116 (was 220, then 150): the eight groups need 1641 px (session log "[ribbon] 접힘",
  // 2026-09-30: 27 chips x 60 px outside plus group edges and row spacing). A 1920 window
  // that is not maximised has a 1904 px client, and with IBM Plex the 「지역」 button and
  // this placeholder grew 8 px, so at 150 the ribbon got 1642 - 2 = 1640 and 정합·기타
  // folded by one pixel. 116 plus the 8 px left margin gives ~17 px of slack; the field
  // still expands into any wider window.
  m_search->setMinimumWidth(116);  // 목업의 핀·돋보기(2026-10-03)로 앱 바가 34px 넓어져 1904 에서 리본 타일 52 -> 50
  m_search->setMaximumWidth(320);
  m_search->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  connect(m_search, &QLineEdit::returnPressed, this, [this]() {
    const QString query = m_search->text().simplified();
    if (!query.isEmpty()) emit searchRequested(query, looksLikeLot(query));
  });
  row->addWidget(m_search, 1);
}

void KaAppBar::setRegionWidget(QWidget* widget) {
  if (!widget) return;
  auto* menu = new QMenu(m_region);
  menu->setObjectName(QStringLiteral("appBarRegionMenu"));
  auto* host = new QWidgetAction(menu);
  host->setDefaultWidget(widget);
  menu->addAction(host);
  m_region->setMenu(menu);
  m_region->show();
}

void KaAppBar::focusSearch() {
  m_search->setFocus(Qt::ShortcutFocusReason);
  m_search->selectAll();
}

bool KaAppBar::looksLikeLot(const QString& query) {
  if (kaIsRoadAddress(query)) return false;
  static const QRegularExpression lot(QStringLiteral(R"(^산?\d+(-\d+)?(번지)?$)"));
  const QStringList words = query.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
  return words.size() >= 2 && lot.match(words.last()).hasMatch();
}
