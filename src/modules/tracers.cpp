#include "pch.h"
#include "core/strcrypt.h"
#include "modules/tracers.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"
#include "sdk/world.h"
#include "render/esp_renderer.h"
#include <cmath>

Tracers::Tracers() : Module("Tracers", ModuleCategory::RENDER, 0, "Draws lines from your crosshair to nearby entities") {
    defineBool("players",   "Players", true);
    defineBool("mobs",      "Mobs",    false);
    defineBool("items",     "Items",   false);
    defineFloat("range",    "Range",   200.0f, 10.0f, 500.0f, "%.0f");
    defineFloat("thickness","Thickness", 1.2f, 0.5f, 5.0f, "%.1f");
    defineColor("color_players", "Player Color", IM_COL32(255,80,80,200));
    defineColor("color_mobs",    "Mob Color",    IM_COL32(255,180,80,200));
    defineColor("color_items",   "Item Color",   IM_COL32(255,255,80,200));
}

// Cached tracer target (world position + color), rebuilt every ~100ms so the
// expensive per-entity JNI (getX/getY/getZ/isPlayer/isLiving/isItem) does NOT
// run 60x/sec — that was the extreme-lag source.
struct TracerTarget { double x, y, z; ImU32 col; };

void Tracers::onRender() {
    if (!m_enabled) return;
    JNIEnv* env = JvmWrapper::getEnv();
    if (!env) return;

    auto cam = EspRenderer::setupFrame();
    if (!cam.valid) return;

    float cx = cam.w * 0.5f, cy = cam.h * 0.5f;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    bool drawP = m_boolSettings["players"];
    bool drawM = m_boolSettings["mobs"];
    bool drawI = m_boolSettings["items"];
    float range = m_floatSettings["range"];
    float thick = m_floatSettings["thickness"];
    ImU32 colP = m_colorSettings["color_players"];
    ImU32 colM = m_colorSettings["color_mobs"];
    ImU32 colItem = m_colorSettings["color_items"];

    static std::vector<TracerTarget> s_cache;
    static unsigned long long s_lastScan = 0;
    unsigned long long nowMs = GetTickCount64();

    // Rebuild entity snapshot at most ~10x/sec
    if (nowMs - s_lastScan >= 100) {
        s_lastScan = nowMs;
        s_cache.clear();

        env->PushLocalFrame(1024);  // entity list can exceed 128 on crowded servers
        jobject p = CMinecraft::getPlayer();
        if (p) {
            CEntity pl(p); int pid = pl.getId();
            jobject w = CMinecraft::getWorld();
            if (w) {
                auto ents = CWorld::getAllEntities(w);
                env->DeleteLocalRef(w);

                for (auto& e : ents) {
                    if (e.getId() == pid || !e.isAlive()) continue;
                    bool isP = e.isPlayer();
                    bool isM = e.isLiving() && !isP;
                    bool isItem = e.isItem();
                    if ((isP && !drawP) || (isM && !drawM) || (isItem && !drawI)) continue;

                    double ex = e.getX(), ey = e.getY() + 1.0, ez = e.getZ();
                    double dx = ex - cam.x, dy = ey - cam.y, dz = ez - cam.z;
                    if (std::sqrt(dx*dx + dy*dy + dz*dz) > range) continue;

                    ImU32 col = isP ? colP : (isM ? colM : colItem);
                    s_cache.push_back({ex, ey, ez, col});
                }
            }
        }
        env->PopLocalFrame(nullptr);
    }

    // Draw from cache (cheap, no JNI)
    for (auto& t : s_cache) {
        ImVec2 s;
        if (!EspRenderer::tracerToScreen(t.x, t.y, t.z, s)) continue;
        dl->AddLine(ImVec2(cx, cy), s, t.col, thick);
    }
}
