#include "HeritageFetchPlan.h"

#include "KoreaRegionCatalog.h"

#include <QDir>
#include <QFileInfo>

QString HeritageCity::display() const {
  if (!ok()) return {};
  const QString s = sido.trimmed();
  const QString c = city.trimmed();
  return s == c ? s : s + QLatin1Char(' ') + c;
}

bool HeritageCity::sameAs(const HeritageCity& other) const {
  return KoreaRegionCatalog::canonicalSido(sido) == KoreaRegionCatalog::canonicalSido(other.sido) &&
         city.trimmed() == other.city.trimmed();
}

QList<HeritageCity> HeritageFetchPlanning::normalizedFollowUps(const HeritageCity& first,
                                                               const QList<HeritageCity>& picked) {
  QList<HeritageCity> out;
  for (const HeritageCity& c : picked) {
    if (!c.ok() || c.sameAs(first)) continue;
    bool seen = false;
    for (const HeritageCity& kept : std::as_const(out))
      if (kept.sameAs(c)) { seen = true; break; }
    if (seen) continue;
    out.append({KoreaRegionCatalog::canonicalSido(c.sido), c.city.trimmed()});
  }
  return out;
}

QString HeritageFetchPlanning::siblingRoot(const QString& currentRoot, const HeritageCity& next) {
  const QString folder = QStringLiteral("%1 %2").arg(next.sido.trimmed(), next.city.trimmed());
  const QString current = QDir::cleanPath(currentRoot);
  // The caller lays out <base>/<시도 시군>/원본. Keep that layout for the next 시/군.
  // Pure string work: the next folder does not exist yet.
  if (QFileInfo(current).fileName() == QStringLiteral("원본")) {
    const QString base = QFileInfo(QFileInfo(current).path()).path();
    return QDir::cleanPath(QDir(base).filePath(folder + QStringLiteral("/원본")));
  }
  return QDir::cleanPath(QDir(current).filePath(folder));
}
