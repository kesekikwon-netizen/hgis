#include "KaStartPage.h"
#include "KaChip.h"
#include "KaHomeConnectionCard.h"
#include "KaHomeGuideCard.h"
#include "KaHomeRecentCard.h"
#include "KaHomeText.h"
#include "KaIcons.h"
#include "KaRecoverySnapshots.h"
#include "KaStartHero.h"
#include "KaTheme.h"
#include "core/RecentSurveys.h"
#include "core/SurveyFacts.h"

#include <QDesktopServices>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSettings>
#include <QShowEvent>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace {

constexpr int kHeroCompact = 184;
constexpr int kHeroTall = 236;
constexpr int kCompactWindowHeight = 820;  // 1366x768 desktops sit under this
constexpr int kRightNarrow = 340;
constexpr int kRightWide = 400;
constexpr int kNarrowWindowWidth = 1500;

QLabel* label(const QString& text, const char* name, QWidget* parent) {
  auto* l = new QLabel(text, parent);
  l->setObjectName(QString::fromLatin1(name));
  return l;
}

QLabel* glyph(const QString& id, int px, const QColor& ink, QWidget* parent) {
  auto* l = new QLabel(parent);
  l->setPixmap(KaIcons::glyphPixmap(id, ink, px, parent->devicePixelRatioF()));
  l->setFixedSize(px, px);
  return l;
}

}  // namespace

KaStartPage::KaStartPage(QWidget* parent) : QWidget(parent) {
  setObjectName(QStringLiteral("startPage"));
  setAttribute(Qt::WA_StyledBackground, true);
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(20, 20, 20, 20);
  root->setSpacing(16);
  root->addWidget(buildHero());
  auto* lower = new QHBoxLayout;
  lower->setSpacing(16);
  m_recentCard = new KaHomeRecentCard(this);
  connect(m_recentCard, &KaHomeRecentCard::openRequested, this, &KaStartPage::recentOpened);
  connect(m_recentCard, &KaHomeRecentCard::forgetRequested, this, &KaStartPage::forgetRequested);
  connect(m_recentCard, &KaHomeRecentCard::recoveryFolderRequested, this, &KaStartPage::openRecoveryFolder);
  connect(m_recentCard, &KaHomeRecentCard::recoveryDismissed, this, &KaStartPage::dismissRecovery);
  lower->addWidget(m_recentCard, 1);
  lower->addWidget(buildRightColumn(), 0);
  root->addLayout(lower, 1);
  applyWindowMetrics();
  reload();
}

int KaStartPage::heroHeightFor(int windowHeight) {
  return windowHeight < kCompactWindowHeight ? kHeroCompact : kHeroTall;
}

int KaStartPage::rightColumnWidthFor(int windowWidth) {
  return windowWidth < kNarrowWindowWidth ? kRightNarrow : kRightWide;
}

