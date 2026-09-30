#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

// Read-only readiness of the data-source keys and accounts (evaluation F172).
//
// The entry points stay where the user put them (더보기 → API 키 입력, VWorld 지적도,
// 수치지형도, 국가유산 인트라넷 아이디·비밀번호). This only reports 설정됨 / 미설정 so the
// home page or a status view can show every source in one place.
//
// Never returns, logs or stores a secret value and never rewrites an account file.
// The only indirect write is VworldSettings::loadApiKey(), which already moves a legacy
// key into its current settings location wherever the key is loaded.
namespace AccountStatus {

enum class Source {
  VworldKey,           // VWorld API 키: 배경 지도·지적 그림·시·군 판정
  HistoryGisKey,       // 역사지리정보DB 키: 1919 조선지형도
  CadastralAccount,    // VWorld 누리집 계정: 지적도 받기
  TopographicAccount,  // 국토정보플랫폼 계정: 수치지형도 받기
  HeritageAccount,     // 국가유산 인트라넷 계정: 주변유적 받기
};

struct Entry {
  Source source = Source::VworldKey;
  QString label;     // 「VWorld API 키」
  QString usedFor;   // 「배경 지도 · 지적 그림 · 시·군 판정」
  QString menuPath;  // 「더보기 → API 키 입력」
  bool ready = false;
};

// Returns true when that source has a usable key or ID+password on this PC.
using Probe = std::function<bool(Source)>;

// Every source in menu order, checked with the real settings (read-only).
QList<Entry> snapshot();
// Same list with an injected probe. Tests use this; no user settings are read.
QList<Entry> snapshot(const Probe& probe);

QString stateText(bool ready);  // 「설정됨」 / 「미설정」
int readyCount(const QList<Entry>& entries);
// 「자료 연결 5곳 중 3곳 설정됨 · 미설정: 수치지형도 계정, 국가유산 인트라넷 계정」
QString summaryLine(const QList<Entry>& entries);
// One line per source: 「VWorld API 키 — 설정됨 (더보기 → API 키 입력)」
QStringList detailLines(const QList<Entry>& entries);

}  // namespace AccountStatus
