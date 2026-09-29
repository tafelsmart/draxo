#include "pch.h"
#include "core/strcrypt.h"
#include "modules/chunkborders.h"
#include "sdk/minecraft.h"
#include "render/esp_renderer.h"

ChunkBorders::ChunkBorders() : Module("ChunkBorders", ModuleCategory::RENDER, 0,
    "Draws chunk boundary grid lines. Useful for farms and slime chunks.") {
    defineFloat("height", "Render height", 2.0f, 0.0f, 10.0f, "%.0f blocks");
    defineFloat("range",  "Chunk range",   3.0f, 1.0f, 10.0f, "%.0f chunks");
    defineColor("color",  "Line color",    IM_COL32(255,255,255,100));
    addSearchTag("chunk"); addSearchTag("grid");
}

void ChunkBorders::onRender() {
    if (!m_enabled) return;
    auto cam = EspRenderer::setupFrame();
    if (!cam.valid) return;

    float height = m_floatSettings["height"];
    int range    = (int)m_floatSettings["range"];
    ImU32 col    = m_colorSettings["color"];
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // Calculate chunk boundaries from camera position
    int camCX = (int)std::floor(cam.x / 16.0);
    int camCZ = (int)std::floor(cam.z / 16.0);

    for (int cx = camCX - range; cx <= camCX + range; cx++) {
        for (int cz = camCZ - range; cz <= camCZ + range; cz++) {
            int wx = cx * 16;
            int wz = cz * 16;

            // 4 vertical edges of the chunk
            auto line = [&](int x1, int z1, int x2, int z2) {
                ImVec2 a, b;
                if (EspRenderer::worldToScreen(x1, cam.y, z1, a) &&
                    EspRenderer::worldToScreen(x2, cam.y, z2, b)) {
                    dl->AddLine(a, b, col, 1.0f);
                }
                // Top at height
                ImVec2 at, bt;
                if (EspRenderer::worldToScreen(x1, cam.y + height, z1, at) &&
                    EspRenderer::worldToScreen(x2, cam.y + height, z2, bt)) {
                    dl->AddLine(at, bt, col, 1.0f);
                }
                // Vertical pillar
                ImVec2 p1, p2;
                if (EspRenderer::worldToScreen(x1, cam.y, z1, p1) &&
                    EspRenderer::worldToScreen(x1, cam.y + height, z1, p2)) {
                    dl->AddLine(p1, p2, col, 1.0f);
                }
            };

            line(wx, wz, wx+16, wz);
            line(wx+16, wz, wx+16, wz+16);
            line(wx+16, wz+16, wx, wz+16);
            line(wx, wz+16, wx, wz);
        }
    }
}
