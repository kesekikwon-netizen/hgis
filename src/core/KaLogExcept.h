#pragma once

// Session-log helper for catch blocks (evaluation F206).
//
//   try { ... } catch (...) { KA_LOG_EXCEPT(); return false; }
//
// The line reads "[except] core/BasemapOps.cpp:2296 — <what()>".
// __FILE__/__LINE__ replace hand-written "file:line" tags, which drifted as code
// moved between files. The active exception's message is added when it is a
// std::exception (or a QgsException where QGIS headers are available).
// Header-only: usable from ka_core, the app and tests without a CMake change.

#include "KaSessionLog.h"

#include <QString>

#include <exception>

#if defined(__has_include)
#if __has_include(<qgsexception.h>)
#include <qgsexception.h>
#define KA_LOG_EXCEPT_HAS_QGIS 1
#endif
#endif

namespace KaLogExceptDetail {

// "A:/repo/src/core/Foo.cpp" -> "core/Foo.cpp"; "…/tests/test_x.cpp" -> "tests/test_x.cpp".
inline QString sourceTag(const char* file, int line) {
  QString path = QString::fromUtf8(file ? file : "");
  path.replace(QLatin1Char('\\'), QLatin1Char('/'));
  if (!path.startsWith(QLatin1Char('/'))) path.prepend(QLatin1Char('/'));  // "src/…" as well
  const int src = path.lastIndexOf(QLatin1String("/src/"));
  const int tests = path.lastIndexOf(QLatin1String("/tests/"));
  if (src >= 0 && src >= tests)
    path = path.mid(src + 5);
  else if (tests >= 0)
    path = path.mid(tests + 1);
  else if (const int slash = path.lastIndexOf(QLatin1Char('/')); slash >= 0)
    path = path.mid(slash + 1);
  return QStringLiteral("%1:%2").arg(path).arg(line);
}

// Message of the exception currently being handled; empty for unknown types.
inline QString currentExceptionText() {
  const std::exception_ptr active = std::current_exception();
  if (!active) return {};
  try {
    std::rethrow_exception(active);
  } catch (const std::exception& e) {
    return QString::fromLocal8Bit(e.what());
#ifdef KA_LOG_EXCEPT_HAS_QGIS
  } catch (const QgsException& e) {
    return e.what();
#endif
  } catch (...) {
    return {};
  }
}

inline QString exceptLine(const char* file, int line) {
  QString text = QStringLiteral("[except] ") + sourceTag(file, line);
  const QString what = currentExceptionText().simplified();
  if (!what.isEmpty()) text += QStringLiteral(" — ") + what;
  return text;
}

inline void logExcept(const char* file, int line) noexcept {
  // Logging must never throw out of a catch block.
  try {
    KaSessionLog::line(exceptLine(file, line));
  } catch (...) {
    return;
  }
}

}  // namespace KaLogExceptDetail

#define KA_LOG_EXCEPT() ::KaLogExceptDetail::logExcept(__FILE__, __LINE__)
