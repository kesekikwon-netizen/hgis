#include "KaStartPage.h"
#include "KaStartHero.h"
#include "KaTheme.h"
#include "core/RecentSurveys.h"

#include <QAbstractItemView>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {

// "바탕 화면 › 광령리" for a survey under the desktop, otherwise the folder path.
QString friendlyFolder(const QString& path) {
  const QString dir = QDir::cleanPath(QFileInfo(path).absolutePath());
  const struct { QStandardPaths::StandardLocation where; const char* label; } roots[] = {
      {QStandardPaths::DesktopLocation, "바탕 화면"},
      {QStandardPaths::DocumentsLocation, "문서"},
  };
  for (const auto& root : roots) {
    const QString base = QDir::cleanPath(QStandardPaths::writableLocation(root.where));
    if (base.isEmpty()) continue;
    if (dir.compare(base, Qt::CaseInsensitive) == 0) return QString::fromUtf8(root.label);
    if (dir.startsWith(base + QLatin1Char('/'), Qt::CaseInsensitive))
      return QString::fromUtf8(root.label) + QStringLiteral(" › ") +
             dir.mid(base.size() + 1).split(QLatin1Char('/')).join(QStringLiteral(" › "));
  }
  return dir.split(QLatin1Char('/'), Qt::SkipEmptyParts).join(QStringLiteral(" › "));
}

QString friendlyWhen(qint64 ms) {
  if (ms <= 0) return {};
  const QDateTime when = QDateTime::fromMSecsSinceEpoch(ms);
  const QDate today = QDate::currentDate();
  const QString time = when.toString(QStringLiteral("HH:mm"));
  if (when.date() == today) return QStringLiteral("오늘 %1").arg(time);
  if (when.date() == today.addDays(-1)) return QStringLiteral("어제 %1").arg(time);
  if (when.date().year() == today.year())
    return QStringLiteral("%1월 %2일 %3").arg(when.date().month()).arg(when.date().day()).arg(time);
  return when.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

QLabel* label(const QString& text, const char* name, QWidget* parent) {
  auto* l = new QLabel(text, parent);
  l->setObjectName(QString::fromLatin1(name));
  return l;
}

}  // namespace

KaStartPage::KaStartPage(QWidget* parent) : QWidget(parent) {
  setObjectName(QStringLiteral("startPage"));
  setAttribute(Qt::WA_StyledBackground, true);
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(22, 20, 22, 20);
  root->setSpacing(18);
  root->addWidget(buildHero());
  // 최근 조사 옆에 작업 순서 카드를 둔다. 목록이 짧을 때 아래 절반이 통째로 비었다.
  auto* lower = new QHBoxLayout;
  lower->setSpacing(18);
  lower->addWidget(buildRecentCard(), 1);
  lower->addWidget(buildGuideCard(), 0);
  root->addLayout(lower, 1);
  reload();
}

QWidget* KaStartPage::buildHero() {
  auto* hero = new KaStartHero(this);
  hero->setFixedHeight(236);
  auto* row = new QHBoxLayout(hero);
  row->setContentsMargins(44, 0, 24, 0);
  auto* text = new QVBoxLayout;
  text->setSpacing(10);
  text->addStretch(1);
  auto* brand = new QHBoxLayout;
  brand->setSpacing(16);
  brand->addWidget(label(QStringLiteral("Strata"), "startHeroName", hero));
  auto* divider = new QFrame(hero);
  divider->setObjectName(QStringLiteral("startHeroDivider"));
  divider->setFixedSize(1, 28);
  brand->addWidget(divider, 0, Qt::AlignVCenter);
  brand->addWidget(label(QStringLiteral("필드고고학 GIS"), "startHeroProduct", hero), 0,
                   Qt::AlignVCenter);
  brand->addStretch(1);
  text->addLayout(brand);
  auto* lead = label(QStringLiteral("오늘 현장을 새로 만들거나, 지난 조사를 이어서 여세요."),
                     "startHeroLead", hero);
  lead->setWordWrap(true);  // the band narrows with small field screens
  text->addWidget(lead);
  auto* actions = new QHBoxLayout;
  actions->setSpacing(12);
  auto* btnNew = new QPushButton(QStringLiteral("새 조사"), hero);
  btnNew->setObjectName(QStringLiteral("startNewBtn"));
  auto* btnOpen = new QPushButton(QStringLiteral("조사 열기"), hero);
  btnOpen->setObjectName(QStringLiteral("startOpenBtn"));
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
  lines->setContentsMargins(22, 18, 22, 18);
  lines->setSpacing(4);
  lines->addWidget(label(QStringLiteral("이어서 작업"), "startContinueTag", card));
  m_continueName = label(QString(), "startContinueName", card);
  m_continuePath = label(QString(), "startContinuePath", card);
  m_continueWhen = label(QString(), "startContinueMeta", card);
  lines->addWidget(m_continueName);
  lines->addWidget(m_continuePath);
  lines->addSpacing(8);
  lines->addWidget(m_continueWhen, 0, Qt::AlignLeft);
  lines->addSpacing(10);
  auto* resume = new QPushButton(QStringLiteral("이어서 열기  →"), card);
  resume->setObjectName(QStringLiteral("startContinueBtn"));
  connect(resume, &QPushButton::clicked, this, [this]() {
    if (!m_continueTarget.isEmpty()) emit recentOpened(m_continueTarget);
  });
  lines->addWidget(resume, 0, Qt::AlignLeft);
  m_continue = card;
  row->addWidget(card, 0, Qt::AlignVCenter);
  return hero;
}

