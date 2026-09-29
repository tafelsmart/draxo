#include "pch.h"
#include "core/strcrypt.h"
#include "modules/mobesp.h"
#include "sdk/minecraft.h"
#include "sdk/world.h"
#include "render/esp_renderer.h"
#include "core/jvm_wrapper.h"

MobESP::MobESP() : Module("MobESP", ModuleCategory::RENDER, 0,
    "Highlights animals and monsters with color-coded boxes and names.") {
    defineGroup("Filters");
    defineBool("show_hostile",  "Hostile Mobs",  true);
    defineBool("show_neutral",  "Neutral Mobs",  true);
    defineBool("show_passive",  "Passive Mobs",  true);
    defineFloat("max_distance", "Max Distance",  64.0f, 8.0f, 256.0f, "%.0f");
    defineGroupEnd();

    defineGroup("Style");
    defineBool("draw_boxes", "3D Boxes",     true);
    defineBool("draw_names", "Show Names",   true);
    defineBool("draw_health","Health Bar",   true);
    defineBool("draw_glow",  "Glow",         false);
    defineColor("c_hostile", "Hostile Color", IM_COL32(255, 60, 60, 220));
    defineColor("c_neutral", "Neutral Color", IM_COL32(255, 200, 60, 220));
    defineColor("c_passive", "Passive Color", IM_COL32(80, 255, 80, 200));
    defineGroupEnd();

    addSearchTag("mob");
    addSearchTag("animal");
    addSearchTag("monster");
}

