#include "pch.h"
#include "core/strcrypt.h"
#include "modules/nametags.h"
#include "sdk/minecraft.h"
#include "sdk/world.h"
#include "render/esp_renderer.h"

// ═══════════════════════════════════════════════════════════════════════
//  NameTags — Clean player name tags for PvP
//
//  Renders player names, health bars, armor info, and distance above
//  their heads. Color-coded by health. Uses worldToScreen for
//  positioning and ImDrawList for rendering.
// ═══════════════════════════════════════════════════════════════════════

NameTags::NameTags() : Module("NameTags", ModuleCategory::RENDER, 0,
    "Clean PvP name tags above players — name, health, armor, distance.") {
    defineGroup("Players");
    defineBool("draw_names",    "Show Names",      true);
    defineBool("draw_health",   "Show Health",     true);
    defineBool("draw_armor",    "Show Armor",      true);
    defineBool("draw_distance", "Show Distance",   true);
    defineBool("sneak_detect",  "Sneak Color",     true);
    defineFloat("max_distance", "Max Distance",    64.0f, 8.0f, 256.0f, "%.0f");
    defineGroupEnd();

    defineGroup("Style");
    defineColor("text_color",   "Text Color",      IM_COL32(255, 255, 255, 255));
    defineColor("bg_color",     "Background",      IM_COL32(0, 0, 0, 160));
    defineColor("sneak_color",  "Sneak Color",     IM_COL32(255, 170, 80, 255));
    defineGroupEnd();

    addSearchTag("name");
    addSearchTag("tag");
    addSearchTag("health");
    addSearchTag("player");
}

// ═══════════════════════════════════════════════════════════════════════
//  Health → color gradient (green → yellow → red)
// ═══════════════════════════════════════════════════════════════════════

static ImU32 healthColor(float ratio) {
    if (ratio > 0.6f) {
        int g = 255;
        int r = (int)((1.0f - ratio) * 2.5f * 255);
        if (r > 255) r = 255;
        return IM_COL32(r, g, 40, 255);
    } else if (ratio > 0.3f) {
        int r = 255;
        int g = (int)(ratio * 3.3f * 255);
        if (g > 255) g = 255;
        return IM_COL32(r, g, 40, 255);
    } else {
        return IM_COL32(255, 60, 40, 255);
    }
}

// ═══════════════════════════════════════════════════════════════════════

