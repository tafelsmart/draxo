#pragma once
#include "pch.h"
#include <string>

/*
 * Theme — Centralized visual style for the entire client.
 * Every UI element reads its colors/values from here.
 * Themes can be saved/loaded/imported/exported via Config.
 */

struct DraxoTheme {
    // ── Colors — Lila Glass Design ──────────────────────────────────
    ImVec4 accent      = ImVec4(0.63f, 0.42f, 0.95f, 1.00f); // Primary lila
    ImVec4 accentHover = ImVec4(0.75f, 0.55f, 1.00f, 1.00f);
    ImVec4 accentDim   = ImVec4(0.40f, 0.25f, 0.70f, 1.00f);

    ImVec4 bgPanel     = ImVec4(0.06f, 0.03f, 0.10f, 0.85f); // Dunkel-lila, transparent
    ImVec4 bgContent   = ImVec4(0.08f, 0.04f, 0.13f, 0.70f); // Content, noch transparenter
    ImVec4 bgCard      = ImVec4(0.10f, 0.05f, 0.15f, 0.60f); // Module cards
    ImVec4 bgSidebar   = ImVec4(0.04f, 0.02f, 0.08f, 0.80f); // Sidebar

    ImVec4 textPrimary   = ImVec4(0.93f, 0.90f, 0.97f, 1.00f);
    ImVec4 textSecondary = ImVec4(0.65f, 0.60f, 0.75f, 1.00f);
    ImVec4 textMuted     = ImVec4(0.45f, 0.40f, 0.55f, 1.00f);

    ImVec4 border      = ImVec4(0.55f, 0.40f, 0.90f, 0.35f);   // Lila border
    ImVec4 separator   = ImVec4(0.40f, 0.30f, 0.65f, 0.40f);

    ImVec4 scrollbar   = ImVec4(0.40f, 0.30f, 0.65f, 0.45f);
    ImVec4 scrollbarHover = ImVec4(0.55f, 0.42f, 0.85f, 0.60f);

    // ── UI Properties — Smooth & Round ──────────────────────────────
    float blurAmount     = 0.25f;  // Light background blur
    float glowAmount     = 0.15f;  // Subtle accent glow
    float shadowAmount   = 0.40f;  // Drop shadow strength
    float windowRounding = 12.0f;  // Big rounded corners
    float frameRounding  = 6.0f;   // Button/slider radius
    float popupRounding  = 8.0f;   // Dropdown radius
    float scrollRounding = 6.0f;

    // ── Fonts ───────────────────────────────────────────────────────
    int   fontSize   = 18;
    float fontSpacing = 0.0f;
    bool  fontBold   = false;

    // ── Spacing ─────────────────────────────────────────────────────
    float itemSpacingX  = 10.0f;
    float itemSpacingY  = 6.0f;
    float framePaddingX = 10.0f;
    float framePaddingY = 5.0f;
    float windowPaddingX = 16.0f;
    float windowPaddingY = 12.0f;

    // ── Name ────────────────────────────────────────────────────────
    std::string name = "Draxo Lila Glass";
};

class ThemeEngine {
public:
    static void init();
    static void applyToImGui();                    // Apply theme to ImGuiStyle
    static void renderConfigUI();                  // Theme editor in CONFIG tab

    // Save/Load
    static void saveToConfig();
    static void loadFromConfig();
    static void resetToDefaults();

    // Access current theme
    static DraxoTheme& current() { return s_theme; }
    static const DraxoTheme& get() { return s_theme; }

private:
    static inline DraxoTheme s_theme;
    static inline DraxoTheme s_defaults;
};
