#include "pch.h"
#include "core/strcrypt.h"
#include "core/hooks.h"
#include "core/cursor.h"
#include "core/config.h"
#include "render/renderer.h"
#include "render/menu.h"
#include "render/notifications.h"
#include "modules/module_manager.h"
#include "core/jvm_wrapper.h"
#include "sdk/minecraft.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

// ── In-Game Modul-/Hotkey-Liste ───────────────────────────────────────
// Zeigt links, welche Module aktiv (lila) oder per Hotkey gebunden (grau)
// sind. Nutzt NUR die Background-DrawList (kein ImGui::Begin) — derselbe
// sichere Render-Pfad wie das Title-Screen-Logo, kein Fenster-System.
static void renderModuleHud() {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) return;
    ImFont* f = Renderer::getHudFont();
    if (!f) f = ImGui::GetFont();
    if (!f) return;

    auto& mods = ModuleManager::getModules();
    float fs = 20.0f;
    float x = 8.0f, y = 150.0f;
    for (auto& m : mods) {
        if (!m || m->isHidden()) continue;
        if (!m->isEnabled() && m->getKeyBind() == 0) continue;

        std::string lbl = m->getName();
        if (m->getKeyBind()) {
            lbl += " [";
            lbl += vkName(m->getKeyBind());
            lbl += "]";
        }

        ImU32 col = m->isEnabled()
            ? IM_COL32(160, 100, 240, 255)   // lila = aktiv
            : IM_COL32(120, 128, 145, 200);  // grau = nur Hotkey

        // kleiner Schatten für Lesbarkeit
        dl->AddText(f, fs, ImVec2(x + 1.0f, y + 1.0f), IM_COL32(0, 0, 0, 170), lbl.c_str());
        dl->AddText(f, fs, ImVec2(x, y), col, lbl.c_str());
        y += fs + 4.0f;
    }
}

bool Hooks::init() {
    if (MH_Initialize() != MH_OK) return false;

    HMODULE opengl = GetModuleHandleA("opengl32.dll");
    if (!opengl) return false;

    void* pSwapBuffers = GetProcAddress(opengl, "wglSwapBuffers");
    if (!pSwapBuffers) return false;

    if (MH_CreateHook(pSwapBuffers, &hkWglSwapBuffers,
                       reinterpret_cast<void**>(&oWglSwapBuffers)) != MH_OK)
        return false;

    if (MH_EnableHook(pSwapBuffers) != MH_OK) return false;
    printf(STR_C("[Draxo] wglSwapBuffers hooked\n"));
    return true;
}

void Hooks::shutdown() {
    s_unloading = true;
    MH_DisableHook(MH_ALL_HOOKS);

    for (int i = 0; s_inHook.load() > 0 && i < 10000; i++)
        Sleep(5);

    MH_Uninitialize();

    if (s_hwnd && oWndProc) {
        SetWindowLongPtrA(s_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(oWndProc));
        oWndProc = nullptr;
    }

    Renderer::shutdown();
    s_initialized = false;
    s_hwnd = nullptr;
    printf(STR_C("[Draxo] Hooks cleaned up\n"));
}