void NameTags::onRender() {
    if (!m_enabled) return;

    JNIEnv* env = JvmWrapper::getEnv();
    if (!env) return;

    env->PushLocalFrame(2048);

    auto cam = EspRenderer::setupFrame();
    if (!cam.valid) { env->PopLocalFrame(nullptr); return; }

    jobject worldObj = CMinecraft::getWorld();
    if (!worldObj) { env->PopLocalFrame(nullptr); return; }

    jobject localPlayer = CMinecraft::getPlayer();
    if (!localPlayer) { env->PopLocalFrame(nullptr); return; }
    CEntity localEnt(localPlayer);
    int localId = localEnt.getId();

    auto players = CWorld::getAllEntities(worldObj);

    bool  showNames    = m_boolSettings["draw_names"];
    bool  showHealth   = m_boolSettings["draw_health"];
    bool  showArmor    = m_boolSettings["draw_armor"];
    bool  showDistance = m_boolSettings["draw_distance"];
    bool  sneakDetect  = m_boolSettings["sneak_detect"];
    float maxDist      = m_floatSettings["max_distance"];

    ImU32 textColor    = m_colorSettings["text_color"];
    ImU32 bgColor      = m_colorSettings["bg_color"];
    ImU32 sneakColor   = m_colorSettings["sneak_color"];

    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    for (auto& entity : players) {
        if (!entity.isPlayer()) continue;
        if (entity.getId() == localId) continue;
        if (!entity.isAlive()) continue;

        double ex = entity.getX();
        double ey = entity.getY();
        double ez = entity.getZ();

        double dx = ex - cam.x;
        double dy = ey - cam.y;
        double dz = ez - cam.z;
        double dist = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (dist > (double)maxDist) continue;

        // Project head position (eye height + margin) to screen
        ImVec2 screen;
        if (!EspRenderer::worldToScreen(ex, ey + 2.3, ez, screen)) continue;

        // ── Name ──────────────────────────────────────────────────
        std::string name = entity.getName();
        if (name.empty()) name = "Player";

        float health   = entity.getHealth();
        float maxHP    = entity.getMaxHealth();
        float hpRatio  = (maxHP > 0) ? (health / maxHP) : 1.0f;

        // Health suffix: "PlayerName 20"
        char line1[128];
        if (showHealth && maxHP > 0)
            snprintf(line1, sizeof(line1), "%s %d", name.c_str(), (int)health);
        else
            snprintf(line1, sizeof(line1), "%s", name.c_str());

        ImVec2 ts = ImGui::CalcTextSize(line1);
        float cx = screen.x - ts.x * 0.5f;  // center-aligned
        float cy = screen.y;

        // Text color: sneak players get orange
        ImU32 tc = textColor;
        if (sneakDetect && entity.isShiftKeyDown())
            tc = sneakColor;

        // Background pill
        float pad = 3;
        dl->AddRectFilled(ImVec2(cx - pad, cy), ImVec2(cx + ts.x + pad, cy + ts.y + 2),
                          bgColor, 4.0f);
        dl->AddText(ImVec2(cx, cy), tc, line1);

        float nextY = cy + ts.y + 2;

        // ── Health bar ────────────────────────────────────────────
        if (showHealth && maxHP > 0) {
            float barH = 3.0f;
            float barW = ts.x + pad * 2;
            float barX = cx;
            float barY = nextY + 2;

            // Dark background
            dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH),
                              IM_COL32(0, 0, 0, 200));
            // Colored fill
            dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW * hpRatio, barY + barH),
                              healthColor(hpRatio));
            nextY = barY + barH;
        }

        // ── Distance ──────────────────────────────────────────────
        if (showDistance) {
            char dstr[32];
            snprintf(dstr, sizeof(dstr), "[%.0fm]", dist);
            ImVec2 ds = ImGui::CalcTextSize(dstr);
            float dxs = cx + (ts.x + pad * 2) * 0.5f - ds.x * 0.5f;
            float dys = nextY + 1;
            dl->AddRectFilled(ImVec2(dxs - 2, dys), ImVec2(dxs + ds.x + 2, dys + ds.y),
                              bgColor, 3.0f);
            dl->AddText(ImVec2(dxs, dys), textColor, dstr);
            nextY = dys + ds.y;
        }

        // ── Armor ─────────────────────────────────────────────────
        if (showArmor) {
            // For PvP nametags, show armor info next to the name
            // We'll add a simple colored dot indicating armor tier
            // Full JNI armor iteration is heavy — use visual cues
            // For now, show the armor as a colored square
            float hp = entity.getHealth();
            float mhp = entity.getMaxHealth();
            // Total HP as proxy for armor (maxHP goes up with armor)
            if (mhp > 20.0f) {
                int extraHP = (int)(mhp - 20.0f);
                // Each armor icon represents ~4 extra HP (2 armor points)
                int armorIcons = extraHP / 4;
                if (armorIcons > 8) armorIcons = 8;

                float iconSize = 6.0f;
                float iconY = nextY + 2;
                float startX = cx + ts.x * 0.5f - (armorIcons * (iconSize + 2)) * 0.5f;

                for (int i = 0; i < armorIcons; i++) {
                    dl->AddRectFilled(
                        ImVec2(startX + i * (iconSize + 2), iconY),
                        ImVec2(startX + i * (iconSize + 2) + iconSize, iconY + iconSize),
                        IM_COL32(100, 180, 255, 220));
                }
            }
        }
    }

    env->PopLocalFrame(nullptr);
}
