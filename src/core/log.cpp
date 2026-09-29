#include "pch.h"
#include "core/log.h"
#include <cstdio>
#include <mutex>
#include <deque>
#include <string>
#include <vector>

namespace {
    std::mutex s_mutex;
    std::deque<std::string> s_lines;
    FILE* s_file = nullptr;
    std::string s_filePath;
    constexpr int MAX_LINES = 300;
}

void Log::init() {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_file) return;

    // Log-Datei NEBEN der DLL (draxo_client.log), Pfad relativ zur DLL auflösen
    wchar_t path[MAX_PATH] = {};
    HMODULE self = GetModuleHandleW(L"draxo.dll");
    if (!self) self = GetModuleHandleW(L"null.dll");  // fallback alter Name
    if (self) {
        DWORD n = GetModuleFileNameW(self, path, MAX_PATH);
        if (n) {
            std::wstring dir(path, n);
            auto slash = dir.find_last_of(L"\\/");
            if (slash != std::wstring::npos) {
                dir = dir.substr(0, slash + 1) + L"draxo_client.log";
                // UTF-8 für die Anzeige im Debug-Fenster
                int len = WideCharToMultiByte(CP_UTF8, 0, dir.c_str(), -1, nullptr, 0, nullptr, nullptr);
                if (len > 0) {
                    s_filePath.resize(len - 1);
                    WideCharToMultiByte(CP_UTF8, 0, dir.c_str(), -1, s_filePath.data(), len, nullptr, nullptr);
                }
            }
        }
    }
    if (s_filePath.empty()) s_filePath = "draxo_client.log";

    s_file = fopen(s_filePath.c_str(), "a");
    if (s_file) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fprintf(s_file, "\n===== DRAXO CLIENT %04d-%02d-%02d %02d:%02d:%02d =====\n",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        fflush(s_file);
    }
}

void Log::shutdown() {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_file) {
        fprintf(s_file, "===== ENDE =====\n");
        fflush(s_file);
        fclose(s_file);
        s_file = nullptr;
    }
}

void Log::write(const char* fmt, ...) {
    if (!fmt) return;
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    // Konsole (sichtbar bei Start via java.exe statt javaw.exe)
    fputs(buf, stdout);

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_lines.push_back(buf);
        while ((int)s_lines.size() > MAX_LINES) s_lines.pop_front();

        if (s_file) {
            fputs(buf, s_file);
            fflush(s_file);  // sofort schreiben, damit die Datei live lesbar bleibt
        }
    }
}

int Log::snapshot(char* out, int maxBytes) {
    if (!out || maxBytes <= 0) return 0;
    std::lock_guard<std::mutex> lock(s_mutex);
    int written = 0;
    out[0] = '\0';
    for (const auto& line : s_lines) {
        int needed = snprintf(out + written, maxBytes - written, "%s", line.c_str());
        if (needed < 0 || written + needed >= maxBytes) break;
        written += needed;
    }
    return written;
}

int Log::count() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return (int)s_lines.size();
}

void Log::clear() {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_lines.clear();
}

const char* Log::filePath() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_filePath.c_str();
}
