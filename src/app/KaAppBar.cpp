#include "KaAppBar.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QMenu>
#include <QRegularExpression>
#include <QToolButton>
#include <QWidgetAction>

KaAppBar::KaAppBar(QWidget* parent) : QWidget(parent) {
  setObjectName(QStringLiteral("appBar"));
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  auto* row = new QHBoxLayout(this);
  row->setContentsMargins(12, 0, 8, 0);
  row->setSpacing(8);

  m_region = new QToolButton(this);
  m_region->setObjectName(QStringLiteral("appBarRegion"));
  m_region->setText(QStringLiteral("지역"));
  m_region->setToolTip(QStringLiteral("시·도를 고른 뒤 시·군·동과 지번으로 찾아갑니다"));
  m_region->setPopupMode(QToolButton::InstantPopup);
  m_region->setToolButtonStyle(Qt::ToolButtonTextOnly);
  m_region->hide();  // shown once the province chips are handed over
  row->addWidget(m_region);

  m_search = new QLineEdit(this);
  m_search->setObjectName(QStringLiteral("appBarSearch"));
  m_search->setPlaceholderText(QStringLiteral("주소·지번으로 찾기 (예: 제주시 애월읍 광령리 1615)  Ctrl+F"));
  m_search->setClearButtonEnabled(true);
  m_search->setMinimumWidth(320);
  m_search->setMaximumWidth(460);
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
  static const QRegularExpression lot(QStringLiteral(R"(^산?\d+(-\d+)?(번지)?$)"));
  const QStringList words = query.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
  return words.size() >= 2 && lot.match(words.last()).hasMatch();
}
