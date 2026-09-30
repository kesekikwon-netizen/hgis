#include "TrenchPlanCache.h"

#include <QCryptographicHash>
#include <QList>
#include <QMutex>
#include <QMutexLocker>

#include <atomic>

namespace TrenchPlanCache {
namespace {

// A few areas x two targets x a handful of azimuths is all the dialog cycles through.
constexpr int kMaxEntries = 16;

struct Key {
  QByteArray areaHash;
  double targetPct = 0.0;
  double width = 0.0;
  double azimuthDeg = 0.0;
  bool operator==(const Key& other) const {
    return areaHash == other.areaHash && targetPct == other.targetPct && width == other.width &&
           azimuthDeg == other.azimuthDeg;
  }
};

struct Entry {
  Key key;
  TrenchGridGenerator::RatioFill plan;
};

QMutex& cacheMutex() {
  static QMutex mutex;
  return mutex;
}

// Newest first.
QList<Entry>& entries() {
  static QList<Entry> list;
  return list;
}

std::atomic<int> g_computeCount{0};

Key makeKey(const QByteArray& areaWkb, double targetPct, double width, double azimuthDeg) {
  return Key{QCryptographicHash::hash(areaWkb, QCryptographicHash::Sha1), targetPct, width,
             azimuthDeg};
}

bool find(const Key& key, TrenchGridGenerator::RatioFill* out) {
  QMutexLocker lock(&cacheMutex());
  QList<Entry>& list = entries();
  for (qsizetype i = 0; i < list.size(); ++i) {
    if (!(list.at(i).key == key)) continue;
    if (out) *out = list.at(i).plan;
    if (i > 0) list.move(i, 0);
    return true;
  }
  return false;
}

}  // namespace

bool lookup(const QByteArray& areaWkb, double targetPct, double width, double azimuthDeg,
            TrenchGridGenerator::RatioFill* out) {
  if (areaWkb.isEmpty()) return false;
  return find(makeKey(areaWkb, targetPct, width, azimuthDeg), out);
}

TrenchGridGenerator::RatioFill ratioPlan(const QByteArray& areaWkb, double targetPct, double width,
                                         double azimuthDeg) {
  if (areaWkb.isEmpty())
    return TrenchGridGenerator::buildForTargetRatio(areaWkb, targetPct, width, azimuthDeg);
  const Key key = makeKey(areaWkb, targetPct, width, azimuthDeg);
  TrenchGridGenerator::RatioFill plan;
  if (find(key, &plan)) return plan;
  // Computed outside the lock: two threads asking for the same new plan both search once.
  plan = TrenchGridGenerator::buildForTargetRatio(areaWkb, targetPct, width, azimuthDeg);
  ++g_computeCount;
  QMutexLocker lock(&cacheMutex());
  QList<Entry>& list = entries();
  list.removeIf([&key](const Entry& entry) { return entry.key == key; });
  list.prepend(Entry{key, plan});
  while (list.size() > kMaxEntries) list.removeLast();
  return plan;
}

int computeCount() { return g_computeCount.load(); }

void clear() {
  QMutexLocker lock(&cacheMutex());
  entries().clear();
  g_computeCount = 0;
}

}  // namespace TrenchPlanCache
