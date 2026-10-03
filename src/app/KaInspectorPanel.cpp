#include "KaInspectorPanel.h"

#include "KaChip.h"
#include "KaFeatureCard.h"
#include "KaIcons.h"
#include "KaInspectorStyle.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr int kWideWindow = 1500;

QLabel* sentenceLabel(const QString& text, QWidget* parent) {
  auto* label = new QLabel(text, parent);
  label->setObjectName(QStringLiteral("inspectorSentence"));
  label->setWordWrap(true);
  label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  return label;
}
}  // namespace

int KaInspectorPanel::preferredWidth(int totalWidth) { return totalWidth < kWideWindow ? 272 : 300; }

KaInspectorPanel::KaInspectorPanel(QWidget* parent) : QFrame(parent) {
  setObjectName(QStringLiteral("inspectorPanel"));
  setMinimumWidth(kMinimumWidth);
  setMaximumWidth(kMaximumWidth);
  auto* column = new QVBoxLayout(this);
  column->setContentsMargins(12, 12, 12, 12);
  column->setSpacing(8);

  auto* head = new QHBoxLayout;
  head->setSpacing(8);
  m_title = new QLabel(QStringLiteral("선택한 유구"), this);
  m_title->setObjectName(QStringLiteral("inspectorTitle"));
  QFont titleFont = m_title->font();
  titleFont.setPixelSize(14);
  titleFont.setBold(true);
  m_title->setFont(titleFont);
  m_countChip = new KaChip(QString(), KaChip::Tone::Neutral, this);
  m_countChip->setObjectName(QStringLiteral("inspectorCount"));
  m_countChip->hide();
  m_collapse = new QToolButton(this);
  m_collapse->setObjectName(QStringLiteral("inspectorCollapse"));
  m_collapse->setAutoRaise(true);
  m_collapse->setIcon(KaIcons::icon(QStringLiteral("chevron_right")));
  m_collapse->setIconSize(QSize(16, 16));
  m_collapse->setToolTip(QStringLiteral("패널 접기 (F10)"));
  connect(m_collapse, &QToolButton::clicked, this, &KaInspectorPanel::collapseRequested);
  head->addWidget(m_title, 1);
  head->addWidget(m_countChip, 0, Qt::AlignVCenter);
  head->addWidget(m_collapse, 0, Qt::AlignVCenter);
  column->addLayout(head);

  m_tabs = new QTabBar(this);
  m_tabs->setObjectName(QStringLiteral("inspectorTabs"));
  m_tabs->setDocumentMode(true);
  m_tabs->setExpanding(true);
  m_tabs->setDrawBase(false);
  m_tabs->setIconSize(QSize(16, 16));
  m_tabs->addTab(KaIcons::icon(QStringLiteral("list")), QStringLiteral("속성"));
  m_tabs->addTab(KaIcons::icon(QStringLiteral("brush")), QStringLiteral("스타일"));
  column->addWidget(m_tabs);

  m_stack = new QStackedWidget(this);
  // 속성: sentence, then the card (a scroll area keeps a long record inside 560 px).
  auto* scroll = new QScrollArea(m_stack);
  scroll->setObjectName(QStringLiteral("inspectorAttributeScroll"));
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setWidgetResizable(true);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_attributePage = new QWidget(scroll);
  m_attributePage->setObjectName(QStringLiteral("inspectorAttributePage"));
  auto* attribute = new QVBoxLayout(m_attributePage);
  attribute->setContentsMargins(0, 0, 0, 0);
  attribute->setSpacing(8);
  m_sentence = sentenceLabel(QString(), m_attributePage);
  attribute->addWidget(m_sentence);
  m_cardSlot = new QVBoxLayout;
  m_cardSlot->setContentsMargins(0, 0, 0, 0);
  m_cardSlot->setSpacing(8);
  attribute->addLayout(m_cardSlot);
  attribute->addStretch(1);
  scroll->setWidget(m_attributePage);
  m_stack->addWidget(scroll);
  // 스타일: the chosen layer's colour and width, changed right here.
  m_style = new KaInspectorStyle(m_stack);
  m_stack->addWidget(m_style);
  connect(m_tabs, &QTabBar::currentChanged, m_stack, &QStackedWidget::setCurrentIndex);
  column->addWidget(m_stack, 1);

  m_footer = new QVBoxLayout;
  m_footer->setContentsMargins(0, 0, 0, 0);
  m_footer->setSpacing(8);
  column->addLayout(m_footer);
  syncAttributeState();
}

QWidget* KaInspectorPanel::page(int index) const {
  QWidget* widget = m_stack->widget(index);
  if (auto* scroll = qobject_cast<QScrollArea*>(widget)) return scroll->widget();
  return widget;
}

void KaInspectorPanel::setFeatureCard(KaFeatureCard* card) {
  if (m_card && m_card != card) disconnect(m_card, nullptr, this, nullptr);
  m_card = card;
  if (!card) {
    syncAttributeState();
    return;
  }
  if (card->parentWidget() != m_attributePage) card->setParent(m_attributePage);
  if (m_cardSlot->indexOf(card) < 0) m_cardSlot->addWidget(card);
  card->setTitleVisible(false);  // the panel header carries the title
  connect(card, &KaFeatureCard::featureChanged, this, &KaInspectorPanel::syncAttributeState);
  syncAttributeState();
}

void KaInspectorPanel::setDrawing(bool drawing) {
  if (m_drawing == drawing) return;
  m_drawing = drawing;
  syncAttributeState();
}

void KaInspectorPanel::setSelectionCount(int count) {
  m_count = qMax(0, count);
  m_countChip->setText(QStringLiteral("%1개 선택").arg(m_count));
  m_countChip->setVisible(m_count > 0);
}

QString KaInspectorPanel::title() const { return m_title->text(); }

QString KaInspectorPanel::sentence() const { return m_sentence->isHidden() ? QString() : m_sentence->text(); }

void KaInspectorPanel::syncAttributeState() {
  const bool has = m_card && m_card->hasFeature();
  if (m_card) m_card->setVisible(has);
  QString text;
  if (m_drawing) text = QStringLiteral("그리는 동안에는 기록을 고치지 않습니다. 도형을 마치면 여기서 이어집니다.");
  else if (!has) text = QStringLiteral("지도에서 도형 하나를 고르면 기록이 여기에 나옵니다.");
  m_sentence->setText(text);
  m_sentence->setVisible(!text.isEmpty());
  m_title->setText(has ? m_card->rowText(QStringLiteral("title")) : QStringLiteral("선택한 유구"));
}
