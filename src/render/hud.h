#pragma once
#include "pch.h"
#include <vector>
#include <unordered_map>

/*
 * HUD — Premium Overlay System (v6)
 *
 * Layout philosophy:
 *   TOP LEFT    → Client info (DRAXO, FPS, Ping, TPS) — compact, right-aligned numbers
 *   TOP CENTER  → Clock only
 *   TOP RIGHT   → Server info (IP, Players, Gamemode)
 *   LEFT MID    → ArrayList (active modules + hotkeys)
 *   LEFT BOTTOM → World info (XYZ, Direction, Biome)
 *   RIGHT       → TargetHUD (separate system), Notifications
 *   BOTTOM RIGHT→ Session stats (Time, Kills, Deaths, KD)
 *
 * Each element can optionally render as a "panel" (blur, shadow, rounded, accent border).
 * All panels share the same styling for visual consistency.
 */

struct HudElement {
    const char* id;         // Unique window ID
    const char* label;      // Display name for menu
    const char* section;    // Section label for menu grouping

    bool  visible = true;

    // Position (screen pixels from top-left)
    float posX = 10, posY = 10;
    float scale = 1.0f;

    // Colors
    ImU32 textColor   = IM_COL32(235, 238, 245, 255);  // 90% white
    ImU32 bgColor     = IM_COL32(12, 14, 20, 160);      // Dark translucent
    ImU32 accentColor = IM_COL32(80, 160, 255, 255);    // Draxo blue

    // Panel settings (shared across all panels for consistency)
    bool  panelMode     = false;   // Render as panel (bg + border + shadow)
    float panelRounding = 8.0f;    // Corner radius
    float panelAlpha    = 0.55f;   // Background opacity
    bool  panelShadow   = true;    // Drop shadow
    bool  panelBorder   = true;    // Accent border left side

    // Animation state
    float animAlpha  = 0.0f;
    float animScale  = 1.0f;
};

class HUD {
public:
    static void render();
    static void renderConfigUI();  // Called from menu CONFIG tab
    static void loadConfig();
    static void saveConfig();

    static HudElement* getElement(const char* id);
    static std::vector<HudElement>& getElements() { return s_elements; }

    // ── Global panel defaults (applied to all elements with panelMode) ──
    static float panelRounding() { return s_panelRounding; }
    static float panelAlpha()    { return s_panelAlpha; }
    static bool  panelShadow()   { return s_panelShadow; }
    static bool  panelBorder()   { return s_panelBorder; }

private:
    // ── Element renderers ───────────────────────────────────────────
    static void renderWatermark();   // TOP LEFT  — DRAXO + version
    static void renderFPS();         // TOP LEFT  — FPS counter
    static void renderPing();        // TOP LEFT  — Ping (color-coded)
    static void renderTPS();         // TOP LEFT  — TPS
    static void renderClock();       // TOP CENTER
    static void renderServerIP();    // TOP RIGHT — Server IP
    static void renderPlayerCount(); // TOP RIGHT — Player count
    static void renderArrayList();   // LEFT MID  — Active modules
    static void renderCoords();      // LEFT BOT  — XYZ + Direction + Biome
    static void renderKeystrokes();  // LEFT BOT  — WASD keys
    static void renderKeybindList(); // LEFT BOT  — Configured hotkeys
    static void renderCPS();         // LEFT BOT  — CPS counter
    static void renderCombo();       // LEFT BOT  — Combo counter
    static void renderDirection();   // LEFT BOT  — Compass direction
    static void renderSession();     // BOT RIGHT — Playtime + K/D

    // ── Helpers ─────────────────────────────────────────────────────
    static bool beginElement(HudElement& el, float w, float h);
    static void endElement();
    static void drawPanel(const HudElement& el, float w, float h);

    // ── FPS / CPS tracking ──────────────────────────────────────────
    static inline float s_fpsSmooth = 0;
    static inline int s_realFPS = 0, s_frameCount = 0;
    static inline std::chrono::steady_clock::time_point s_lastFPS;
    static inline int s_leftClicks = 0, s_realCPS = 0;
    static inline float s_cpsSmooth = 0;

    // ── Session tracking ────────────────────────────────────────────
    static inline int s_comboCount = 0;
    static inline int s_kills = 0, s_deaths = 0;
    static inline std::chrono::steady_clock::time_point s_sessionStart;

    // ── Panel defaults ──────────────────────────────────────────────
    static inline float s_panelRounding = 8.0f;
    static inline float s_panelAlpha    = 0.55f;
    static inline bool  s_panelShadow   = true;
    static inline bool  s_panelBorder   = true;

    // ── Element registry ────────────────────────────────────────────
    static inline std::vector<HudElement> s_elements;
};