QWidget* KaStartPage::buildGuideCard() {
  auto* card = new QFrame(this);
  card->setObjectName(QStringLiteral("startGuideCard"));
  card->setFixedWidth(340);
  auto* col = new QVBoxLayout(card);
  col->setContentsMargins(20, 16, 20, 18);
  col->setSpacing(14);
  col->addWidget(label(QStringLiteral("작업 순서"), "startRecentTitle", card));
  struct Step { const char* title; const char* body; };
  const Step steps[] = {
      {"새 조사", "조사 이름과 좌표계(5186·5187)를 정하면 조사 파일(GPKG)이 만들어집니다."},
      {"조사구역 그리기", "지도 탭에서 위성·지적을 보며 「그리기」로 구역과 유구를 그립니다."},
      {"도면 · 제출", "「도면」으로 종이에 옮기고, 「제출 변환」으로 5179 SHP·PDF를 냅니다."},
  };
  int number = 1;
  for (const auto& step : steps) {
    auto* row = new QHBoxLayout;
    row->setSpacing(12);
    auto* badge = label(QString::number(number++), "startStepNumber", card);
    badge->setAlignment(Qt::AlignCenter);
    badge->setFixedSize(26, 26);
    row->addWidget(badge, 0, Qt::AlignTop);
    auto* text = new QVBoxLayout;
    text->setSpacing(2);
    text->addWidget(label(QString::fromUtf8(step.title), "startStepTitle", card));
    auto* body = label(QString::fromUtf8(step.body), "startStepBody", card);
    body->setWordWrap(true);
    text->addWidget(body);
    row->addLayout(text, 1);
    col->addLayout(row);
  }
  col->addStretch(1);
  return card;
}

QWidget* KaStartPage::buildRecentCard() {
  auto* card = new QFrame(this);
  card->setObjectName(QStringLiteral("startRecentCard"));
  auto* rows = new QVBoxLayout(card);
  rows->setContentsMargins(0, 0, 0, 6);
  rows->setSpacing(0);
  auto* header = new QHBoxLayout;
  header->setContentsMargins(18, 14, 18, 12);
  header->setSpacing(10);
  header->addWidget(label(QStringLiteral("최근 조사"), "startRecentTitle", card));
  m_count = label(QString(), "startRecentCount", card);
  header->addWidget(m_count);
  header->addStretch(1);
  header->addWidget(label(QStringLiteral("오른쪽 클릭: 목록에서 제거"), "startRecentCount", card));
  m_filter = new QLineEdit(card);
  m_filter->setObjectName(QStringLiteral("startRecentFilter"));
  m_filter->setPlaceholderText(QStringLiteral("이름·폴더로 찾기"));
  m_filter->setClearButtonEnabled(true);
  m_filter->setFixedWidth(260);
  connect(m_filter, &QLineEdit::textChanged, this, &KaStartPage::applyFilter);
  header->addWidget(m_filter);
  rows->addLayout(header);

  m_empty = label(QStringLiteral("아직 최근 조사가 없습니다. 새 조사 또는 조사 열기로 시작하세요."),
                  "recentEmptyHint", card);
  m_empty->setWordWrap(true);
  m_empty->setContentsMargins(18, 8, 18, 8);
  rows->addWidget(m_empty);

  m_recent = new QTableWidget(0, 3, card);
  m_recent->setObjectName(QStringLiteral("recentSurveyList"));
  m_recent->horizontalHeader()->setVisible(false);
  m_recent->verticalHeader()->setVisible(false);
  m_recent->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_recent->setSelectionMode(QAbstractItemView::SingleSelection);
  m_recent->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_recent->setShowGrid(false);
  m_recent->setFocusPolicy(Qt::StrongFocus);
  m_recent->setContextMenuPolicy(Qt::CustomContextMenu);
  m_recent->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
  m_recent->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  m_recent->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  m_recent->setColumnWidth(0, 320);
  connect(m_recent, &QTableWidget::cellActivated, this, [this](int row, int) { openRow(row); });
  connect(m_recent, &QTableWidget::cellClicked, this, [this](int row, int) { openRow(row); });
  connect(m_recent, &QTableWidget::customContextMenuRequested, this, &KaStartPage::showRecentMenu);
  rows->addWidget(m_recent, 1);
  return card;
}