void MobESP::onRender() {
    if (!m_enabled) return;
    JNIEnv* env = JvmWrapper::getEnv();
    if (!env) return;
    env->PushLocalFrame(4096);

    auto cam = EspRenderer::setupFrame();
    if (!cam.valid) { env->PopLocalFrame(nullptr); return; }

    jobject worldObj = CMinecraft::getWorld();
    if (!worldObj) { env->PopLocalFrame(nullptr); return; }

    auto entities = CWorld::getAllEntities(worldObj);
    float maxDist = m_floatSettings["max_distance"];
    bool hostile  = m_boolSettings["show_hostile"];
    bool neutral  = m_boolSettings["show_neutral"];
    bool passive  = m_boolSettings["show_passive"];
    bool boxes    = m_boolSettings["draw_boxes"];
    bool names    = m_boolSettings["draw_names"];
    bool health   = m_boolSettings["draw_health"];
    bool glow     = m_boolSettings["draw_glow"];

    ImU32 cH = m_colorSettings["c_hostile"];
    ImU32 cN = m_colorSettings["c_neutral"];
    ImU32 cP = m_colorSettings["c_passive"];

    // JNI: get entity class name for type detection
    static jmethodID s_getName = nullptr;
    static bool s_jni = false;
    if (!s_jni) {
        s_jni = true;
        jclass entCls = JvmWrapper::findClass(Mappings::Entity_Class);
        if (entCls) {
            s_getName = env->GetMethodID(entCls, "getType",
                "()Lnet/minecraft/world/entity/EntityType;");
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_getName = nullptr; }
        }
    }

    for (auto& entity : entities) {
        if (entity.isPlayer() || entity.isItem() || entity.isArmorStand()) continue;
        if (!entity.isLiving()) continue;

        double ex = entity.getX(), ey = entity.getY(), ez = entity.getZ();
        double dx = ex - cam.x, dy = ey - cam.y, dz = ez - cam.z;
        double dist = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (dist > maxDist) continue;

        // Classify mob type via entity name substring matching
        std::string name = entity.getName();
        if (name.empty()) name = "Mob";
        for (auto& c : name) c = (char)tolower((unsigned char)c);

        int mobType = 0; // 0=none, 1=hostile, 2=neutral, 3=passive
        // Hostile keywords
        if (name.find("zombie")!=std::string::npos||name.find("skeleton")!=std::string::npos
         ||name.find("creeper")!=std::string::npos||name.find("spider")!=std::string::npos
         ||name.find("witch")!=std::string::npos||name.find("slime")!=std::string::npos
         ||name.find("guardian")!=std::string::npos||name.find("phantom")!=std::string::npos
         ||name.find("pillager")!=std::string::npos||name.find("vindicator")!=std::string::npos
         ||name.find("evoker")!=std::string::npos||name.find("ravager")!=std::string::npos
         ||name.find("hoglin")!=std::string::npos||name.find("piglin")!=std::string::npos&&name.find("piglin brute")==std::string::npos
         ||name.find("drowned")!=std::string::npos||name.find("husk")!=std::string::npos
         ||name.find("stray")!=std::string::npos||name.find("blaze")!=std::string::npos
         ||name.find("ghast")!=std::string::npos||name.find("magma")!=std::string::npos
         ||name.find("wither")!=std::string::npos||name.find("warden")!=std::string::npos
         ||name.find("endermite")!=std::string::npos||name.find("silverfish")!=std::string::npos)
            mobType = 1;
        // Neutral keywords (overrides hostile for piglin, enderman, etc.)
        else if (name.find("enderman")!=std::string::npos||name.find("wolf")!=std::string::npos
              ||name.find("spider")!=std::string::npos||name.find("piglin")!=std::string::npos&&name.find("piglin brute")==std::string::npos
              ||name.find("bee")!=std::string::npos||name.find("iron_golem")!=std::string::npos
              ||name.find("snow_golem")!=std::string::npos||name.find("llama")!=std::string::npos
              ||name.find("dolphin")!=std::string::npos||name.find("panda")!=std::string::npos
              ||name.find("polar_bear")!=std::string::npos||name.find("cave_spider")!=std::string::npos)
            mobType = 2;
        // Passive keywords
        else
            mobType = 3;

        if (mobType==1 && !hostile) continue;
        if (mobType==2 && !neutral) continue;
        if (mobType==3 && !passive) continue;

        ImU32 col = (mobType==1) ? cH : (mobType==2) ? cN : cP;
        if (glow) entity.setGlowingTag(true);

        if (boxes) {
            auto bb = entity.getBoundingBox();
            if (bb.minX != 0) EspRenderer::draw3DBox(bb.minX,bb.minY,bb.minZ,bb.maxX,bb.maxY,bb.maxZ,col,1.2f);
        }
        if (health) {
            float hp = entity.getHealth(), mhp = entity.getMaxHealth();
            if (mhp>0) {
                float r = hp/mhp;
                ImU32 hc = (r>0.6f)?IM_COL32((int)((1-r)*2.5f*255),255,40,255)
                          :(r>0.3f)?IM_COL32(255,(int)(r*3.3f*255),40,255):IM_COL32(255,60,40,255);
                // Draw health bar above head
                ImVec2 sc;
                if (EspRenderer::worldToScreen(ex, ey+entity.getBoundingBox().maxY-entity.getBoundingBox().minY+0.3, ez, sc)) {
                    float w=40, h=3;
                    ImDrawList* dl=ImGui::GetBackgroundDrawList();
                    dl->AddRectFilled(ImVec2(sc.x-w/2,sc.y),ImVec2(sc.x+w/2,sc.y+h),IM_COL32(0,0,0,180));
                    dl->AddRectFilled(ImVec2(sc.x-w/2,sc.y),ImVec2(sc.x-w/2+w*r,sc.y+h),hc);
                }
            }
        }
        if (names) {
            char buf[64]; snprintf(buf,sizeof(buf),"%s [%.0fm]",name.c_str(),dist);
            ImVec2 sc;
            if (EspRenderer::worldToScreen(ex, ey+2.0, ez, sc)) {
                ImVec2 ts=ImGui::CalcTextSize(buf);
                ImDrawList* dl=ImGui::GetBackgroundDrawList();
                dl->AddRectFilled(ImVec2(sc.x-ts.x/2-2,sc.y),ImVec2(sc.x+ts.x/2+2,sc.y+ts.y),IM_COL32(0,0,0,160),3);
                dl->AddText(ImVec2(sc.x-ts.x/2,sc.y),col,buf);
            }
        }
    }
    env->PopLocalFrame(nullptr);
}
