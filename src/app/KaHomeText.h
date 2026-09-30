#pragma once

#include "core/RecentSurveys.h"

#include <QString>

namespace SurveyFacts {
struct Facts;
}

// Short, human wording for paths, times and states on the home page.
namespace KaHomeText {

// "바탕 화면 › 광령리" for a survey under the desktop, otherwise the folder path with ›.
QString folder(const QString& path);
// "오늘 14:02", "어제 09:10", "9월 3일 17:45" or "2025-11-02 08:00"; empty for 0.
QString when(qint64 msecsSinceEpoch);

// The 「상태」 chip of a recent-survey row: 「원본 없음」 (danger) when the file is not
// reachable, 「저장 안 됨」 (warn) when a recovery copy holds unsaved edits, else 「저장됨」.
// tone is a KaChip tone name; tip is the cell's tooltip.
struct StateChip {
  QString text;
  QString tone;
  QString tip;
};
StateChip stateChip(const RecentSurveys::Item& item, const QString& snapshot);

// Tooltip of the 「구역 · 유구 · 검수」 dots: what is known and what is still to come.
QString dotsTip(const SurveyFacts::Facts& facts, bool available);

}  // namespace KaHomeText
