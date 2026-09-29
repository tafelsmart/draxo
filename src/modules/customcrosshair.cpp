#include "pch.h"
#include "core/strcrypt.h"
#include "modules/customcrosshair.h"
#include "render/esp_renderer.h"

CustomCrosshair::CustomCrosshair() : Module("CustomCrosshair", ModuleCategory::RENDER, 0,
    "Fully customizable crosshair — style, size, color, and gap. Drawn over vanilla.") {
    defineGroup("Style");
    defineMode("type",  "Type",  0, {"Cross (+)", "Circle", "Square", "Dot", "T-Shape", "X-Shape"});
    defineFloat("size",  "Size", 10.0f, 4.0f, 40.0f, "%.0fpx");
    defineFloat("gap",   "Gap",   4.0f, 0.0f, 20.0f, "%.0fpx");
    defineFloat("thick", "Thickness", 1.5f, 0.5f, 8.0f, "%.1fpx");
    defineBool("outline", "Outline", true);
    defineGroupEnd();

    defineGroup("Colors");
    defineColor("color",     "Color",       IM_COL32(255, 255, 255, 255));
    defineColor("outline_c", "Outline Col", IM_COL32(0, 0, 0, 200));
    defineGroupEnd();

    addSearchTag("crosshair");
    addSearchTag("aim");
    addSearchTag("cursor");
}

void CustomCrosshair::onRender() {
    if (!m_enabled) return;

    int type = m_intSettings["type"];
    float size  = m_floatSettings["size"];
    float gap   = m_floatSettings["gap"];
    float thick = m_floatSettings["thick"];
    bool outline = m_boolSettings["outline"];
    ImU32 color  = m_colorSettings["color"];
    ImU32 outCol = m_colorSettings["outline_c"];

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImVec2 disp = ImGui::GetIO().DisplaySize;
    float cx = disp.x * 0.5f;
    float cy = disp.y * 0.5f;

    float halfT = thick * 0.5f;

    auto drawRect = [&](float x, float y, float w, float h) {
        if (outline) {
            dl->AddRectFilled(ImVec2(x-1, y-1), ImVec2(x+w+1, y+h+1), outCol);
        }
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x+w, y+h), color);
    };

    switch (type) {
    case 0: { // Cross (+)
        // Horizontal
        drawRect(cx + gap, cy - halfT, size, thick);
        drawRect(cx - gap - size, cy - halfT, size, thick);
        // Vertical
        drawRect(cx - halfT, cy + gap, thick, size);
        drawRect(cx - halfT, cy - gap - size, thick, size);
        break;
    }
    case 1: { // Circle
        float r = size * 0.5f;
        int segs = 32;
        if (outline)
            dl->AddCircle(ImVec2(cx, cy), r + 1.0f, outCol, segs, thick + 2.0f);
        dl->AddCircle(ImVec2(cx, cy), r, color, segs, thick);
        break;
    }
    case 2: { // Square (hollow)
        float hs = size * 0.5f;
        float t2 = thick;
        // Top edge
        drawRect(cx - hs, cy - hs, size, t2);
        // Bottom edge
        drawRect(cx - hs, cy + hs - t2, size, t2);
        // Left edge
        drawRect(cx - hs, cy - hs, t2, size);
        // Right edge
        drawRect(cx + hs - t2, cy - hs, t2, size);
        break;
    }
    case 3: { // Dot
        float r = size * 0.35f;
        int segs = 16;
        if (outline)
            dl->AddCircleFilled(ImVec2(cx, cy), r + 1.5f, outCol, segs);
        dl->AddCircleFilled(ImVec2(cx, cy), r, color, segs);
        break;
    }
    case 4: { // T-Shape (only top + bottom)
        drawRect(cx - halfT, cy + gap, thick, size);
        drawRect(cx - halfT, cy - gap - size, thick, size);
        break;
    }
    case 5: { // X-Shape (diagonal lines)
        // Use rotated rectangles — approximate with AddLine
        float hs = size * 0.5f;
        if (outline) {
            dl->AddLine(ImVec2(cx-hs, cy-hs), ImVec2(cx+hs, cy+hs), outCol, thick+2);
            dl->AddLine(ImVec2(cx+hs, cy-hs), ImVec2(cx-hs, cy+hs), outCol, thick+2);
        }
        dl->AddLine(ImVec2(cx-hs, cy-hs), ImVec2(cx+hs, cy+hs), color, thick);
        dl->AddLine(ImVec2(cx+hs, cy-hs), ImVec2(cx-hs, cy+hs), color, thick);
        break;
    }
    }
}
