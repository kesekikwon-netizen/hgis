#pragma once

#include <QString>

// 충돌 진단 장치. 처리되지 않은 예외·치명적 CRT 오류가 나면
// %LOCALAPPDATA%\ka-hgis\logs 에 (1) 사람이 읽을 수 있는 심볼 스택 로그와
// (2) 미니덤프(.dmp)를 남긴다. 경고 이상 Qt/QGIS 메시지도 세션 로그로 남겨
// "어디서 에러가 났는지"를 현장에서 재현 없이 추적할 수 있게 한다.
class KaCrashGuard {
public:
  // main() 최상단에서 한 번 호출. (QApplication 생성 전이어도 안전)
  static void install();

  // 세션 로그 한 줄 기록(스레드 안전, 연속 중복은 횟수로 접어서 기록).
  static void logLine(const QString& line);

  // 로그 폴더 경로(%LOCALAPPDATA%\ka-hgis\logs). KA_HGIS_LOG_DIR 이 있으면 그 폴더를
  // 쓴다. 검사가 사용자의 실제 세션 로그와 충돌 폴더에 기록하지 않게 하려는 것이다.
  static QString logDir();

  // session.log·crash-*.log·crash-*.dmp 위치를 한 줄로 안내한다.
  static QString dumpHint();

  // Crash files are kept, but not forever: install() keeps only the newest dumps and logs.
  // A dump can hold memory contents, so the cap also limits what a shared laptop carries.
  static constexpr int kKeepCrashDumps = 5;
  static constexpr int kKeepCrashLogs = 20;

  // Removes the oldest crash-*.dmp / crash-*.log in dir beyond the given counts.
  // File names carry the timestamp (crash-YYYYMMDD-HHMMSS), so name order is age order.
  // Returns how many files were removed. Other files in the folder are never touched.
  static int pruneCrashFiles(const QString& dir, int keepDumps = kKeepCrashDumps,
                             int keepLogs = kKeepCrashLogs);

  // MINIDUMP_TYPE flags used for crash dumps (0 off Windows). Thread info and stacks only:
  // no indirectly referenced heap, which could carry keys or account values.
  static unsigned long miniDumpTypeFlags();
};
