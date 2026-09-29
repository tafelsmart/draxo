#pragma once
#include <Windows.h>
#include <cstdint>

/*
 * INTEGRITY - DLL Tamper Detection
 *
 * A post-build script (tools/patch_integrity.py) computes the CRC32 of
 * draxo.dll's .text section and patches a magic placeholder inside the DLL.
 *
 * At runtime integrity::check() recomputes the CRC and compares it against
 * the patched value. If they differ, someone modified the DLL.
 *
 * PLACEMENT GUARANTEE
 * -------------------
 * The sentinel lives in a DEDICATED PE section (.drsent) via
 * __declspec(allocate) with a single definition in integrity.cpp.
 * That way MSVC can NEVER fold the magic constants into code immediates
 * (which happened with plain inline/volatile variables and made the
 * post-build patch script fail intermittently). The 16 bytes are always
 * present as raw data in the binary.
 *
 * Response to tamper:
 *   - Sets g_tampered = true
 *   - Subsequent checks in the main loop trigger a crash via null-deref
 *     (NOT ExitProcess/MsgBox - too easy to patch out)
 */

// Defined in integrity.cpp (dedicated PE section .drsent, see below).
// Declared extern so only ONE translation unit owns the definition.
extern const volatile uint64_t g_drsentinel[2];

namespace integrity {

inline volatile bool    g_tampered  = false;
inline volatile HMODULE g_ourModule = nullptr;

// ── CRC32 (standard polynomial, table-driven, computed once) ────────
inline uint32_t crc32(const uint8_t* data, size_t len) {
    static uint32_t table[256] = {};
    static bool tableReady = false;
    if (!tableReady) {
        for (int i = 0; i < 256; i++) {
            uint32_t crc = (uint32_t)i;
            for (int j = 0; j < 8; j++)
                crc = (crc & 1) ? (0xEDB88320U ^ (crc >> 1)) : (crc >> 1);
            table[i] = crc;
        }
        tableReady = true;
    }
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; ++i)
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFF;
}

// ── Main check ──────────────────────────────────────────────────────
// Returns true if DLL passes integrity check, false if tampered.
// Must be called AFTER g_ourModule is set (DLL_PROCESS_ATTACH).
inline bool check() {
    if (!g_ourModule) return false;

    // Read the DLL from disk (not from memory - in-memory could have
    // relocation fixups and imports resolved, which changes bytes)
    wchar_t dllPath[MAX_PATH];
    if (!GetModuleFileNameW((HMODULE)g_ourModule, dllPath, MAX_PATH))
        return false;

    HANDLE hFile = CreateFileW(dllPath, GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    // Read entire DLL into memory
    DWORD fileSize = GetFileSize(hFile, nullptr);
    if (fileSize == INVALID_FILE_SIZE || fileSize < 1024) {
        CloseHandle(hFile);
        return false;
    }

    uint8_t* buffer = (uint8_t*)VirtualAlloc(nullptr, fileSize,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!buffer) { CloseHandle(hFile); return false; }

    DWORD bytesRead = 0;
    if (!ReadFile(hFile, buffer, fileSize, &bytesRead, nullptr) ||
        bytesRead != fileSize) {
        VirtualFree(buffer, 0, MEM_RELEASE);
        CloseHandle(hFile);
        return false;
    }
    CloseHandle(hFile);

    // Parse PE to find .text section
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)buffer;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        VirtualFree(buffer, 0, MEM_RELEASE);
        return false;
    }

    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)(buffer + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        VirtualFree(buffer, 0, MEM_RELEASE);
        return false;
    }

    // Find .text section
    WORD numSections = nt->FileHeader.NumberOfSections;
    PIMAGE_SECTION_HEADER sec = IMAGE_FIRST_SECTION(nt);
    PIMAGE_SECTION_HEADER textSec = nullptr;

    for (WORD i = 0; i < numSections; i++) {
        if (memcmp(sec[i].Name, ".text", 5) == 0) {
            textSec = &sec[i];
            break;
        }
    }

    if (!textSec) {
        VirtualFree(buffer, 0, MEM_RELEASE);
        return false;
    }

    // CRC32 the .text section
    uint32_t computed = crc32(buffer + textSec->PointerToRawData,
                              textSec->SizeOfRawData);

    // Read the patched hash from the dedicated section (volatile read -
    // the compiler must load from memory, never substitute constants)
    uint64_t s1 = g_drsentinel[0];
    uint64_t s2 = g_drsentinel[1];

    VirtualFree(buffer, 0, MEM_RELEASE);

    // Sentinel still has magic values = build step skipped -> allow
    const uint64_t M1 = 0xD7A0D7D0D7A0D7D0ULL;
    const uint64_t M2 = 0xD7D0D7A0D7D0D7A0ULL;
    if (s1 == M1 || s2 == M2)
        return true;

    uint32_t h1 = (uint32_t)(s1 & 0xFFFFFFFF);
    uint32_t h2 = (uint32_t)(s2 & 0xFFFFFFFF);

    if (computed != h1 || h1 != h2) {
        g_tampered = true;
        return false;
    }

    return true;
}

} // namespace integrity
