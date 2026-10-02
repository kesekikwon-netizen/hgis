#include "KaGdalErrorLog.h"
#include "core/KaLogExcept.h"

#include "KaCrashGuard.h"

#include <cpl_error.h>

#include <atomic>
#include <cstdio>

namespace {

std::atomic<CPLErrorHandler> g_previous{nullptr};
std::atomic<bool> g_installed{false};

// CE_Fatal is followed by abort() after the handler returns. Stay off the Qt
// heap and off any mutex that logLine / QFile would take.
void writeFatalCrt(CPLErrorNum code, const char* message) {
  char buf[768];
  const int n = std::snprintf(buf, sizeof(buf), "[gdal] FATAL %d: %s\n", int(code),
                              message ? message : "");
  if (n > 0) {
    fputs(buf, stderr);
    fflush(stderr);
  }
}

void CPL_STDCALL handleGdalError(CPLErr level, CPLErrorNum code, const char* message) {
  thread_local bool inHandler = false;
  if (inHandler) return;
  inHandler = true;
  try {
    if (level >= CE_Fatal) {
      writeFatalCrt(code, message);
    } else {
      const QString line = KaGdalErrorLog::describe(int(level), int(code),
                                                    QString::fromUtf8(message ? message : ""));
      if (!line.isEmpty()) KaCrashGuard::logLine(line);  // thread safe, folds repeats
    }
    // 기본 처리기는 같은 오류를 터미널(stderr)에 다시 찍을 뿐이다: 세션 기록에 남겼으니 넘기지 않는다
    // (앱을 터미널에서 켜면 같은 오류 1000개가 쏟아졌다, R30).
    if (const CPLErrorHandler previous = g_previous.load(); previous && previous != CPLDefaultErrorHandler)
      previous(level, code, message);
  } catch (...) {
    KA_LOG_EXCEPT();
  }
  inHandler = false;
}

}  // namespace

namespace KaGdalErrorLog {

QString describe(int level, int code, const QString& message) {
  if (level < int(CE_Failure)) return QString();
  const QString kind = level >= int(CE_Fatal) ? QStringLiteral("FATAL") : QStringLiteral("ERROR");
  // GDAL messages may span lines; one log line per message keeps repeat folding intact.
  return QStringLiteral("[gdal] %1 %2: %3").arg(kind).arg(code).arg(message.simplified());
}

void install() {
  if (g_installed.exchange(true)) return;
  // Set updates the process handler. Push updates this thread, which is what
  // CPLError actually calls once any handler is already on the thread.
  g_previous.store(CPLSetErrorHandler(handleGdalError));
  CPLPushErrorHandler(handleGdalError);
}

void uninstall() {
  if (!g_installed.exchange(false)) return;
  CPLPopErrorHandler();
  CPLSetErrorHandler(g_previous.exchange(nullptr));
}

}  // namespace KaGdalErrorLog