QWidget* KaStartPage::buildHero() {
  const auto& t = KaTheme::tokens();
  auto* hero = new KaStartHero(this);
  hero->setFixedHeight(kHeroTall);
  m_hero = hero;
  auto* row = new QHBoxLayout(hero);
  row->setContentsMargins(40, 0, 24, 0);
  auto* text = new QVBoxLayout;
  text->setSpacing(8);
  text->addStretch(1);
  auto* brand = new QHBoxLayout;
  brand->setSpacing(16);
  auto* logo = new QLabel(hero);
  logo->setObjectName(QStringLiteral("startHeroLogo"));
  logo->setPixmap(KaIcons::appIcon().pixmap(QSize(48, 48), hero->devicePixelRatioF()));
  logo->setFixedSize(48, 48);
  brand->addWidget(logo, 0, Qt::AlignVCenter);
  brand->addWidget(label(QStringLiteral("Strata"), "startHeroName", hero));
  auto* divider = new QFrame(hero);
  divider->setObjectName(QStringLiteral("startHeroDivider"));
  divider->setFixedSize(1, 28);
  brand->addWidget(divider, 0, Qt::AlignVCenter);
  brand->addWidget(label(QStringLiteral("필드고고학 GIS"), "startHeroProduct", hero), 0, Qt::AlignVCenter);
  brand->addStretch(1);
  text->addLayout(brand);
  auto* lead = label(QStringLiteral("오늘 현장을 새로 만들거나, 지난 조사를 이어서 여세요."), "startHeroLead", hero);
  lead->setWordWrap(true);  // the band narrows with small field screens
  text->addWidget(lead);
  auto* actions = new QHBoxLayout;
  actions->setSpacing(12);
  auto* btnNew = new QPushButton(QStringLiteral("새 조사"), hero);
  btnNew->setObjectName(QStringLiteral("startNewBtn"));
  btnNew->setIcon(KaIcons::icon(QStringLiteral("new"), t.rail));
  btnNew->setIconSize(QSize(18, 18));
  auto* btnOpen = new QPushButton(QStringLiteral("조사 열기"), hero);
  btnOpen->setObjectName(QStringLiteral("startOpenBtn"));
  btnOpen->setIcon(KaIcons::icon(QStringLiteral("open"), t.railText));
  btnOpen->setIconSize(QSize(18, 18));
  connect(btnNew, &QPushButton::clicked, this, &KaStartPage::newSurveyRequested);
  connect(btnOpen, &QPushButton::clicked, this, &KaStartPage::openRequested);
  actions->addWidget(btnNew);
  actions->addWidget(btnOpen);
  actions->addStretch(1);
  text->addSpacing(8);
  text->addLayout(actions);
  text->addStretch(1);
  row->addLayout(text, 1);

  auto* card = new QFrame(hero);
  card->setObjectName(QStringLiteral("startContinueCard"));
  card->setMinimumWidth(300);
  card->setMaximumWidth(470);
  auto* lines = new QVBoxLayout(card);
  lines->setContentsMargins(20, 14, 20, 14);
  lines->setSpacing(4);
  auto* tagRow = new QHBoxLayout;
  tagRow->setSpacing(8);
  tagRow->addWidget(label(QStringLiteral("이어서 작업"), "startContinueTag", card), 0, Qt::AlignVCenter);
  tagRow->addStretch(1);
  m_badge = new KaChip(QStringLiteral("저장됨"), KaChip::Tone::Ok, card);
  m_badge->setObjectName(QStringLiteral("startContinueBadge"));
  tagRow->addWidget(m_badge, 0, Qt::AlignVCenter);
  lines->addLayout(tagRow);
  m_continueName = label(QString(), "startContinueName", card);
  lines->addWidget(m_continueName);
  auto* pathRow = new QHBoxLayout;
  pathRow->setSpacing(6);
  pathRow->addWidget(glyph(QStringLiteral("folder"), 14, t.inkMuted, card), 0, Qt::AlignVCenter);
  m_continuePath = label(QString(), "startContinuePath", card);
  pathRow->addWidget(m_continuePath, 1, Qt::AlignVCenter);
  lines->addLayout(pathRow);
  auto* whenRow = new QHBoxLayout;
  whenRow->setSpacing(6);
  whenRow->addWidget(glyph(QStringLiteral("clock"), 14, t.inkMuted, card), 0, Qt::AlignVCenter);
  m_continueWhen = label(QString(), "startContinueMeta", card);
  whenRow->addWidget(m_continueWhen, 1, Qt::AlignVCenter);
  lines->addLayout(whenRow);
  // Quiet, click-only trace of a recovery copy (no dialog, no status bar, no auto-open).
  m_recovery = label(QString(), "startRecoveryNote", card);
  m_recovery->setWordWrap(true);
  m_recovery->setTextFormat(Qt::RichText);
  m_recovery->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
  m_recovery->setToolTip(QStringLiteral("사본에는 저장하지 않은 편집이 있던 레이어가 들어 있습니다. "
                                        "원래 조사 파일은 바뀌지 않았습니다."));
  connect(m_recovery, &QLabel::linkActivated, this, &KaStartPage::onRecoveryLink);
  m_recovery->hide();
  lines->addWidget(m_recovery);
  lines->addSpacing(8);
  m_resume = new QPushButton(QStringLiteral("이어서 열기  →"), card);
  m_resume->setObjectName(QStringLiteral("startContinueBtn"));
  connect(m_resume, &QPushButton::clicked, this, [this]() {
    if (!m_continueTarget.isEmpty()) emit recentOpened(m_continueTarget);
  });
  lines->addWidget(m_resume);  // full card width
  m_continue = card;
  row->addWidget(card, 0, Qt::AlignVCenter);
  return hero;
}

QWidget* KaStartPage::buildRightColumn() {
  // 작업 순서 (≈200) + 연결 상태 5행 (≈250) exceed the 320 px left under a compact hero:
  // the whole column scrolls (vertical bar only) instead of squeezing the cards.
  auto* scroll = new QScrollArea(this);
  scroll->setObjectName(QStringLiteral("startRightColumn"));
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scroll->viewport()->setAutoFillBackground(false);
  auto* inner = new QWidget(scroll);
  inner->setObjectName(QStringLiteral("startRightColumnInner"));
  inner->setAutoFillBackground(false);
  auto* col = new QVBoxLayout(inner);
  col->setContentsMargins(0, 0, 0, 0);
  col->setSpacing(16);
  m_guide = new KaHomeGuideCard(inner);
  col->addWidget(m_guide);
  m_connection = new KaHomeConnectionCard(inner);
  connect(m_connection, &KaHomeConnectionCard::configureRequested, this, &KaStartPage::configureRequested);
  col->addWidget(m_connection);
  col->addStretch(1);
  scroll->setWidget(inner);
  scroll->setFixedWidth(kRightNarrow);
  m_rightColumn = scroll;
  return scroll;
}

