#pragma once
#include <string>
#include <fstream>
#include <Windows.h>

/* DraxoVersion — Reads the VERSION file from beside the DLL at runtime.
   On failure, returns a sensible default so the UI never breaks. */

inline std::string draxoVersion() {
    static std::string s_version;
    if (!s_version.empty()) return s_version;

    // Get DLL directory
    wchar_t path[MAX_PATH];
    HMODULE self = GetModuleHandleW(L"draxo.dll");
    if (!self) { s_version = "v1.0"; return s_version; }
    DWORD n = GetModuleFileNameW(self, path, MAX_PATH);
    if (!n) { s_version = "v1.0"; return s_version; }

    std::wstring ws(path, n);
    auto slash = ws.find_last_of(L"\\/");
    if (slash == std::wstring::npos) { s_version = "v1.0"; return s_version; }
    ws = ws.substr(0, slash + 1);

    std::string vpath;
    // wchar_t->char sauber konvertieren (WideCharToMultiByte) statt
    // wchar-truncation — verhindert Mojibake und C4244-Warnungen.
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(),
                                  nullptr, 0, nullptr, nullptr);
    if (len > 0) {
        vpath.resize(len);
        WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(),
                            vpath.data(), len, nullptr, nullptr);
    }
    vpath += "VERSION";
    std::ifstream vf(vpath);
    if (vf.is_open()) {
        std::string v;
        std::getline(vf, v);
        if (!v.empty()) s_version = "v" + v;
    }
    if (s_version.empty()) s_version = "v1.0";
    return s_version;
}
