#include "pch.h"
#include "core/strcrypt.h"
#include "core/hooks.h"
#include "core/jvm_wrapper.h"
#include "modules/module_manager.h"

static HMODULE g_hModule = nullptr;

static void earlyLog(const char* msg) {
    wchar_t path[MAX_PATH] = {};
    HMODULE self = GetModuleHandleW(L"draxo.dll");
    if (!self) self = GetModuleHandleW(NULL);
    if (self) {
        DWORD n = GetModuleFileNameW(self, path, MAX_PATH);
        if (n) {
            std::wstring dir(path, n);
            auto slash = dir.find_last_of(L'\\/');
            if (slash != std::wstring::npos) {
                dir = dir.substr(0, slash + 1) + L"draxo_early.log";
                FILE* f = _wfopen(dir.c_str(), L"a");
                if (f) {
                    SYSTEMTIME st; GetLocalTime(&st);
                    fprintf(f, "%02d:%02d:%02d.%03d | %s\n",
                        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, msg);
                    fclose(f);
                }
            }
        }
    }
}

static DWORD WINAPI InitThread(LPVOID param) {
    earlyLog("InitThread: start");
    // Init JNI
    earlyLog("InitThread: calling JvmWrapper::init()");
    if (!JvmWrapper::init()) {
        earlyLog("InitThread: JvmWrapper::init FAILED — will unload in 3s");
        Sleep(3000);
        earlyLog("InitThread: calling FreeLibraryAndExitThread");
        FreeLibraryAndExitThread(g_hModule, 0);
        return 0;
    }
    earlyLog("InitThread: JvmWrapper::init OK");

    // Init modules
    earlyLog("InitThread: calling ModuleManager::init()");
    ModuleManager::init();
    earlyLog("InitThread: ModuleManager::init OK");

    // Init hooks
    earlyLog("InitThread: calling Hooks::init()");
    if (!Hooks::init()) {
        earlyLog("InitThread: Hooks::init FAILED");
        JvmWrapper::shutdown();
        Sleep(3000);
        FreeLibraryAndExitThread(g_hModule, 0);
        return 0;
    }
    earlyLog("InitThread: Hooks::init OK — READY");

    printf(STR_C("[Draxo] Ready\n"));

    // Wait for unload (DELETE key)
    while (!(GetAsyncKeyState(VK_DELETE) & 1))
        Sleep(100);

    Hooks::shutdown();
    ModuleManager::shutdown();
    JvmWrapper::shutdown();

    FreeLibraryAndExitThread(g_hModule, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        earlyLog("DllMain DLL_PROCESS_ATTACH");
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        HANDLE h = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
        if (h) {
            earlyLog("InitThread created OK");
            CloseHandle(h);
        } else {
            earlyLog("CreateThread FAILED");
        }
    } else if (reason == DLL_PROCESS_DETACH) {
        earlyLog("DllMain DLL_PROCESS_DETACH");
    }
    return TRUE;
}
