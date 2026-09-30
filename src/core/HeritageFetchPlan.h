#pragma once

#include <QList>
#include <QString>

// One request unit of the heritage intranet: a single 시/군. Never 전국, never 읍면동.
struct HeritageCity {
  QString sido;  // 경상북도
  QString city;  // 안동시

  bool ok() const { return !sido.trimmed().isEmpty() && !city.trimmed().isEmpty(); }
  QString display() const;  // "경상북도 안동시" (세종처럼 같으면 한 번)
  bool sameAs(const HeritageCity& other) const;
};

// What the one 시·군 confirm dialog decided beyond the first 시/군 (evaluation F120, F174).
// Each 시/군 is still requested on its own, one after another.
struct HeritageFetchPlan {
  // Neighbouring 시/군 that the survey's 5 km scope touches and the user ticked.
  QList<HeritageCity> followUps;
  // Use a recent, already validated download of the same 시/군 instead of asking the site.
  bool reuseRecent = false;
};

namespace HeritageFetchPlanning {

// Keeps order, drops invalid entries, duplicates and the first 시/군 itself.
QList<HeritageCity> normalizedFollowUps(const HeritageCity& first, const QList<HeritageCity>& picked);

// Download root of another 시/군 next to the current one.
// ".../주변유적/경상북도 안동시/원본" + 예천군 → ".../주변유적/경상북도 예천군/원본".
// When the current root does not follow that layout, the 시/군 folder goes inside it.
QString siblingRoot(const QString& currentRoot, const HeritageCity& next);

}  // namespace HeritageFetchPlanning
