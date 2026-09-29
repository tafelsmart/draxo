#pragma once
#include "pch.h"

/*
 * Hooks — MinHook-based wglSwapBuffers + WNDPROC hooking.
 * Minimal version — no overlay, no stream clean, no complexity.
 */
class Hooks {
public:
    static bool init();
    static void shutdown();

private:
    using wglSwapBuffers_t = BOOL(WINAPI*)(HDC);
    static BOOL WINAPI hkWglSwapBuffers(HDC hdc);
    static inline wglSwapBuffers_t oWglSwapBuffers = nullptr;

    static LRESULT CALLBACK hkWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    static inline WNDPROC oWndProc = nullptr;

    static inline bool s_initialized = false;
    static inline HWND  s_hwnd       = nullptr;

    static inline std::atomic<int>  s_inHook    = 0;
    static inline std::atomic<bool> s_unloading = false;
};
