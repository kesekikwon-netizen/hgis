#include "KaHomeText.h"

#include "core/SurveyFacts.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStringList>

namespace KaHomeText {

QString folder(const QString& path) {
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

QString when(qint64 ms) {
  if (ms <= 0) return {};
  const QDateTime at = QDateTime::fromMSecsSinceEpoch(ms);
  const QDate today = QDate::currentDate();
  const QString time = at.toString(QStringLiteral("HH:mm"));
  if (at.date() == today) return QStringLiteral("오늘 %1").arg(time);
  if (at.date() == today.addDays(-1)) return QStringLiteral("어제 %1").arg(time);
  if (at.date().year() == today.year())
    return QStringLiteral("%1월 %2일 %3").arg(at.date().month()).arg(at.date().day()).arg(time);
  return at.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

StateChip stateChip(const RecentSurveys::Item& item, const QString& snapshot) {
  if (!item.available)
    return {QStringLiteral("원본 없음"), QStringLiteral("danger"),
            QStringLiteral("지금 찾을 수 없습니다. USB·네트워크 드라이브를 연결하면 다시 열 수 있습니다.")};
  if (!snapshot.isEmpty())
    return {QStringLiteral("저장 안 됨"), QStringLiteral("warn"),
            QStringLiteral("저장하지 않은 편집의 복구 사본이 있습니다. 오른쪽 클릭에서 사본을 엽니다.")};
  return {QStringLiteral("저장됨"), QStringLiteral("ok"),
          QStringLiteral("마지막 저장 뒤 남은 편집이 없습니다. 누르면 이 조사를 엽니다.")};
}

QString dotsTip(const SurveyFacts::Facts& facts, bool available) {
  if (!available) return QStringLiteral("원본을 찾을 수 없어 구역·유구·검수를 알 수 없습니다.");
  QStringList lines;
  lines << (facts.hasCounts() ? QStringLiteral("구역 %1 · 유구 %2").arg(facts.areas).arg(facts.features)
                              : QStringLiteral("구역·유구: 아직 세지 않았습니다. 저장하면 기록됩니다."));
  lines << (facts.hasCheck() ? QStringLiteral("검수: 오류 %1 · 경고 %2").arg(facts.errors).arg(facts.warnings)
                             : QStringLiteral("검수: 아직 하지 않았습니다."));
  return lines.join(QLatin1Char('\n'));
}

}  // namespace KaHomeText
