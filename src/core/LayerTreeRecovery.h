#pragma once

#include <QtGlobal>
#include <functional>

class QgsMapLayer;

// Retry spacing for layers whose source file is gone (USB pulled, L: network
// drive offline). reviveInvalidLayers keeps its 30 s audit and its immediate
// first attempt; once a file is confirmed missing, the next reload/exists probes
// on the GUI thread wait 15 s, 30 s, 60 s ... up to 5 minutes. A layer whose file
// exists but is briefly locked (GPKG being written) is not delayed.
namespace LayerTreeRecovery {

// False while a confirmed-missing source is waiting for its next probe.
bool shouldProbe(const QgsMapLayer* layer);
// The source file was checked and is missing: schedule the next probe later.
void noteMissing(const QgsMapLayer* layer);
// The layer is valid again (or its source changed): forget the wait.
void noteFound(const QgsMapLayer* layer);

// Wait before the next probe after `failures` confirmed misses (1 -> 15 s).
int backoffSeconds(int failures);

// Tests: replace the monotonic clock (milliseconds); an empty function restores it.
void setClockForTests(std::function<qint64()> msecsNow);
void resetForTests();

}  // namespace LayerTreeRecovery
