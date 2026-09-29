#pragma once
#include "pch.h"

class Menu {
public:
    static void render();
    static void toggle();
    static void renderWelcomeOverlay();

    static bool isVisible()      { return s_visible; }
    static bool isWelcomeOpen()  { return s_welcomeOpen; }

    // HUD-Grid (für HUD::render, deaktiviert)
    static bool isHudEditMode();
    static bool hudShowGrid();
    static float hudGridSize();

private:
    static inline bool s_visible = false;
    static inline bool s_welcomeOpen = false;
};

const char* vkName(int vk);
