#pragma once

#include <QString>

// GDAL/OGR report errors through their own handler, which writes only to the
// console. A desktop launch has no console, so failures such as a GeoPackage
// that cannot be read ("unable to open database file") left no trace anywhere.
// This handler also records failures in the session log (KaCrashGuard), where
// repeated identical lines are folded into a count, and still passes every
// message on to the previous handler so a console run shows them as before.
namespace KaGdalErrorLog {

// Call once after QGIS/GDAL initialization.
void install();

// Restore the previous CPL handler. Safe to call when not installed.
void uninstall();

// The session log line for a GDAL message, or an empty string when the message
// is not worth recording. Only failures are recorded; warnings such as field
// width truncation are left to the console.
QString describe(int level, int code, const QString& message);

}  // namespace KaGdalErrorLog
