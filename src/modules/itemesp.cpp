#include "pch.h"
#include "core/strcrypt.h"
#include "modules/itemesp.h"
#include "sdk/minecraft.h"
#include "sdk/world.h"
#include "render/esp_renderer.h"

ItemESP::ItemESP() : Module("ItemESP", ModuleCategory::RENDER, 0,
    "Highlights dropped items on the ground with name, distance, and rarity color.") {
    defineFloat("max_distance", "Max Distance",  32.0f,  8.0f, 128.0f, "%.0f");
    defineBool("draw_names",    "Show Names",    true);
    defineBool("draw_distance", "Show Distance", true);
    defineBool("draw_boxes",    "Show Boxes",    true);
    defineBool("draw_glow",     "Glow Outline",  true);
    defineFloat("box_size",     "Box Size",      0.4f, 0.2f, 1.0f, "%.1f");
    defineBool("filter_junk",   "Hide Junk",     false);
    defineColor("box_color",    "Box Color",     IM_COL32(255, 255, 80, 220));
    defineColor("rare_color",   "Rare Color",    IM_COL32(80, 220, 255, 230));
    addSearchTag("item");
    addSearchTag("drop");
    addSearchTag("loot");
}

void ItemESP::onRender() {
    if (!m_enabled) return;

    JNIEnv* env = JvmWrapper::getEnv();
    if (!env) return;
    env->PushLocalFrame(4096);

    auto cam = EspRenderer::setupFrame();
    if (!cam.valid) { env->PopLocalFrame(nullptr); return; }

    jobject worldObj = CMinecraft::getWorld();
    if (!worldObj) { env->PopLocalFrame(nullptr); return; }

    auto entities = CWorld::getAllEntities(worldObj);
    float  maxDist = m_floatSettings["max_distance"];
    bool   names   = m_boolSettings["draw_names"];
    bool   distOn  = m_boolSettings["draw_distance"];
    bool   boxes   = m_boolSettings["draw_boxes"];
    bool   glow    = m_boolSettings["draw_glow"];
    bool   hideJunk= m_boolSettings["filter_junk"];
    float  boxSize = m_floatSettings["box_size"];
    ImU32  boxCol  = m_colorSettings["box_color"];
    ImU32  rareCol = m_colorSettings["rare_color"];

    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // JNI cache for item name extraction
    static jmethodID s_getItem  = nullptr;
    static jmethodID s_getHover = nullptr;
    static jmethodID s_getStr   = nullptr;
    static jmethodID s_getCount = nullptr;
    static bool s_jni = false;
    if (!s_jni) {
        s_jni = true;
        jclass isCls = JvmWrapper::findClass(Mappings::ItemStack_Class);
        jclass compCls = JvmWrapper::findClass(Mappings::Component_Class);
        if (isCls) {
            s_getItem  = env->GetMethodID(isCls, Mappings::ItemStack_getItem, Mappings::ItemStack_getItem_Sig);
            s_getHover = env->GetMethodID(isCls, Mappings::ItemStack_getHoverName, Mappings::ItemStack_getHoverName_Sig);
            s_getCount = env->GetMethodID(isCls, Mappings::ItemStack_getCount, Mappings::ItemStack_getCount_Sig);
        }
        if (compCls) s_getStr = env->GetMethodID(compCls, Mappings::Component_getString, Mappings::Component_getString_Sig);
    }

    for (auto& entity : entities) {
        if (!entity.isItem()) continue;

        double ex = entity.getX();
        double ey = entity.getY();
        double ez = entity.getZ();
        double dx = ex - cam.x, dy = ey - cam.y, dz = ez - cam.z;
        double dist = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (dist > maxDist) continue;

        // Glow effect
        if (glow && !entity.hasGlowingTag()) entity.setGlowingTag(true);

        // Extract item name via JNI
        std::string label;
        int count = 0;
        bool isRare = false;
        if (names || distOn) {
            jobject stack = entity.getObject();
            if (stack && s_getItem && s_getHover && s_getStr) {
                if (s_getCount) {
                    count = env->CallIntMethod(stack, s_getCount);
                    if (env->ExceptionCheck()) { env->ExceptionClear(); count = 0; }
                }
                jobject hover = env->CallObjectMethod(stack, s_getHover);
                if (hover && !env->ExceptionCheck()) {
                    jstring js = (jstring)env->CallObjectMethod(hover, s_getStr);
                    if (js && !env->ExceptionCheck()) {
                        const char* cp = env->GetStringUTFChars(js, nullptr);
                        if (cp) {
                            label = cp;
                            // Rare items: diamond, netherite, emerald, enchanted
                            const char* lc = label.c_str();
                            isRare = strstr(lc, "iamond") || strstr(lc, "etherite")
                                  || strstr(lc, "merald") || strstr(lc, "nchanted")
                                  || strstr(lc, "olden") || strstr(lc, "§");
                            env->ReleaseStringUTFChars(js, cp);
                        }
                        env->DeleteLocalRef(js);
                    }
                    env->DeleteLocalRef(hover);
                }
                if (env->ExceptionCheck()) env->ExceptionClear();
            }
            if (label.empty()) label = "Item";
        }

        // Junk filter (skip common trash like stone, dirt, seeds)
        if (hideJunk && !isRare) {
            const char* lc = label.c_str();
            if (strstr(lc, "obblestone") || strstr(lc, "Dirt") || strstr(lc, "Sand")
             || strstr(lc, "Seeds") || strstr(lc, "Rotten") || strstr(lc, "Bone")
             || strstr(lc, "String") || strstr(lc, "Arrow")) continue;
        }

        // 3D box
        if (boxes) {
            float s = boxSize;
            ImU32 col = isRare ? rareCol : boxCol;
            EspRenderer::draw3DBox(ex - s, ey, ez - s, ex + s, ey + s*2, ez + s, col, 1.2f);
        }

        // Name + count label above item
        if (names || distOn) {
            ImVec2 sc;
            if (EspRenderer::worldToScreen(ex, ey + 0.6, ez, sc)) {
                char buf[128];
                if (names && distOn && count > 1)
                    snprintf(buf, sizeof(buf), "%s x%d [%.0fm]", label.c_str(), count, dist);
                else if (names && distOn)
                    snprintf(buf, sizeof(buf), "%s [%.0fm]", label.c_str(), dist);
                else if (names && count > 1)
                    snprintf(buf, sizeof(buf), "%s x%d", label.c_str(), count);
                else if (names)
                    snprintf(buf, sizeof(buf), "%s", label.c_str());
                else
                    snprintf(buf, sizeof(buf), "[%.0fm]", dist);

                ImVec2 ts = ImGui::CalcTextSize(buf);
                float sx = sc.x - ts.x * 0.5f;
                ImU32 col = isRare ? rareCol : boxCol;
                dl->AddRectFilled(ImVec2(sx-2, sc.y), ImVec2(sx+ts.x+2, sc.y+ts.y),
                                  IM_COL32(0,0,0,180), 3.0f);
                dl->AddText(ImVec2(sx, sc.y), col, buf);
            }
        }
    }

    env->PopLocalFrame(nullptr);
}
