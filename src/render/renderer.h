#pragma once
#include "pch.h"

/*
 * Renderer — ImGui + OpenGL3 bootstrap.
 *
 * Font Engine: pre-loads custom fonts (Inter, JetBrains Mono) at
 * multiple sizes so the user can change font size at runtime without
 * rebuilding the font atlas. Falls back to Windows system fonts.
 */

enum class FontFamily : int { Inter = 0, Arial = 1 };

class Renderer {
public:
    static void init(HWND hwnd);
    static void shutdown();
    static void beginFrame();
    static void endFrame();

    static bool isInitialized() { return s_initialized; }

    // ── Logo (draxo_logo.png neben der DLL) ────────────────────────
    static GLuint getLogoTex() { return s_logoTex; }
    static float getLogoW() { return s_logoW; }
    static float getLogoH() { return s_logoH; }

    // ── Font Engine ────────────────────────────────────────────────
    // Pre-loaded size slots: 12, 16, 20, 24, 28 (5 sizes — keeps atlas small)
    static constexpr int kFontSizes[] = {12, 16, 20, 24, 28};
    static constexpr int kFontSizeCount = 5;

    static ImFont* getBoldFont();
    static ImFont* getMonoFont();
    static ImFont* getHudFont();
    static float   getHudFontSize() { return s_hudFontSize; }

    // Runtime font size change (swaps io.FontDefault to nearest pre-loaded size)
    static int  getFontSize()       { return s_activeFontSize; }
    static void setFontSize(int px);
    static void setFontFamily(FontFamily fam);
    static FontFamily getFontFamily() { return s_fontFamily; }

private:
    static void loadFonts();
    static void loadLogo();
    static std::string fontPath(const char* ttfName);
    static ImFont* addFontAtSize(const char* ttfPath, float size);

    static inline bool s_initialized = false;
    static inline HGLRC s_glCtx = nullptr;
    static inline GLuint s_logoTex = 0;
    static inline float  s_logoW = 0;
    static inline float  s_logoH = 0;

    // Font family arrays [FontFamily][kFontSizeCount]
    static inline ImFont* s_fonts[2][5] = {};   // Inter, Arial at 5 sizes
    static inline ImFont* s_boldFonts[2][5] = {};
    static inline ImFont* s_monoFont  = nullptr;  // JetBrains Mono at 16px

    static inline int   s_activeFontSize = 20;
    static inline FontFamily s_fontFamily = FontFamily::Inter;

    // Legacy pointers (point into s_fonts at active size)
    static inline ImFont* s_boldFont  = nullptr;
    static inline ImFont* s_hudFont   = nullptr;
    static inline float   s_hudFontSize = 26.0f;
};
