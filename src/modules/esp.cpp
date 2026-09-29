#include "pch.h"
#include "core/strcrypt.h"
#include "modules/esp.h"
#include "sdk/minecraft.h"
#include "sdk/world.h"
#include "core/jvm_wrapper.h"
#include "render/esp_renderer.h"

ESP::ESP() : Module("ESP", ModuleCategory::RENDER, 0, "Shows players + storage through walls — boxes, names, health, chests") {
    defineGroup("Players");
    defineBool("draw_boxes",    "2D Boxes",     true);
    defineBool("draw_3d_boxes", "3D Corners",   false);
    defineBool("draw_names",    "Render Names", true);
    defineBool("draw_health",   "Health Bars",  true);
    defineBool("draw_distance", "Distance Info",true);
    defineBool("draw_armor",    "Armor Info",   false);
    defineBool("chams",         "Chams (Glow)",   false);
    defineGroupEnd();

    defineGroup("Entities");
    defineBool("draw_animals",  "Render Animals", false);
    defineBool("draw_chests",   "Render Chests",  false);
    defineBool("draw_items",    "Render Items",   false);
    defineBool("draw_objects",  "Render Objects", false);
    defineGroupEnd();

    // ── ChestESP / StorageESP ───────────────────────────────────────
    defineGroup("Storage ESP");
    defineBool("chest_esp",     "Enable Storage ESP", true);
    defineFloat("chest_range",  "Max Range",     64.0f,  8.0f, 256.0f, "%.0f");
    defineBool("chest_chests",  "Show Chests",   true);
    defineBool("chest_barrels", "Show Barrels",   true);
    defineBool("chest_shulkers","Show Shulkers",  true);
    defineBool("chest_ender",   "Show Ender Chests", true);
    defineBool("chest_furnaces","Show Furnaces",  false);
    defineBool("chest_names",   "Show Names",     true);
    defineBool("chest_dist",    "Show Distance",  true);
    defineBool("chest_3d",      "3D Wireframe",   true);
    defineColor("chest_c_chest",    "Chest Color",      IM_COL32(255, 170, 80,  220));
    defineColor("chest_c_barrel",   "Barrel Color",     IM_COL32(180, 140, 90,  220));
    defineColor("chest_c_shulker",  "Shulker Color",    IM_COL32(200, 80,  220, 220));
    defineColor("chest_c_ender",    "Ender Chest Color",IM_COL32(80,  220, 220, 220));
    defineColor("chest_c_furnace",  "Furnace Color",    IM_COL32(140, 140, 140, 200));
    defineGroupEnd();

    defineGroup("Colors");
    defineFloat("max_distance", "Max Distance", 200.0f, 10.0f, 500.0f, "%.0f");
    defineColor("box_visible",  "Box Visible",   IM_COL32(140, 100, 255, 200));
    defineColor("box_hidden",   "Box Hidden",    IM_COL32(200, 60, 60, 160));
    defineColor("text_color",   "Text Color",    IM_COL32(255, 255, 255, 255));
    defineColor("bg_color",     "Name BG Color", IM_COL32(0, 0, 0, 150));
    defineGroupEnd();

    addSearchTag("chest");
    addSearchTag("storage");
    addSearchTag("barrel");
    addSearchTag("shulker");
}

// ═══════════════════════════════════════════════════════════════════════
//  ChestESP: JNI init for block scanning
// ═══════════════════════════════════════════════════════════════════════

bool ESP::initChestJNI(JNIEnv* env) {
    if (s_chestJni) return true;

    if (!s_lvlCls)  s_lvlCls  = JvmWrapper::findClass(Mappings::Level_Class);
    if (s_lvlCls && !s_getState)
        s_getState = env->GetMethodID(s_lvlCls, Mappings::Level_getBlockState, Mappings::Level_getBlockState_Sig);

    if (!s_bsCls)   s_bsCls   = JvmWrapper::findClass(Mappings::BlockState_Class);
    if (s_bsCls && !s_bs_getBlock)
        s_bs_getBlock = env->GetMethodID(s_bsCls, Mappings::BlockState_getBlock, Mappings::BlockState_getBlock_Sig);

    if (!s_blockCls) s_blockCls = JvmWrapper::findClass(Mappings::Block_Class);
    if (s_blockCls && !s_block_descId)
        s_block_descId = env->GetMethodID(s_blockCls, Mappings::Block_getDescriptionId, Mappings::Block_getDescriptionId_Sig);

    if (!s_mbpCls)   s_mbpCls   = JvmWrapper::findClass(Mappings::MutableBlockPos_Class);
    if (s_mbpCls && !s_mbpInit)
        s_mbpInit = env->GetMethodID(s_mbpCls, Mappings::MutableBlockPos_Init, Mappings::MutableBlockPos_Init_Sig);

    s_chestJni = s_getState && s_bs_getBlock && s_block_descId && s_mbpInit;
    return s_chestJni;
}

