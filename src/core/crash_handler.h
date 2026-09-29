#pragma once
#include <Windows.h>
#include <string>

/*
 * CrashHandler — VectoredExceptionHandler that catches crashes
 * and writes a crash_YYYYMMDD_HHMMSS.log next to the DLL.
 * The DLL continues to run after the log is written (no terminate).
 */

namespace CrashHandler {
void install();
void uninstall();

// Manually trigger a crash log (for non-crash diagnostics)
void dumpInfo(const char* reason);
}
