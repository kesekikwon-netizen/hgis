#include "AccountStatus.h"

#include "CadastralPortal.h"
#include "HeritageIntranetSettings.h"
#include "TopographicSettings.h"
#include "VworldSettings.h"

#include <QStringList>

namespace {

struct Descriptor {
  AccountStatus::Source source;
  const char* label;
  const char* usedFor;
  const char* menuPath;
};

// Menu order of 더보기. Labels match the menu entries the user already knows.
const Descriptor kSources[] = {
    {AccountStatus::Source::VworldKey, "VWorld API 키", "배경 지도 · 지적 그림 · 시·군 판정",
     "더보기 → API 키 입력"},
    {AccountStatus::Source::HistoryGisKey, "역사지리정보DB 키", "1919 조선지형도",
     "더보기 → API 키 입력"},
    {AccountStatus::Source::CadastralAccount, "VWorld 지적도 계정", "지적도 받기",
     "더보기 → VWorld 지적도 아이디·비밀번호"},
    {AccountStatus::Source::TopographicAccount, "수치지형도 계정", "수치지형도 받기",
     "더보기 → 수치지형도 아이디·비밀번호"},
    {AccountStatus::Source::HeritageAccount, "국가유산 인트라넷 계정", "주변유적 받기",
     "더보기 → 국가유산 인트라넷 아이디·비밀번호"},
};

bool realProbe(AccountStatus::Source source) {
  switch (source) {
    case AccountStatus::Source::VworldKey:
      return !VworldSettings::loadApiKey().trimmed().isEmpty();
    case AccountStatus::Source::HistoryGisKey:
      return !VworldSettings::loadHistoryGisApiKey().trimmed().isEmpty();
    case AccountStatus::Source::CadastralAccount:
      return CadastralPortal::hasCredentials();
    case AccountStatus::Source::TopographicAccount:
      return TopographicSettings::hasCredentials();
    case AccountStatus::Source::HeritageAccount:
      return HeritageIntranetSettings::hasCredentials();
  }
  return false;
}

}  // namespace

QList<AccountStatus::Entry> AccountStatus::snapshot() { return snapshot(realProbe); }

QList<AccountStatus::Entry> AccountStatus::snapshot(const Probe& probe) {
  QList<Entry> entries;
  for (const Descriptor& d : kSources) {
    Entry e;
    e.source = d.source;
    e.label = QString::fromUtf8(d.label);
    e.usedFor = QString::fromUtf8(d.usedFor);
    e.menuPath = QString::fromUtf8(d.menuPath);
    e.ready = probe ? probe(d.source) : false;
    entries.append(e);
  }
  return entries;
}

QString AccountStatus::stateText(bool ready) {
  return ready ? QStringLiteral("설정됨") : QStringLiteral("미설정");
}

int AccountStatus::readyCount(const QList<Entry>& entries) {
  int n = 0;
  for (const Entry& e : entries)
    if (e.ready) ++n;
  return n;
}

QString AccountStatus::summaryLine(const QList<Entry>& entries) {
  QStringList missing;
  for (const Entry& e : entries)
    if (!e.ready) missing.append(e.label);
  const QString head = QStringLiteral("자료 연결 %1곳 중 %2곳 설정됨")
                           .arg(entries.size())
                           .arg(readyCount(entries));
  if (missing.isEmpty()) return head;
  return head + QStringLiteral(" · 미설정: ") + missing.join(QStringLiteral(", "));
}

QStringList AccountStatus::detailLines(const QList<Entry>& entries) {
  QStringList lines;
  for (const Entry& e : entries)
    lines.append(QStringLiteral("%1 — %2 (%3)").arg(e.label, stateText(e.ready), e.menuPath));
  return lines;
}
