#include "LayerTreeRecovery.h"

#include <QElapsedTimer>
#include <QHash>
#include <QString>

#include <qgsmaplayer.h>

namespace {
struct Wait {
  int failures = 0;
  qint64 nextProbeMs = 0;
};

// GUI thread only, like the layer registry it follows.
QHash<QString, Wait>& waits() {
  static QHash<QString, Wait> table;
  return table;
}

std::function<qint64()>& testClock() {
  static std::function<qint64()> clock;
  return clock;
}

qint64 nowMs() {
  if (testClock()) return testClock()();
  static QElapsedTimer timer;
  if (!timer.isValid()) timer.start();
  return timer.elapsed();
}

// The source is part of the key, so repointing a layer starts over at once.
QString keyOf(const QgsMapLayer* layer) {
  return layer->id() + QLatin1Char('\n') + layer->source();
}
}  // namespace

int LayerTreeRecovery::backoffSeconds(int failures) {
  if (failures <= 0) return 0;
  const int shift = qMin(failures - 1, 5);
  return qMin(15 << shift, 300);
}

bool LayerTreeRecovery::shouldProbe(const QgsMapLayer* layer) {
  if (!layer) return false;
  const auto it = waits().constFind(keyOf(layer));
  return it == waits().constEnd() || nowMs() >= it->nextProbeMs;
}

void LayerTreeRecovery::noteMissing(const QgsMapLayer* layer) {
  if (!layer) return;
  Wait& wait = waits()[keyOf(layer)];
  ++wait.failures;
  wait.nextProbeMs = nowMs() + qint64(backoffSeconds(wait.failures)) * 1000;
}

void LayerTreeRecovery::noteFound(const QgsMapLayer* layer) {
  if (!layer || waits().isEmpty()) return;
  waits().remove(keyOf(layer));
}

void LayerTreeRecovery::setClockForTests(std::function<qint64()> msecsNow) {
  testClock() = std::move(msecsNow);
}

void LayerTreeRecovery::resetForTests() {
  waits().clear();
}
