#pragma once

#include <QString>
#include <QtGlobal>

// Session file logger used by KaCrashGuard and by catch (...) handlers in ka_core.
// Thread-safe. Repeated identical lines fold. KA_HGIS_LOG_DIR overrides the folder.
// session.log rotates to session.old.log when it exceeds maxBytes() (default 10 MiB).
// KA_HGIS_LOG_MAX_BYTES overrides the cap for tests.
class KaSessionLog {
public:
  static constexpr qint64 kDefaultMaxBytes = 10LL * 1024 * 1024;

  static void line(const QString& text);
  static QString dir();
  static qint64 maxBytes();
  static QString dumpHint();
};
