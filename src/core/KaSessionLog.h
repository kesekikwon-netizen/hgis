#pragma once

#include <QString>
#include <QtGlobal>

// Session file logger used by KaCrashGuard and by catch (...) handlers in ka_core.
// Thread-safe. Repeated identical lines fold. KA_HGIS_LOG_DIR overrides the folder.
// A portable bundle (apps\qgis-dev beside the exe, the KaPortableRuntime rule) logs under its
// own config\logs so a USB copy keeps its logs with it; an installed/dev build keeps
// %LOCALAPPDATA%\ka-hgis\logs.
// session.log rotates to session.old.log when it exceeds maxBytes() (default 10 MiB); older
// rotations shift to session.old.2.log … session.old.<keepOldFiles()>.log.
// KA_HGIS_LOG_MAX_BYTES and KA_HGIS_LOG_KEEP override the cap and the kept count.
// Every line passes SecretMask first: key/account values never reach the file.
class KaSessionLog {
public:
  static constexpr qint64 kDefaultMaxBytes = 10LL * 1024 * 1024;
  static constexpr int kDefaultKeepOldFiles = 3;

  static void line(const QString& text);
  static void setQgisVersion(const QString& version);
  static QString buildLabel();
  static QString dir();
  static qint64 maxBytes();
  static int keepOldFiles();
  static QString dumpHint();
  // <exeDir>/config/logs when exeDir is a portable bundle, else empty.
  static QString portableLogDirFor(const QString& exeDir);
  // File name of the n-th rotated log (1 = session.old.log).
  static QString rotatedFileName(int index);
};