void KaStartPage::applyWindowMetrics() {
  if (m_hero) m_hero->setFixedHeight(heroHeightFor(window()->height()));
  if (m_rightColumn) m_rightColumn->setFixedWidth(rightColumnWidthFor(window()->width()));
}

void KaStartPage::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  applyWindowMetrics();
}

void KaStartPage::reload() {
  QSettings st = RecentSurveys::userSettings();
  // Offline entries (USB not plugged in) stay listed in grey; they are never dropped here.
  m_items = RecentSurveys::loadAll(st);
  if (m_recentCard) m_recentCard->setItems(m_items, st);
  // 이어서 열기 always points at a survey that can open right now.
  const RecentSurveys::Item* last = nullptr;
  for (const RecentSurveys::Item& it : m_items)
    if (it.available) { last = &it; break; }
  if (m_continue) m_continue->setVisible(last != nullptr);
  m_continueTarget = last ? last->path : QString();
  m_recoverySnapshot = last ? KaRecoverySnapshots::unsavedSnapshotFor(st, last->path) : QString();
  // Read-only record of the continue survey; the steps and 「제출 준비」 follow it.
  if (m_guide) m_guide->setFacts(last ? last->name : QString(), last ? SurveyFacts::lookup(st, last->path) : SurveyFacts::Facts{});
  if (last) {
    m_continueName->setText(last->name);
    m_continuePath->setText(m_continuePath->fontMetrics().elidedText(
        last->path, Qt::ElideMiddle, 260));  // '/' reads as a path; '\' shows as ₩ in Malgun
    m_continuePath->setToolTip(last->path);
    m_continueWhen->setText(QStringLiteral("마지막 열림 %1").arg(KaHomeText::when(last->lastOpenedMs)));
    const bool unsaved = !m_recoverySnapshot.isEmpty();
    m_badge->setText(unsaved ? QStringLiteral("저장 안 됨") : QStringLiteral("저장됨"));
    m_badge->setTone(unsaved ? KaChip::Tone::Warn : KaChip::Tone::Ok);
    m_badge->setGlyph(unsaved ? QStringLiteral("warn") : QStringLiteral("check"));
  }
  if (!m_recovery) return;
  m_recovery->setVisible(!m_recoverySnapshot.isEmpty());
  if (m_recoverySnapshot.isEmpty()) return;
  const qint64 copiedAt = QFileInfo(m_recoverySnapshot).lastModified().toMSecsSinceEpoch();
  m_recovery->setText(QStringLiteral("저장하지 않은 편집의 복구 사본이 있습니다(%1). "
                                     "<a href=\"open\">사본 열기</a> · <a href=\"folder\">폴더 열기</a> · "
                                     "<a href=\"dismiss\">숨기기</a>")
                          .arg(KaHomeText::when(copiedAt)));
}

void KaStartPage::refreshConnections() {
  if (m_connection) QTimer::singleShot(0, m_connection, &KaHomeConnectionCard::refresh);
}

void KaStartPage::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  refreshConnections();  // paint first; the rows, the keyboard focus and the GPKG counts follow next turn
  QTimer::singleShot(0, this, [this]() {
    if (!isVisible()) return;
    if (m_resume && m_continue && m_continue->isVisible()) m_resume->setFocus(Qt::OtherFocusReason);
    probeSurveyFacts();
  });
}

QStringList KaStartPage::probeCandidates(const QVector<RecentSurveys::Item>& items) {
  QStringList paths;
  for (const RecentSurveys::Item& it : items)
    if (it.available && !RecentSurveys::isRemotePath(it.path)) paths << it.path;
  return paths;
}

void KaStartPage::probeSurveyFacts() {
  // Read-side fallback (KA_HGIS_HOME_PROBE=0 turns it off): counts the tables of local GPKG
  // files that have no record yet, never opening a project or a layer.
  if (!SurveyFacts::probeEnabled()) return;
  QSettings st = RecentSurveys::userSettings();
  if (SurveyFacts::probeCountsIfUnknown(st, probeCandidates(m_items)) > 0) reload();
}

void KaStartPage::onRecoveryLink(const QString& link) {
  if (m_recoverySnapshot.isEmpty()) return;
  if (link == QLatin1String("open"))
    emit recentOpened(m_recoverySnapshot);
  else if (link == QLatin1String("folder"))
    openRecoveryFolder(m_recoverySnapshot);
  else if (link == QLatin1String("dismiss"))
    dismissRecovery(m_continueTarget);
}

void KaStartPage::openRecoveryFolder(const QString& snapshotPath) {
  QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(snapshotPath).absolutePath()));
}

void KaStartPage::dismissRecovery(const QString& surveyPath) {
  // Only the home-page note goes away; the copy files stay in the folder.
  QSettings st = RecentSurveys::userSettings();
  KaRecoverySnapshots::forgetUnsaved(st, surveyPath);
  reload();
}
