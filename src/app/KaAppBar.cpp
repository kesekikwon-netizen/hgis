#include "KaAppBar.h"

#include "KaIcons.h"
#include "KaSplashArt.h"
#include "KaTheme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QMenu>
#include <QPainter>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QStyle>
#include <QToolButton>
#include <QWidgetAction>

namespace {

QLabel* label(const QString& text, const char* name, QWidget* parent) {
  auto* l = new QLabel(text, parent);
  l->setObjectName(QString::fromLatin1(name));
  return l;
}

}  // namespace

KaAppBar::KaAppBar(QWidget* parent) : QWidget(parent) {
  setObjectName(QStringLiteral("appBar"));
  setFixedHeight(48);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  auto* row = new QHBoxLayout(this);
  row->setContentsMargins(14, 0, 14, 0);
  row->setSpacing(10);

  auto* icon = new QLabel(this);
  icon->setObjectName(QStringLiteral("appBarIcon"));
  icon->setPixmap(KaIcons::appIcon().pixmap(QSize(24, 24)));
  row->addWidget(icon);
  row->addWidget(label(QStringLiteral("Strata"), "appBarName", this));
  auto* divider = new QFrame(this);
  divider->setObjectName(QStringLiteral("appBarDivider"));
  divider->setFixedSize(1, 22);
  row->addWidget(divider);
  row->addWidget(label(QStringLiteral("필드고고학 GIS"), "appBarProduct", this));
  row->addSpacing(14);
  m_survey = label(QString(), "appBarSurvey", this);
  m_state = label(QString(), "appBarState", this);
  row->addWidget(m_survey);
  row->addWidget(m_state);
  row->addStretch(1);

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
  m_search->setMinimumWidth(280);
  m_search->setMaximumWidth(460);
  m_search->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  connect(m_search, &QLineEdit::returnPressed, this, [this]() {
    const QString query = m_search->text().simplified();
    if (!query.isEmpty()) emit searchRequested(query, looksLikeLot(query));
  });
  row->addWidget(m_search, 2);
  row->addStretch(1);

  auto* about = new QToolButton(this);
  about->setObjectName(QStringLiteral("appBarAbout"));
  about->setText(QStringLiteral("정보"));
  about->setToolButtonStyle(Qt::ToolButtonTextOnly);
  connect(about, &QToolButton::clicked, this, &KaAppBar::aboutRequested);
  row->addWidget(about);
  setSurvey(QString(), false);
}

void KaAppBar::setSurvey(const QString& name, bool unsaved) {
  m_survey->setText(name);
  m_survey->setVisible(!name.isEmpty());
  m_state->setText(unsaved ? QStringLiteral("● 저장 안 됨") : QStringLiteral("저장됨"));
  m_state->setProperty("unsaved", unsaved);
  m_state->setVisible(!name.isEmpty());
  m_state->style()->unpolish(m_state);
  m_state->style()->polish(m_state);
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

void KaAppBar::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  m_texture = QPixmap();
}

void KaAppBar::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  const QRectF band(rect());
  const auto& tokens = KaTheme::tokens();
  QLinearGradient navy(band.topLeft(), band.topRight());
  navy.setColorAt(0.0, tokens.rail);
  navy.setColorAt(0.6, tokens.sky3);
  navy.setColorAt(1.0, tokens.rail);
  painter.fillRect(band, navy);
  const qreal dpr = devicePixelRatioF();
  if (m_texture.isNull() || !qFuzzyCompare(m_texture.devicePixelRatioF(), dpr)) {
    // The same mound as the startup notice, faint behind the right-hand side.
    const QPointF summit(band.width() * 0.88, band.height() * 0.3);
    m_texture = QPixmap::fromImage(KaSplashArt::contours(band.size(), dpr, summit, 300.0, 0.0));
  }
  painter.setOpacity(0.8);
  painter.drawPixmap(0, 0, m_texture);
}
