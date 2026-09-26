#pragma once
#include <QRegularExpression>
#include <QString>
#include <QStringList>

// 길·대로·로 뒤에 건물번호가 있으면 도로명주소다.
// 세종로는 법정동이라 그 뒤 번호는 지번으로 남긴다.
inline bool kaIsRoadAddress(const QString& query) {
  const QStringList words = query.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
  if (words.size() < 2) return false;
  static const QRegularExpression building(QStringLiteral(R"(^\d+(-\d+)?(번)?$)"));
  if (!building.match(words.last()).hasMatch()) return false;
  for (int i = 0; i < words.size() - 1; ++i) {
    const QString& word = words.at(i);
    if (word.endsWith(QStringLiteral("대로")) || word.endsWith(QStringLiteral("길"))) return true;
    if (word.endsWith(QStringLiteral("로")) && word != QStringLiteral("세종로")) return true;
  }
  return false;
}
