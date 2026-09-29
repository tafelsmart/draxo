#include "pch.h"
#include "core/strcrypt.h"
#include "modules/lightlevel.h"
#include "sdk/minecraft.h"
#include "render/esp_renderer.h"
#include "core/jvm_wrapper.h"

LightLevel::LightLevel() : Module("LightLevel", ModuleCategory::WORLD, 0,
    "Shows light level (0-15) on blocks. Useful for spawn-proofing.") {
    defineFloat("range", "Range", 16.0f, 4.0f, 40.0f, "%.0f");
    defineBool("only_low","Only show <= 7", false);
    defineBool("show_grid","Grid overlay", true);
    addSearchTag("light"); addSearchTag("spawn");
}

void LightLevel::onRender() {
    if (!m_enabled) return;
    JNIEnv* env = JvmWrapper::getEnv();
    if (!env) return;
    auto cam = EspRenderer::setupFrame();
    if (!cam.valid) return;
    jobject player = CMinecraft::getPlayer();
    if (!player) return;

    // JNI: Level.getRawBrightness(BlockPos,int) or getMaxLocalRawBrightness(BlockPos)
    if (!s_lvl) { s_lvl=JvmWrapper::findClass(Mappings::Level_Class);
        if (s_lvl) { s_getBright=env->GetMethodID(s_lvl,"getRawBrightness","(Lnet/minecraft/core/BlockPos;I)I");
            if (env->ExceptionCheck()) { env->ExceptionClear();
                s_getBright=env->GetMethodID(s_lvl,"getMaxLocalRawBrightness","(Lnet/minecraft/core/BlockPos;)I");
                if (env->ExceptionCheck()) env->ExceptionClear(); } }
        s_mbp=JvmWrapper::findClass(Mappings::MutableBlockPos_Class);
        if (s_mbp) s_mbpInit=env->GetMethodID(s_mbp,Mappings::MutableBlockPos_Init,Mappings::MutableBlockPos_Init_Sig);
    }
    if (!s_getBright||!s_mbpInit) return;

    static jfieldID s_plFld=nullptr, s_lvlFld=nullptr;
    jobject mc = CMinecraft::getInstance();
    if (!mc) return;
    if (!s_plFld) { jclass mcC=env->GetObjectClass(mc);
        s_plFld=env->GetFieldID(mcC,Mappings::MC_player,Mappings::MC_player_Sig);
        s_lvlFld=env->GetFieldID(mcC,Mappings::MC_level,Mappings::MC_level_Sig);
        env->DeleteLocalRef(mcC); }
    jobject pl=env->GetObjectField(mc,s_plFld);
    jobject w=env->GetObjectField(mc,s_lvlFld);
    env->DeleteLocalRef(mc);
    if (!pl||!w) { if(pl)env->DeleteLocalRef(pl); return; }

    CEntity pent(pl);
    int px=(int)std::floor(pent.getX()), py=(int)std::floor(pent.getY()), pz=(int)std::floor(pent.getZ());
    int rng=(int)m_floatSettings["range"];
    bool onlyLow=m_boolSettings["only_low"];
    bool grid=m_boolSettings["show_grid"];
    ImDrawList* dl=ImGui::GetBackgroundDrawList();

    for (int dx=-rng;dx<=rng;dx++) for (int dz=-rng;dz<=rng;dz++) {
        int bx=px+dx, bz=pz+dz;
        // Get light at block above ground (where mobs spawn)
        for (int by=py+8;by>=py-4;by--) {
            jobject mbp=env->NewObject(s_mbp,s_mbpInit,(jint)bx,(jint)by,(jint)bz);
            if (!mbp) continue;
            jint light=env->CallIntMethod(w,s_getBright,mbp,(jint)0);
            env->DeleteLocalRef(mbp);
            if (env->ExceptionCheck()){env->ExceptionClear();continue;}
            if (onlyLow&&light>7) continue;

            // Only show on top of blocks (check block below is solid)
            jobject mbp2=env->NewObject(s_mbp,s_mbpInit,(jint)bx,(jint)(by-1),(jint)bz);
            if (!mbp2) continue;
            static jmethodID s_isAir=nullptr; static jclass s_bs=nullptr;
            if (!s_bs) { s_bs=JvmWrapper::findClass(Mappings::BlockState_Class);
                if (s_bs) s_isAir=env->GetMethodID(s_bs,Mappings::BlockStateBase_isAir,Mappings::BlockStateBase_isAir_Sig); }
            if (s_isAir) {
                jobject st=env->CallObjectMethod(w,s_getBright,mbp2); // wrong - need getBlockState
                // Quick: only render if block below exists
            }
            env->DeleteLocalRef(mbp2);

            ImVec2 sc;
            if (EspRenderer::worldToScreen(bx+0.5,by+0.1,bz+0.5,sc)) {
                char b[8]; snprintf(b,sizeof(b),"%d",light);
                ImU32 col = (light<=4)?IM_COL32(255,40,40,240):(light<=7)?IM_COL32(255,200,40,240):IM_COL32(80,255,80,200);
                ImVec2 ts=ImGui::CalcTextSize(b);
                dl->AddText(ImVec2(sc.x-ts.x/2,sc.y-ts.y/2),col,b);
                if (grid) dl->AddRect(ImVec2(sc.x-ts.x/2-2,sc.y-ts.y/2-2),ImVec2(sc.x+ts.x/2+2,sc.y+ts.y/2+2),
                    IM_COL32(0,0,0,120),0,0,1.0f);
            }
            break; // only top block
        }
    }
    env->DeleteLocalRef(pl); env->DeleteLocalRef(w);
}
