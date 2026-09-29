#pragma once
#include "pch.h"

/*
 * Cursor — zentrale Windows-Cursor-Sichtbarkeit (ShowCursor-Anzeigezähler).
 *
 * ShowCursor arbeitet mit einem Zähler: sichtbar ab >= 0, versteckt bei < 0.
 * Die while-Schleifen erzwingen den gewünschten Zustand, egal wie oft das
 * Spiel den Zähler bewegt hat.
 *
 * Zusätzlich wird ein eigener, gezeichneter Mauszeiger im Spiel angeboten
 * („Custom Cursor in Game“), weil Minecraft den OS-Cursor im Gameplay
 * versteckt — ohne ihn sieht man die Maus nur im Menü.
 */
namespace Cursor {
    // Zähler hochziehen, bis der Cursor sichtbar ist (>= 0)
    inline void forceVisible() {
        while (ShowCursor(TRUE) < 0) {}
    }

    // Zähler runterziehen, bis der Cursor versteckt ist (< 0)
    inline void forceHidden() {
        while (ShowCursor(FALSE) >= 0) {}
    }

    // Einstellung: eigener Mauszeiger im Spiel (an/aus, per Config persistiert)
    inline bool& enabled() {
        static bool s_enabled = true;
        return s_enabled;
    }

    // Zeichnet einen kleinen Pfeil an die OS-Mausposition. Wird jeden Frame
    // aus dem Render-Hook gerufen, wenn das Menü zu ist und enabled() gilt.
    inline void renderGameCursor() {
        if (!enabled()) return;

        // OS-Mausposition (das Spiel liefert uns keine MousePos, weil es die
        // Maus captured — wir lesen die echte Windows-Position).
        POINT pt;
        if (!GetCursorPos(&pt)) return;

        // HWND cachen statt jeden Frame FindWindowA (Fenster-Enumeration).
        static HWND s_hwnd = FindWindowA("GLFW30", nullptr);
        if (!s_hwnd) s_hwnd = FindWindowA("GLFW30", nullptr);
        if (!s_hwnd) return;
        RECT rc;
        if (!GetClientRect(s_hwnd, &rc) || rc.right <= 0 || rc.bottom <= 0) return;
        ScreenToClient(s_hwnd, &pt);
        if (pt.x < 0 || pt.y < 0 || pt.x > rc.right || pt.y > rc.bottom) return;

        ImDrawList* dl = ImGui::GetForegroundDrawList();
        float x = (float)pt.x, y = (float)pt.y;
        const float s = 1.35f;  // etwas größer als 16px-Font → gut sichtbar

        // Pfeil-Spitze (weiß mit dunklem Outline) — moderner Look
        ImVec2 tip(x, y);
        ImVec2 right(x + 11 * s, y + 8 * s);
        ImVec2 bottom(x + 7 * s, y + 11 * s);
        ImVec2 tailL(x + 8 * s, y + 13 * s);
        ImVec2 tailT(x + 4 * s, y + 9 * s);
        ImVec2 tailB(x + 2 * s, y + 14 * s);

        // Körper: zwei Dreiecke (Spitze + Schwanz) als geschlossenes Polygon
        dl->AddTriangleFilled(tip, right, bottom, IM_COL32(255,255,255,235));
        dl->AddTriangleFilled(bottom, tailL, tailT, IM_COL32(255,255,255,235));
        dl->AddTriangleFilled(tailT, tailL, tailB, IM_COL32(255,255,255,235));
        // Outline
        dl->AddTriangle(tip, right, bottom, IM_COL32(20,22,28,200), 1.4f);
        dl->AddTriangle(bottom, tailL, tailB, IM_COL32(20,22,28,200), 1.4f);
        dl->AddTriangle(tailL, tailB, tailT, IM_COL32(20,22,28,200), 1.4f);

        // Kleiner Akzent-Punkt an der Spitze
        dl->AddCircleFilled(tip, 2.2f * s, IM_COL32(80,160,255,230));
    }
}
