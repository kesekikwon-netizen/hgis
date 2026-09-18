#include "KaGdalErrorLog.h"

#include "KaCrashGuard.h"

#include <cpl_error.h>

#include <atomic>

namespace {

std::atomic<CPLErrorHandler> g_previous{nullptr};
std::atomic<bool> g_installed{false};

void CPL_STDCALL handleGdalError(CPLErr level, CPLErrorNum code, const char* message) {
  const QString line = KaGdalErrorLog::describe(int(level), int(code),
                                                QString::fromUtf8(message ? message : ""));
  if (!line.isEmpty()) KaCrashGuard::logLine(line);  // thread safe, folds repeats
  if (const CPLErrorHandler previous = g_previous.load())
    previous(level, code, message);
  else
    CPLDefaultErrorHandler(level, code, message);
}

}  // namespace

namespace KaGdalErrorLog {

QString describe(int level, int code, const QString& message) {
  if (level < int(CE_Failure)) return QString();
  const QString kind = level >= int(CE_Fatal) ? QStringLiteral("FATAL") : QStringLiteral("ERROR");
  return QStringLiteral("[gdal] %1 %2: %3").arg(kind).arg(code).arg(message.trimmed());
}

void install() {
  if (g_installed.exchange(true)) return;
  g_previous.store(CPLSetErrorHandler(handleGdalError));
}

}  // namespace KaGdalErrorLog
