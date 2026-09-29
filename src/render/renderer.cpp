#include "pch.h"
#include "core/strcrypt.h"
#include "render/renderer.h"
#include "render/theme.h"
#include "core/config.h"
#include <GL/gl.h>
// WIN32_LEAN_AND_MEAN schließt IStream/PROPID aus — GDI+ braucht sie.
#include <objidl.h>
#include <propidl.h>
#include <gdiplus.h>
#include <string>

#pragma comment(lib, "gdiplus.lib")

// Windows gl.h ist GL 1.1 — GL_CLAMP_TO_EDGE fehlt dort
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace {
    ULONG_PTR g_gdiplusToken = 0;
}

void Renderer::init(HWND hwnd) {
    if (s_initialized) return;

    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // KRITISCH: Ohne dieses Flag ruft ImGui_ImplWin32_NewFrame() jeden Frame
    // ShowCursor(TRUE) auf. Der Zähler läuft hoch und der Mauszeiger bleibt
    // am Fadenkreuz sichtbar. Mit dem Flag steuern wir den Cursor selbst
    // (Menu::toggle macht das sauber).
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.IniFilename = nullptr;  // Don't save layout to disk

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplOpenGL3_Init("#version 130");

    // GDI+ für Logo-PNG
    Gdiplus::GdiplusStartupInput gsi;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &gsi, nullptr);

    // KRITISCH: Fonts VOR dem ersten Frame laden (in init, nicht lazy im
    // Menü). Nachträgliches AddFontFromFileTTF baut die Font-Atlas-Textur
    // mitten im Rendering neu → Glyphen rendern als weiße Punkte.
    loadFonts();

    ThemeEngine::init();
    ThemeEngine::applyToImGui();
    loadLogo();
    s_initialized = true;
}

void Renderer::shutdown() {
    if (!s_initialized) return;

    if (s_logoTex) {
        glDeleteTextures(1, &s_logoTex);
        s_logoTex = 0;
    }
    s_logoW = s_logoH = 0;

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (g_gdiplusToken) {
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }

    s_glCtx = nullptr;
    s_initialized = false;
}

void Renderer::loadLogo() {
    // Logo liegt als draxo_logo.png NEBEN der DLL (vom CMake-Build kopiert).
    wchar_t path[MAX_PATH] = {};
    HMODULE self = GetModuleHandleW(L"draxo.dll");
    if (!self) self = GetModuleHandleW(L"null.dll");
    if (!self) return;
    DWORD n = GetModuleFileNameW(self, path, MAX_PATH);
    if (!n) return;

    std::wstring dir(path, n);
    auto slash = dir.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return;
    dir = dir.substr(0, slash + 1);
    std::wstring pngPath = dir + L"draxo_logo.png";

    Gdiplus::Bitmap bmp(pngPath.c_str());
    if (bmp.GetLastStatus() != Gdiplus::Ok) {
        printf(STR_C("[Draxo] Logo nicht gefunden: %ls\n"), pngPath.c_str());
        return;
    }

    UINT w = bmp.GetWidth();
    UINT h = bmp.GetHeight();
    if (w == 0 || h == 0) return;

    Gdiplus::Rect rc(0, 0, (INT)w, (INT)h);
    Gdiplus::BitmapData bd;
    if (bmp.LockBits(&rc, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bd) != Gdiplus::Ok) {
        return;
    }

    std::vector<unsigned char> rgba(w * h * 4);
    const unsigned char* src = (const unsigned char*)bd.Scan0;
    for (UINT y = 0; y < h; y++) {
        const unsigned char* row = src + (size_t)y * bd.Stride;
        unsigned char* dst = &rgba[(size_t)y * w * 4];
        for (UINT x = 0; x < w; x++) {
            dst[x * 4 + 0] = row[x * 4 + 2]; // B -> R
            dst[x * 4 + 1] = row[x * 4 + 1]; // G
            dst[x * 4 + 2] = row[x * 4 + 0]; // R -> B
            dst[x * 4 + 3] = row[x * 4 + 3]; // A
        }
    }
    bmp.UnlockBits(&bd);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

    glGenTextures(1, &s_logoTex);
    glBindTexture(GL_TEXTURE_2D, s_logoTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)w, (GLsizei)h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    s_logoW = (float)w;
    s_logoH = (float)h;
    printf(STR_C("[Draxo] Logo geladen: %ux%u\n"), w, h);
}