// ═══════════════════════════════════════════════════════════════════════
//  classifyContainer — returns 0=none, 1=chest, 2=barrel, 3=shulker,
//                      4=ender, 5=furnace
// ═══════════════════════════════════════════════════════════════════════

int ESP::classifyContainer(const char* descId) {
    if (!descId) return 0;

    // Ender chest first (before normal "chest" match)
    if (strstr(descId, "ender_chest"))
        return 4;
    // Shulker boxes: "shulker_box" matches all colors
    if (strstr(descId, "shulker_box"))
        return 3;
    // Normal + trapped chests: "chest" (but not "ender_chest" already checked)
    if (strstr(descId, "chest"))
        return 1;
    if (strstr(descId, "barrel"))
        return 2;
    if (strstr(descId, "furnace")
     || strstr(descId, "blast_furnace")
     || strstr(descId, "smoker"))
        return 5;
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════
//  onRender — entity ESP + ChestESP
// ═══════════════════════════════════════════════════════════════════════

void ESP::onRender() {
    if (!m_enabled) return;

    JNIEnv* env = JvmWrapper::getEnv();
    if (!env) return;

    env->PushLocalFrame(4096);

    jobject localPlayer = CMinecraft::getPlayer();
    if (!localPlayer) { env->PopLocalFrame(nullptr); return; }

    auto cam = EspRenderer::setupFrame();
    if (!cam.valid) { env->PopLocalFrame(nullptr); return; }
    double camX = cam.x, camY = cam.y, camZ = cam.z;

    jobject worldObj = CMinecraft::getWorld();
    if (!worldObj) { env->PopLocalFrame(nullptr); return; }

    CEntity localEntity(localPlayer);
    int localId = localEntity.getId();
    auto players = CWorld::getAllEntities(worldObj);

    bool boxes       = m_boolSettings["draw_boxes"];
    bool boxes3D     = m_boolSettings["draw_3d_boxes"];
    bool names       = m_boolSettings["draw_names"];
    bool health      = m_boolSettings["draw_health"];
    bool distance    = m_boolSettings["draw_distance"];
    bool drawAnimals = m_boolSettings["draw_animals"];
    bool drawItems   = m_boolSettings["draw_items"];
    bool drawObjects = m_boolSettings["draw_objects"];
    bool drawChests  = m_boolSettings["draw_chests"];
    bool chams       = m_boolSettings["chams"];
    float maxDist    = m_floatSettings["max_distance"];

    // ═══════════════════════════════════════════════════════════════════
    //  Phase 1: Entity ESP (existing)
    // ═══════════════════════════════════════════════════════════════════

    for (auto& entity : players) {
        if (entity.getId() == localId) continue;
        if (!entity.isAlive()) continue;

        bool isPlayer      = entity.isPlayer();
        bool isItem        = entity.isItem();
        bool isLiving      = entity.isLiving() && !entity.isArmorStand();
        bool isMinecartC   = entity.isMinecartChest();

        if (!isPlayer) {
            if (isItem)        { if (!drawItems)    continue; }
            else if (isLiving) { if (!drawAnimals)  continue; }
            else if (isMinecartC) { if (!drawChests) continue; }
            else               { if (!drawObjects)  continue; }
        }

        double dx = entity.getX() - camX;
        double dy = entity.getY() - camY;
        double dz = entity.getZ() - camZ;
        double distToEntity = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (distToEntity > maxDist) continue;

        if (chams) entity.setGlowingTag(true);
        else if (entity.hasGlowingTag()) entity.setGlowingTag(false);

        ImU32 boxColor = m_colorSettings["box_visible"];
        if (!isPlayer) {
            if (isItem)        boxColor = IM_COL32(255, 255, 0, 200);
            else if (isLiving) boxColor = IM_COL32(255, 165, 0, 200);
            else if (isMinecartC) boxColor = IM_COL32(255, 105, 180, 200);
            else               boxColor = IM_COL32(100, 200, 255, 200);
        }

        EspRenderer::EspColors colors = {
            boxColor,
            m_colorSettings["box_hidden"],
            m_colorSettings["text_color"],
            m_colorSettings["bg_color"]
        };

        bool drawEntHealth = health && (isPlayer || isLiving);
        EspRenderer::drawEntity(entity, boxes || boxes3D, names, drawEntHealth,
                                distance, colors, m_boolSettings["draw_armor"]);

        if (boxes3D) {
            auto bb = entity.getBoundingBox();
            if (bb.minX != 0.0 || bb.maxX != 0.0) {
                EspRenderer::draw3DBox(bb.minX, bb.minY, bb.minZ,
                                       bb.maxX, bb.maxY, bb.maxZ,
                                       boxColor, 1.2f);
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    //  Phase 2: ChestESP / StorageESP (block scanning, THROTTLED)
    //  Der Scan macht pro Frame ~100k+ JNI-Calls → extreme Lags. Jetzt:
    //  nur alle ~600ms (oder bei Bewegung >3 Blöcken) scannen, Treffer
    //  cachen und jede Frame billig (reine Mathe, kein JNI) zeichnen.
    // ═══════════════════════════════════════════════════════════════════

    if (!m_boolSettings["chest_esp"]) { env->PopLocalFrame(nullptr); return; }
    if (!initChestJNI(env)) { env->PopLocalFrame(nullptr); return; }

    float chestRange = m_floatSettings["chest_range"];
    int   iRange     = (int)std::ceil(chestRange);

    bool showChests   = m_boolSettings["chest_chests"];
    bool showBarrels  = m_boolSettings["chest_barrels"];
    bool showShulkers = m_boolSettings["chest_shulkers"];
    bool showEnder    = m_boolSettings["chest_ender"];
    bool showFurnaces = m_boolSettings["chest_furnaces"];
    bool chestNames   = m_boolSettings["chest_names"];
    bool chestDist    = m_boolSettings["chest_dist"];
    bool chest3D      = m_boolSettings["chest_3d"];

    // Color per container type
    ImU32 colChest   = m_colorSettings["chest_c_chest"];
    ImU32 colBarrel  = m_colorSettings["chest_c_barrel"];
    ImU32 colShulker = m_colorSettings["chest_c_shulker"];
    ImU32 colEnder   = m_colorSettings["chest_c_ender"];
    ImU32 colFurnace = m_colorSettings["chest_c_furnace"];

    int px = (int)std::floor(localEntity.getX());
    int py = (int)std::floor(localEntity.getY());
    int pz = (int)std::floor(localEntity.getZ());

    unsigned long long nowMs = GetTickCount64();
    bool movedFar = (px > m_chestPx ? px - m_chestPx : m_chestPx - px) > 3 ||
                    (py > m_chestPy ? py - m_chestPy : m_chestPy - py) > 3 ||
                    (pz > m_chestPz ? pz - m_chestPz : m_chestPz - pz) > 3;
    bool needScan = m_chestCache.empty() ||
                    (nowMs - m_chestLastScan >= 600 && movedFar) ||
                    (nowMs - m_chestLastScan >= 2000);

    if (needScan) {
        m_chestLastScan = nowMs;
        m_chestPx = px; m_chestPy = py; m_chestPz = pz;
        m_chestCache.clear();

        for (int dx = -iRange; dx <= iRange; dx++) {
            for (int dy = -5; dy <= 5; dy++) {
                for (int dz = -iRange; dz <= iRange; dz++) {
                    int bx = px + dx, by = py + dy, bz = pz + dz;

                    // Quick distance pre-check (cheaper than JNI call)
                    float cxf = (bx + 0.5f) - (float)camX;
                    float cyf = (by + 0.5f) - (float)camY;
                    float czf = (bz + 0.5f) - (float)camZ;
                    if (std::sqrt(cxf*cxf + cyf*cyf + czf*czf) > chestRange) continue;

                    // Get block state via JNI
                    jobject mbp = env->NewObject(s_mbpCls, s_mbpInit, (jint)bx, (jint)by, (jint)bz);
                    if (!mbp) continue;

                    jobject state = env->CallObjectMethod(worldObj, s_getState, mbp);
                    env->DeleteLocalRef(mbp);
                    if (!state || env->ExceptionCheck()) {
                        env->ExceptionClear();
                        if (state) env->DeleteLocalRef(state);
                        continue;
                    }

                    jobject block = env->CallObjectMethod(state, s_bs_getBlock);
                    if (!block || env->ExceptionCheck()) {
                        env->ExceptionClear();
                        if (block) env->DeleteLocalRef(block);
                        env->DeleteLocalRef(state);
                        continue;
                    }

                    jstring descStr = (jstring)env->CallObjectMethod(block, s_block_descId);
                    env->DeleteLocalRef(block);
                    if (!descStr || env->ExceptionCheck()) {
                        env->ExceptionClear();
                        if (descStr) env->DeleteLocalRef(descStr);
                        env->DeleteLocalRef(state);
                        continue;
                    }

                    const char* descC = env->GetStringUTFChars(descStr, nullptr);
                    if (!descC) {
                        env->DeleteLocalRef(descStr);
                        env->DeleteLocalRef(state);
                        continue;
                    }

                    int type = classifyContainer(descC);
                    env->ReleaseStringUTFChars(descStr, descC);
                    env->DeleteLocalRef(descStr);
                    env->DeleteLocalRef(state);

                    if (type == 0) continue;

                    // Filter by enabled container types
                    switch (type) {
                        case 1: if (!showChests)   continue; break;
                        case 2: if (!showBarrels)  continue; break;
                        case 3: if (!showShulkers) continue; break;
                        case 4: if (!showEnder)    continue; break;
                        case 5: if (!showFurnaces) continue; break;
                        default: continue;
                    }

                    m_chestCache.push_back({type, bx, by, bz});
                }
            }
        }
    }

    // ── Draw from cache (cheap: pure math, no JNI) ───────────────────
    ImDrawList* bdl = ImGui::GetBackgroundDrawList();
    for (auto& hit : m_chestCache) {
        ImU32 color = 0;
        const char* label = "";
        switch (hit.type) {
            case 1: color = colChest;   label = "Chest"; break;
            case 2: color = colBarrel;  label = "Barrel"; break;
            case 3: color = colShulker; label = "Shulker"; break;
            case 4: color = colEnder;   label = "Ender Chest"; break;
            case 5: color = colFurnace; label = "Furnace"; break;
            default: continue;
        }

        float cxf = (hit.x + 0.5f) - (float)camX;
        float cyf = (hit.y + 0.5f) - (float)camY;
        float czf = (hit.z + 0.5f) - (float)camZ;
        float dist = std::sqrt(cxf*cxf + cyf*cyf + czf*czf);

        // Draw 3D wireframe box
        if (chest3D) {
            EspRenderer::draw3DBox((double)hit.x, (double)hit.y, (double)hit.z,
                                   (double)(hit.x + 1), (double)(hit.y + 1), (double)(hit.z + 1),
                                   color, 1.5f);
        }

        // Draw label at center-top of block
        if (chestNames || chestDist) {
            ImVec2 screen;
            if (EspRenderer::worldToScreen(hit.x + 0.5, hit.y + 1.2, hit.z + 0.5, screen)) {
                char buf[64];
                if (chestNames && chestDist)
                    snprintf(buf, sizeof(buf), "%s [%.0fm]", label, dist);
                else if (chestNames)
                    snprintf(buf, sizeof(buf), "%s", label);
                else
                    snprintf(buf, sizeof(buf), "%.0fm", dist);

                ImVec2 ts = ImGui::CalcTextSize(buf);
                float sx = screen.x - ts.x * 0.5f;
                float sy = screen.y;
                bdl->AddRectFilled(ImVec2(sx - 2, sy), ImVec2(sx + ts.x + 2, sy + ts.y),
                                   IM_COL32(0, 0, 0, 160), 3.0f);
                bdl->AddText(ImVec2(sx, sy), IM_COL32(255, 255, 255, 240), buf);
            }
        }
    }

    env->PopLocalFrame(nullptr);
}
