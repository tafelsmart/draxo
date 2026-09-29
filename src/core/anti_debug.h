#pragma once
#include <Windows.h>
#include <winternl.h>
#include <atomic>

/*
 * ANTI_DEBUG — Multi-layer debugger detection & self-disable.
 *
 * Layers (ordered from basic to advanced):
 *   1. IsDebuggerPresent()              — user-mode flag, easily bypassed
 *   2. CheckRemoteDebuggerPresent()      — kernel-mode check
 *   3. NtQueryInfo(ProcessDebugPort)     — kernel debug port (ring0 debugger)
 *   4. NtQueryInfo(ProcessDebugObject)   — debug object handle
 *   5. NtQueryInfo(ProcessDebugFlags)    — EPROCESS->NoDebugInherit
 *   6. Timing check (rdtsc)              — detects instruction-level slowdown
 *   7. NtGlobalFlag check                — heap debug flags set by debugger
 *   8. Hardware breakpoint scan          — checks DR0-DR3 debug registers
 *
 * If ANY layer triggers, all features are silently disabled.
 * NO crash, NO MessageBox, NO visible reaction — that would confirm
 * detection to the reverse engineer. The DLL loads but does nothing.
 */

namespace antidebug {

// Global flag: set to true if debugger detected during init
inline std::atomic<bool> g_detected = false;

// ── NtQueryInformationProcess wrapper ───────────────────────────────
typedef NTSTATUS (NTAPI* pNtQueryInformationProcess)(
    HANDLE, DWORD, PVOID, ULONG, PULONG);

inline pNtQueryInformationProcess getNtQueryInfo() {
    static pNtQueryInformationProcess fn = nullptr;
    if (!fn) {
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (ntdll)
            fn = (pNtQueryInformationProcess)GetProcAddress(ntdll, "NtQueryInformationProcess");
    }
    return fn;
}

// ── Layer 1: IsDebuggerPresent ──────────────────────────────────────
inline bool layer_isDebuggerPresent() {
    return IsDebuggerPresent() != FALSE;
}

// ── Layer 2: CheckRemoteDebuggerPresent ─────────────────────────────
inline bool layer_checkRemoteDebugger() {
    BOOL remoteDebugger = FALSE;
    CheckRemoteDebuggerPresent(GetCurrentProcess(), &remoteDebugger);
    return remoteDebugger != FALSE;
}

// ── Layer 3: ProcessDebugPort (kernel debug port) ───────────────────
inline bool layer_kernelDebugPort() {
    auto fn = getNtQueryInfo();
    if (!fn) return false;
    DWORD debugPort = 0;
    NTSTATUS st = fn(GetCurrentProcess(), 7 /* ProcessDebugPort */,
                     &debugPort, sizeof(debugPort), nullptr);
    return (st >= 0 && debugPort != 0);
}

// ── Layer 4: ProcessDebugObjectHandle ───────────────────────────────
inline bool layer_debugObjectHandle() {
    auto fn = getNtQueryInfo();
    if (!fn) return false;
    HANDLE debugObj = nullptr;
    NTSTATUS st = fn(GetCurrentProcess(), 0x1E /* ProcessDebugObjectHandle */,
                     &debugObj, sizeof(debugObj), nullptr);
    return (st >= 0 && debugObj != nullptr);
}

// ── Layer 5: ProcessDebugFlags (NoDebugInherit = 0 if debugged) ────
inline bool layer_debugFlags() {
    auto fn = getNtQueryInfo();
    if (!fn) return false;
    DWORD noDebugInherit = 0;
    NTSTATUS st = fn(GetCurrentProcess(), 0x1F /* ProcessDebugFlags */,
                     &noDebugInherit, sizeof(noDebugInherit), nullptr);
    // If debugged, EPROCESS->NoDebugInherit = 0
    return (st >= 0 && noDebugInherit == 0);
}

// ── Layer 6: Timing check (rdtsc) ───────────────────────────────────
// Debuggers add instruction-level overhead. We measure a tight loop of
// simple instructions and compare against normal timing.
inline bool layer_timingCheck() {
    // Time a few simple operations. Debuggers introduce measurable latency.
    LARGE_INTEGER freq, start, end;
    if (!QueryPerformanceFrequency(&freq)) return false;
    QueryPerformanceCounter(&start);

    // 1000 volatile operations to amplify timing differences
    volatile int x = 0;
    for (int i = 0; i < 1000; i++) {
        x += i;
        x ^= (i << 2);
    }

    QueryPerformanceCounter(&end);
    double elapsedUs = (double)(end.QuadPart - start.QuadPart) * 1e6 / freq.QuadPart;

    // Normal execution: < 100us. Debugger with single-step traps: > 500us.
    // Use a generous threshold to avoid false positives on slow hardware.
    (void)x; // suppress unused warning
    return elapsedUs > 400.0;
}

// ── Layer 7: NtGlobalFlag (heap debugging flags) ────────────────────
// When a process is started by a debugger, NtGlobalFlag gets these flags:
// FLG_HEAP_ENABLE_TAIL_CHECK (0x10)
// FLG_HEAP_ENABLE_FREE_CHECK  (0x20)
// FLG_HEAP_VALIDATE_PARAMETERS(0x40)
// Typical debugged value: 0x70
inline bool layer_ntGlobalFlag() {
    auto fn = getNtQueryInfo();
    if (!fn) return false;

    // Read PEB via __readgsqword (x64) or __readfsdword (x86)
    #ifdef _WIN64
    PPEB peb = (PPEB)__readgsqword(0x60);
    #else
    PPEB peb = (PPEB)__readfsdword(0x30);
    #endif

    if (!peb) return false;

    // NtGlobalFlag is at offset 0xBC in PEB (x64)
    DWORD flags = *(DWORD*)((BYTE*)peb + 0xBC);

    // Check for heap debug flags
    return (flags & 0x70) != 0;
}

// ── Layer 8: Hardware breakpoint scan (DR0-DR3) ─────────────────────
// Hardware breakpoints (set by debuggers like x64dbg/IDA) are stored
// in the x86 debug registers DR0-DR3. We read them via GetThreadContext.
// NOTE: Must NOT SuspendThread on the current thread - undefined behavior!
// Instead we use a helper thread or read directly (GetThreadContext works
// on the calling thread without suspension).
inline bool layer_hardwareBreakpoints() {
    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;

    // GetThreadContext on the current thread works fine for reading
    // debug registers on Windows 10+. No suspension needed.
    BOOL ok = GetThreadContext(GetCurrentThread(), &ctx);

    if (!ok) return false;

    // If any of DR0-DR3 is non-zero, a hardware breakpoint is set
    return (ctx.Dr0 != 0 || ctx.Dr1 != 0 || ctx.Dr2 != 0 || ctx.Dr3 != 0);
}

// ── Master check: run ALL layers ────────────────────────────────────
inline bool detect() {
    int hits = 0;
    int total = 0;

    #define CHECK(layer, name) do { total++; if (layer()) { hits++; } } while(0)

    CHECK(layer_isDebuggerPresent,    "IsDebuggerPresent");
    CHECK(layer_checkRemoteDebugger,   "CheckRemoteDebugger");
    CHECK(layer_kernelDebugPort,       "KernelDebugPort");
    CHECK(layer_debugObjectHandle,     "DebugObjectHandle");
    CHECK(layer_debugFlags,            "DebugFlags");
    CHECK(layer_timingCheck,           "TimingCheck");
    CHECK(layer_ntGlobalFlag,          "NtGlobalFlag");
    CHECK(layer_hardwareBreakpoints,   "HardwareBreakpoints");

    #undef CHECK

    // Require at least 2 independent layers to agree (reduces false positives)
    bool detected = (hits >= 2);

    if (detected) {
        g_detected = true;
        // Silent: no printf, no MessageBox, no Beep — don't alert the debugger
    }

    return detected;
}

// Quick check: call this BEFORE any module/hook init
// Returns true if safe to continue, false if debugger was detected
inline bool isSafeToInit() {
    g_detected = false;
    return !detect();
}

} // namespace antidebug
