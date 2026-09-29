#pragma once
#include "pch.h"
#include <vector>
#include <deque>
#include <chrono>
#include <string>

/*
 * Notifications — Animated popup notification system.
 * Supports 5 styles, configurable position/animation, auto-dismiss.
 */

enum class NotifStyle : int {
    Modern,   // Dark glass, blue accent, rounded
    Windows,  // Sharp corners, title bar, white bg
    macOS,    // Translucent, soft shadow, SF-style
    Material, // Elevated card, bold text, ripple accent
    Minimal   // Bare text, thin border, no bg
};
inline const char* notifStyleName(NotifStyle s) {
    switch(s) {
        case NotifStyle::Modern: return "Modern";
        case NotifStyle::Windows: return "Windows";
        case NotifStyle::macOS: return "macOS";
        case NotifStyle::Material: return "Material";
        case NotifStyle::Minimal: return "Minimal";
        default: return "Modern";
    }
}

enum class NotifAnim : int {
    SlideRight,  // Slide in from left edge
    SlideUp,     // Slide in from bottom
    Fade,        // Pure fade
    Pop          // Scale bounce
};
inline const char* notifAnimName(NotifAnim a) {
    switch(a) {
        case NotifAnim::SlideRight: return "Slide Right";
        case NotifAnim::SlideUp: return "Slide Up";
        case NotifAnim::Fade: return "Fade";
        case NotifAnim::Pop: return "Pop";
        default: return "Slide Right";
    }
}

struct Notification {
    std::string title;
    std::string message;
    std::chrono::steady_clock::time_point createdAt;
    float animProgress = 0.0f; // 0=hidden, 1=fully visible
    bool dismissing = false;
};

class Notifications {
public:
    // Push a notification (e.g., "Module Toggled", "Reach ON")
    static void push(const std::string& title, const std::string& msg);
    // Called each frame to render all active notifications
    static void render();
    // Called from CONFIG tab to show settings UI
    static void renderConfigUI();
    // Save/load
    static void saveConfig();
    static void loadConfig();

    // Settings accessors
    static NotifStyle& style()      { return s_style; }
    static NotifAnim&  animation()  { return s_anim; }
    static float& posX()  { return s_posX; }
    static float& posY()  { return s_posY; }
    static float& duration()   { return s_duration; }
    static float& animSpeed()  { return s_animSpeed; }
    static float& maxWidth()   { return s_maxWidth; }
    static int&   maxVisible() { return s_maxVisible; }

private:
    static void renderModern(const Notification& n, float alpha, float x, float y);
    static void renderWindows(const Notification& n, float alpha, float x, float y);
    static void renderMacOS(const Notification& n, float alpha, float x, float y);
    static void renderMaterial(const Notification& n, float alpha, float x, float y);
    static void renderMinimal(const Notification& n, float alpha, float x, float y);

    static inline std::deque<Notification> s_queue;
    static inline NotifStyle s_style      = NotifStyle::Modern;
    static inline NotifAnim  s_anim       = NotifAnim::SlideRight;
    // Standard: unten links (s_posY == 0 => automatisch am unteren Rand)
    static inline float s_posX  = 20, s_posY = 0;
    static inline float s_duration  = 3.0f;
    static inline float s_animSpeed = 0.15f;
    static inline float s_maxWidth  = 280.0f;
    static inline int   s_maxVisible = 6;
    static inline float s_notifHeight = 50.0f;
};