void Renderer::beginFrame() {
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

    HGLRC curCtx = wglGetCurrentContext();
    if (curCtx && s_glCtx && curCtx != s_glCtx) {
        printf(STR_C("[Draxo] GL-Kontext gewechselt (%p -> %p), baue Texturen neu\n"),
               (void*)s_glCtx, (void*)curCtx);
        ImGui_ImplOpenGL3_DestroyDeviceObjects();
        if (!ImGui_ImplOpenGL3_CreateDeviceObjects())
            printf(STR_C("[Draxo] WARNUNG: Shader-Neuaufbau nach Kontextwechsel fehlgeschlagen\n"));
        if (s_logoTex) {
            glDeleteTextures(1, &s_logoTex);
            s_logoTex = 0;
        }
        s_logoW = s_logoH = 0;
        loadLogo();
    }
    s_glCtx = curCtx;

    glPushAttrib(GL_ALL_ATTRIB_BITS);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void Renderer::endFrame() {
    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    glPopAttrib();
}

// ── Font path resolver ──────────────────────────────────────────────
std::string Renderer::fontPath(const char* ttfName) {
    wchar_t path[MAX_PATH] = {};
    HMODULE self = GetModuleHandleW(L"draxo.dll");
    if (!self) return std::string("C:\\Windows\\Fonts\\") + ttfName;
    DWORD n = GetModuleFileNameW(self, path, MAX_PATH);
    if (!n) return std::string("C:\\Windows\\Fonts\\") + ttfName;
    std::wstring wpath(path, n);
    auto slash = wpath.find_last_of(L"\\/");
    if (slash != std::wstring::npos) wpath = wpath.substr(0, slash + 1);
    int len = WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, &result[0], len, nullptr, nullptr);
    while (!result.empty() && result.back() == '\0') result.pop_back();
    // Try assets/fonts/ beside DLL first
    std::string direct = result + "assets/fonts/" + ttfName;
    DWORD attr = GetFileAttributesA(direct.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
        return direct;
    // Fallback: Windows system font
    return std::string("C:\\Windows\\Fonts\\") + ttfName;
}

ImFont* Renderer::addFontAtSize(const char* ttfPath, float size) {
    ImFont* f = ImGui::GetIO().Fonts->AddFontFromFileTTF(ttfPath, size);
    if (f) return f;
    return ImGui::GetIO().Fonts->AddFontDefault();
}

void Renderer::loadFonts() {
    ImGuiIO& io = ImGui::GetIO();

    // ── Custom font paths (assets/fonts/ beside DLL, or fallback to Windows) ──
    std::string interReg   = fontPath("Inter-Regular.ttf");
    std::string interBold  = fontPath("Inter-Bold.ttf");
    std::string jmReg      = fontPath("JetBrainsMono-Regular.ttf");
    std::string jmBold     = fontPath("JetBrainsMono-Bold.ttf");
    // Windows fallback paths
    std::string arial      = "C:\\Windows\\Fonts\\arial.ttf";
    std::string arialBold  = "C:\\Windows\\Fonts\\arialbd.ttf";
    std::string consola    = "C:\\Windows\\Fonts\\consola.ttf";

    // Probe: try loading Inter Regular at 20px to check availability
    // Load directly into the pre-load slot — no wasted test entries.
    s_fonts[0][2] = io.Fonts->AddFontFromFileTTF(interReg.c_str(), 20.0f);
    s_boldFonts[0][2] = io.Fonts->AddFontFromFileTTF(interBold.c_str(), 20.0f);
    bool haveInter = (s_fonts[0][2] != nullptr && s_boldFonts[0][2] != nullptr);

    // Probe Arial
    s_fonts[1][2] = io.Fonts->AddFontFromFileTTF(arial.c_str(), 20.0f);
    s_boldFonts[1][2] = io.Fonts->AddFontFromFileTTF(arialBold.c_str(), 20.0f);
    bool haveArial = (s_fonts[1][2] != nullptr);

    printf(STR_C("[Draxo] Font Engine: Inter=%s Arial=%s\n"),
           haveInter ? "YES" : "NO", haveArial ? "YES" : "NO");

    // Pre-load font family 0 (Inter or Arial) at remaining 8 sizes
    int famIdx = haveInter ? 0 : 1;
    const char* regPath = haveInter ? interReg.c_str() : arial.c_str();
    const char* boldPath= haveInter ? interBold.c_str() : arialBold.c_str();

    for (int i = 0; i < kFontSizeCount; i++) {
        if (i == 2) continue; // already probed at 20px
        float sz = (float)kFontSizes[i];
        s_fonts[famIdx][i] = addFontAtSize(regPath, sz);
        s_boldFonts[famIdx][i] = addFontAtSize(boldPath, sz);
    }

    // If we have Inter, also pre-load Arial at remaining sizes (for switching)
    if (haveInter && haveArial) {
        for (int i = 0; i < kFontSizeCount; i++) {
            if (i == 2) continue; // already probed
            float sz = (float)kFontSizes[i];
            s_fonts[1][i] = addFontAtSize(arial.c_str(), sz);
            s_boldFonts[1][i] = addFontAtSize(arialBold.c_str(), sz);
        }
    } else if (!haveArial) {
        // No Arial — copy Inter slots to Arial slots
        for (int i = 0; i < kFontSizeCount; i++) {
            if (i == 2) continue;
            s_fonts[1][i] = s_fonts[0][i];
            s_boldFonts[1][i] = s_boldFonts[0][i];
        }
    }

    // Monospace: JetBrains Mono at 16px (debug overlay / console)
    s_monoFont = io.Fonts->AddFontFromFileTTF(jmReg.c_str(), 16.0f);
    if (!s_monoFont)
        s_monoFont = io.Fonts->AddFontFromFileTTF(consola.c_str(), 16.0f);
    if (!s_monoFont)
        s_monoFont = io.Fonts->AddFontDefault();

    // ── Apply active font size (from config) ───────────────────────
    s_activeFontSize = Config::getInt("Font", "size", 20);
    if (s_activeFontSize < 12) s_activeFontSize = 12;
    if (s_activeFontSize > 28) s_activeFontSize = 28;
    int fam = Config::getInt("Font", "family", (int)FontFamily::Inter);
    if (fam < 0 || fam > 1) fam = haveInter ? 0 : 1;
    s_fontFamily = (FontFamily)fam;

    setFontSize(s_activeFontSize);  // swaps io.FontDefault + s_boldFont

    // HUD-Font: 24px from active family
    int hudIdx = 3; // 24px = index 3
    s_hudFontSize = 24.0f;
    s_hudFont = s_fonts[fam][hudIdx];
    if (!s_hudFont) s_hudFont = io.Fonts->AddFontDefault();

    printf(STR_C("[Draxo] Font Engine ready: size=%dpx family=%s hud=%p\n"),
           s_activeFontSize, fam == 0 ? "Inter" : "Arial", (void*)s_hudFont);
}

// ── Runtime font size swap (changes io.FontDefault immediately) ─────
void Renderer::setFontSize(int px) {
    int best = 0, bestDist = 999;
    for (int i = 0; i < kFontSizeCount; i++) {
        int d = abs(kFontSizes[i] - px);
        if (d < bestDist) { bestDist = d; best = i; }
    }
    s_activeFontSize = kFontSizes[best];
    int fam = (int)s_fontFamily;
    ImFont* newDef = s_fonts[fam][best];
    if (newDef) {
        ImGui::GetIO().FontDefault = newDef;
        s_boldFont = s_boldFonts[fam][best];
    }
    Config::setInt("Font", "size", s_activeFontSize);
}

void Renderer::setFontFamily(FontFamily fam) {
    s_fontFamily = fam;
    int best = 0, bestDist = 999;
    for (int i = 0; i < kFontSizeCount; i++) {
        int d = abs(kFontSizes[i] - s_activeFontSize);
        if (d < bestDist) { bestDist = d; best = i; }
    }
    int fi = (int)fam;
    ImFont* newDef = s_fonts[fi][best];
    if (newDef) {
        ImGui::GetIO().FontDefault = newDef;
        s_boldFont = s_boldFonts[fi][best];
        s_activeFontSize = kFontSizes[best];
    }
    s_hudFont = s_fonts[fi][3]; // 24px
    Config::setInt("Font", "family", (int)fam);
}

ImFont* Renderer::getBoldFont() {
    if (s_boldFont) return s_boldFont;
    return ImGui::GetIO().FontDefault;
}
ImFont* Renderer::getMonoFont() {
    if (s_monoFont) return s_monoFont;
    return ImGui::GetIO().FontDefault;
}
ImFont* Renderer::getHudFont() {
    if (s_hudFont) return s_hudFont;
    return ImGui::GetIO().FontDefault;
}
