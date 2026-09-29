#include "pch.h"
#include "core/strcrypt.h"
#include "modules/projectileesp.h"
#include "sdk/minecraft.h"
#include "sdk/world.h"
#include "render/esp_renderer.h"
#include "core/jvm_wrapper.h"

ProjectileESP::ProjectileESP() : Module("ProjectileESP", ModuleCategory::RENDER, 0,
    "Visualizes arrows, tridents, fireballs, and thrown projectiles with tracers.") {
    defineBool("arrows",     "Arrows",        true);
    defineBool("tridents",   "Tridents",      true);
    defineBool("fireballs",  "Fireballs",     true);
    defineBool("throwables", "Throwables",    true); // eggs, snowballs, potions
    defineFloat("max_dist",  "Max Distance",  64.0f, 8.0f, 256.0f, "%.0f");
    defineColor("c_arrow",   "Arrow Color",   IM_COL32(255, 220, 80, 230));
    defineColor("c_trident", "Trident Color", IM_COL32(80, 220, 255, 230));
    defineColor("c_fire",    "Fireball",      IM_COL32(255, 100, 40, 240));
    defineColor("c_throw",   "Throwable",     IM_COL32(200, 200, 200, 200));
    addSearchTag("arrow"); addSearchTag("projectile");
}

void ProjectileESP::onRender() {
    if (!m_enabled) return;
    JNIEnv* env = JvmWrapper::getEnv();
    if (!env) return;
    env->PushLocalFrame(4096);
    auto cam = EspRenderer::setupFrame();
    if (!cam.valid) { env->PopLocalFrame(nullptr); return; }
    jobject w = CMinecraft::getWorld();
    if (!w) { env->PopLocalFrame(nullptr); return; }

    auto ents = CWorld::getAllEntities(w);
    float md = m_floatSettings["max_dist"];
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    for (auto& e : ents) {
        if (e.isPlayer()||e.isItem()||e.isLiving()||e.isArmorStand()) continue;
        std::string n = e.getName();
        for (auto& c:n) c=(char)tolower((unsigned char)c);
        bool isArrow=false,isTri=false,isFire=false,isThr=false;
        ImU32 col = 0;
        if (n.find("arrow")!=std::string::npos&&n.find("spectral")==std::string::npos) {isArrow=m_boolSettings["arrows"];col=m_colorSettings["c_arrow"];}
        else if (n.find("trident")!=std::string::npos) {isTri=m_boolSettings["tridents"];col=m_colorSettings["c_trident"];}
        else if (n.find("fireball")!=std::string::npos||n.find("firework")!=std::string::npos||n.find("wither_skull")!=std::string::npos||n.find("dragon_fireball")!=std::string::npos) {isFire=m_boolSettings["fireballs"];col=m_colorSettings["c_fire"];}
        else if (n.find("snowball")!=std::string::npos||n.find("egg")!=std::string::npos||n.find("potion")!=std::string::npos||n.find("ender_pearl")!=std::string::npos||n.find("experience")!=std::string::npos||n.find("eye_of_ender")!=std::string::npos) {isThr=m_boolSettings["throwables"];col=m_colorSettings["c_throw"];}
        else continue;
        if (!isArrow&&!isTri&&!isFire&&!isThr) continue;

        double ex=e.getX(),ey=e.getY(),ez=e.getZ();
        double dx=ex-cam.x,dy=ey-cam.y,dz=ez-cam.z;
        if (std::sqrt(dx*dx+dy*dy+dz*dz)>md) continue;

        ImVec2 sc;
        if (EspRenderer::tracerToScreen(ex,ey,ez,sc))
            dl->AddLine(ImVec2(cam.w/2,cam.h/2),sc,col,1.2f);
        if (EspRenderer::worldToScreen(ex,ey,ez,sc)) {
            char b[32]; snprintf(b,sizeof(b),"%s",n.c_str());
            ImVec2 ts=ImGui::CalcTextSize(b);
            dl->AddRectFilled(ImVec2(sc.x-ts.x/2-2,sc.y),ImVec2(sc.x+ts.x/2+2,sc.y+ts.y),IM_COL32(0,0,0,160),3);
            dl->AddText(ImVec2(sc.x-ts.x/2,sc.y),col,b);
        }
    }
    env->PopLocalFrame(nullptr);
}