void KaStartPage::reload() {
  if (!m_recent) return;
  m_recent->setRowCount(0);
  QSettings st = RecentSurveys::userSettings();
  const auto items = RecentSurveys::load(st);
  if (m_empty) m_empty->setVisible(items.isEmpty());
  m_recent->setVisible(!items.isEmpty());
  if (m_count) m_count->setText(items.isEmpty() ? QString() : QStringLiteral("%1개").arg(items.size()));
  const QColor muted = KaTheme::tokens().inkMuted;
  int row = 0;
  for (const RecentSurveys::Item& it : items) {
    m_recent->insertRow(row);
    auto* name = new QTableWidgetItem(it.name);
    QFont bold = name->font();
    bold.setBold(true);
    name->setFont(bold);
    name->setData(Qt::UserRole, it.path);
    name->setToolTip(it.path);
    auto* folder = new QTableWidgetItem(friendlyFolder(it.path));
    folder->setForeground(muted);
    folder->setToolTip(it.path);
    auto* when = new QTableWidgetItem(friendlyWhen(it.lastOpenedMs));
    when->setForeground(muted);
    when->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_recent->setItem(row, 0, name);
    m_recent->setItem(row, 1, folder);
    m_recent->setItem(row, 2, when);
    m_recent->setRowHeight(row, 44);
    ++row;
  }
  if (m_continue) m_continue->setVisible(!items.isEmpty());
  m_continueTarget = items.isEmpty() ? QString() : items.first().path;
  if (!items.isEmpty()) {
    const RecentSurveys::Item& last = items.first();
    m_continueName->setText(last.name);
    m_continuePath->setText(m_continuePath->fontMetrics().elidedText(
        last.path, Qt::ElideMiddle, 260));  // '/' reads as a path; '\' shows as ₩ in Malgun
    m_continuePath->setToolTip(last.path);
    m_continueWhen->setText(QStringLiteral("마지막 열림 %1").arg(friendlyWhen(last.lastOpenedMs)));
  }
  if (m_filter) applyFilter(m_filter->text());
}

void KaStartPage::applyFilter(const QString& text) {
  if (!m_recent) return;
  const QString needle = text.trimmed();
  for (int row = 0; row < m_recent->rowCount(); ++row) {
    const QTableWidgetItem* name = m_recent->item(row, 0);
    const QString path = name ? name->data(Qt::UserRole).toString() : QString();
    const bool match = needle.isEmpty() ||
                       (name && name->text().contains(needle, Qt::CaseInsensitive)) ||
                       path.contains(needle, Qt::CaseInsensitive);
    m_recent->setRowHidden(row, !match);
  }
}

void KaStartPage::openRow(int row) {
  if (!m_recent || row < 0) return;
  auto* it = m_recent->item(row, 0);
  if (!it) return;
  const QString path = it->data(Qt::UserRole).toString();
  if (path.isEmpty()) return;
  emit recentOpened(path);
}

void KaStartPage::showRecentMenu(const QPoint& pos) {
  if (!m_recent) return;
  auto* it = m_recent->itemAt(pos);
  if (!it) return;
  auto* name = m_recent->item(it->row(), 0);
  if (!name) return;
  const QString path = name->data(Qt::UserRole).toString();
  if (path.isEmpty()) return;
  QMenu menu(this);
  QAction* forget = menu.addAction(QStringLiteral("목록에서 제거"));
  if (menu.exec(m_recent->viewport()->mapToGlobal(pos)) == forget)
    emit forgetRequested(path);
}