BOOL WINAPI Hooks::hkWglSwapBuffers(HDC hdc) {
    static thread_local bool tl_inside = false;
    if (tl_inside) return oWglSwapBuffers(hdc);
    tl_inside = true;

    struct Guard {
        bool& f; std::atomic<int>& c;
        Guard(bool& x, std::atomic<int>& y) : f(x), c(y) { c++; }
        ~Guard() { c--; f = false; }
    } guard(tl_inside, s_inHook);

    if (s_unloading) return oWglSwapBuffers(hdc);

    // One-time init
    if (!s_initialized) {
        s_hwnd = WindowFromDC(hdc);
        if (s_hwnd) {
            LONG_PTR prev = SetWindowLongPtrA(s_hwnd, GWLP_WNDPROC,
                                              reinterpret_cast<LONG_PTR>(hkWndProc));
            if (prev != reinterpret_cast<LONG_PTR>(hkWndProc))
                oWndProc = reinterpret_cast<WNDPROC>(prev);
            Renderer::init(s_hwnd);
            s_initialized = true;
            printf(STR_C("[Draxo] Renderer initialized\n"));
        }
        return oWglSwapBuffers(hdc);
    }

    // Self-healing WNDPROC
    if (s_hwnd) {
        LONG_PTR cur = GetWindowLongPtrA(s_hwnd, GWLP_WNDPROC);
        if (cur != reinterpret_cast<LONG_PTR>(hkWndProc)) {
            LONG_PTR prev = SetWindowLongPtrA(s_hwnd, GWLP_WNDPROC,
                                              reinterpret_cast<LONG_PTR>(hkWndProc));
            if (prev != reinterpret_cast<LONG_PTR>(hkWndProc) && prev)
                oWndProc = reinterpret_cast<WNDPROC>(prev);
        }
    }

    // Update modules (lightweight JNI for tickAll)
    JNIEnv* env = JvmWrapper::getEnv();
    if (env) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        ModuleManager::tickAll(env);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }

    // Render ImGui
    Renderer::beginFrame();

    bool menuOpen = Menu::isVisible();
    if (menuOpen) Cursor::forceVisible();

    Menu::render();
    if (!menuOpen) {
        Notifications::render();
        Cursor::renderGameCursor();
    }
    Menu::renderWelcomeOverlay();
    ModuleManager::renderAll();

    // Leichtgewichtige Modul-/Hotkey-Liste (Background-DrawList, keine
    // ImGui-Fenster → kein Crash-Risiko wie beim vollen HUD-System).
    if (!menuOpen) {
        renderModuleHud();
    }

    // Title screen: big "DRAXO CLIENT" when no world/player loaded
    if (!menuOpen) {
        JNIEnv* env2 = JvmWrapper::getEnv();
        bool onTitle = false;
        if (env2) {
            jobject p = CMinecraft::getPlayer();
            if (!p) onTitle = true;
            else env2->DeleteLocalRef(p);
        }
        if (onTitle) {
            ImDrawList* bdl = ImGui::GetBackgroundDrawList();
            ImVec2 ds2 = ImGui::GetIO().DisplaySize;
            ImFont* f = Renderer::getBoldFont();
            if (!f) f = ImGui::GetFont();
            float fs = 40.0f;
            const char* txt = "DRAXO CLIENT";
            ImVec2 ts = f->CalcTextSizeA(fs, FLT_MAX, 0.0f, txt);
            float x = (ds2.x - ts.x) * 0.5f;
            float y = ds2.y * 0.05f;
            bdl->AddText(f, fs, ImVec2(x+3, y+3), IM_COL32(0,0,0,180), txt);
            bdl->AddText(f, fs, ImVec2(x, y), IM_COL32(160,100,240,255), txt);
        }
    }

    Renderer::endFrame();

    return oWglSwapBuffers(hdc);
}

LRESULT CALLBACK Hooks::hkWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    // Menu toggle
    if (msg == WM_KEYDOWN && (wp == VK_INSERT || wp == VK_RSHIFT) && !(lp & (1 << 30))) {
        Menu::toggle();
        return TRUE;
    }

    // Interface mode toggle
    if (msg == WM_KEYDOWN && !(lp & (1 << 30))) {
        int vk = static_cast<int>(wp);
        int ifKey = ModuleManager::interfaceToggleKey();
        if (ifKey && vk == ifKey) {
            int newMode = Module::isSimpleMode() ? 1 : 0;
            ModuleManager::setInterfaceMode(newMode);
            Notifications::push("Interface", newMode == 0 ? "Simple mode" : "Advanced mode");
            return TRUE;
        }
    }

    // Quick-Save Ctrl+S
    if (msg == WM_KEYDOWN && wp == 'S' && !(lp & (1 << 30))) {
        if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) && Menu::isVisible()) {
            ModuleManager::saveAllSettings();
            Config::save();
            Notifications::push("Config", "Saved (Ctrl+S)");
            return TRUE;
        }
    }

    // ImGui input
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp))
        return TRUE;

    // Module keybinds
    if (msg == WM_KEYDOWN)
        ModuleManager::handleKey(static_cast<int>(wp));

    // Mouse block when menu open
    if (Menu::isVisible() || Menu::isWelcomeOpen()) {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse || io.WantCaptureKeyboard) {
            switch (msg) {
                case WM_LBUTTONDOWN: case WM_LBUTTONUP:
                case WM_RBUTTONDOWN: case WM_RBUTTONUP:
                case WM_MBUTTONDOWN: case WM_MBUTTONUP:
                case WM_MOUSEWHEEL: case WM_MOUSEMOVE:
                    return TRUE;
            }
        }
    }

    return CallWindowProcA(oWndProc, hwnd, msg, wp, lp);
}
