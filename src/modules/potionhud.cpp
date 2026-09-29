#include "pch.h"
#include "core/strcrypt.h"
#include "modules/potionhud.h"
#include "sdk/minecraft.h"
#include "core/jvm_wrapper.h"

PotionHUD::PotionHUD() : Module("PotionHUD", ModuleCategory::RENDER, 0,
    "Displays active potion effects with timers and duration bars.") {
    defineFloat("x","X Position", 4.0f, 0, 1920, "%.0f");
    defineFloat("y","Y Position", 60.0f, 0, 1080, "%.0f");
    defineFloat("w","Width",      120.0f, 60, 300, "%.0f");
    defineBool("timers","Show Timers", true);
    addSearchTag("potion"); addSearchTag("effect");
}

void PotionHUD::onRender() {
    if (!m_enabled) return;
    JNIEnv* env = JvmWrapper::getEnv();
    if (!env) return;

    jobject player = CMinecraft::getPlayer();
    if (!player) return;

    // JNI: player.getActiveEffects() → Collection<MobEffectInstance>
    static jmethodID s_getFX=nullptr; static jclass s_livingCls=nullptr;
    static jmethodID s_it=nullptr, s_hn=nullptr, s_nxt=nullptr;
    static jclass s_mobFXCls=nullptr; static jmethodID s_fxDescId=nullptr, s_fxDur=nullptr, s_fxAmp=nullptr;
    static bool sj=false;
    if (!sj) { sj=true;
        s_livingCls=JvmWrapper::findClass(Mappings::LivingEntity_Class);
        if (s_livingCls) { s_getFX=env->GetMethodID(s_livingCls,"getActiveEffects","()Ljava/util/Collection;");
            if (env->ExceptionCheck()){env->ExceptionClear();s_getFX=nullptr;} }
        s_mobFXCls=JvmWrapper::findClass("net/minecraft/world/effect/MobEffectInstance");
        if (s_mobFXCls) { s_fxDur=env->GetMethodID(s_mobFXCls,"getDuration","()I");
            s_fxAmp=env->GetMethodID(s_mobFXCls,"getAmplifier","()I");
            s_fxDescId=env->GetMethodID(s_mobFXCls,"getDescriptionId","()Ljava/lang/String;");
        }
        jclass iterCls=JvmWrapper::findClass("java/util/Iterator");
        if (iterCls) { s_hn=env->GetMethodID(iterCls,"hasNext","()Z"); s_nxt=env->GetMethodID(iterCls,"next","()Ljava/lang/Object;"); }
    }
    if (!s_getFX||!s_fxDur) return;

    jobject coll = env->CallObjectMethod(player, s_getFX);
    if (!coll||env->ExceptionCheck()){env->ExceptionClear();return;}
    static jmethodID s_collIt=nullptr; static jclass s_collCls=nullptr;
    if (!s_collCls){s_collCls=JvmWrapper::findClass("java/util/Collection");
        if (s_collCls) s_collIt=env->GetMethodID(s_collCls,"iterator","()Ljava/util/Iterator;");}
    if (!s_collIt){env->DeleteLocalRef(coll);return;}
    jobject iter=env->CallObjectMethod(coll,s_collIt);
    env->DeleteLocalRef(coll);
    if (!iter) return;

    float x=m_floatSettings["x"], y=m_floatSettings["y"], w=m_floatSettings["w"];
    bool timers=m_boolSettings["timers"];
    ImDrawList* dl=ImGui::GetBackgroundDrawList();
    float rowH=18; int i=0;

    while (env->CallBooleanMethod(iter,s_hn)) {
        jobject fx=env->CallObjectMethod(iter,s_nxt);
        if (!fx||env->ExceptionCheck()){env->ExceptionClear();continue;}
        jint dur=env->CallIntMethod(fx,s_fxDur);
        jint amp=env->CallIntMethod(fx,s_fxAmp);
        if (env->ExceptionCheck()){env->ExceptionClear();env->DeleteLocalRef(fx);continue;}
        if (dur<=0){env->DeleteLocalRef(fx);continue;}

        jstring desc=(jstring)env->CallObjectMethod(fx,s_fxDescId);
        const char* cd=desc?env->GetStringUTFChars(desc,nullptr):nullptr;
        char line[64]; float sec=dur/20.0f;
        if (cd&&timers) snprintf(line,sizeof(line),"%s %s %d (%.0fs)",cd,"+",amp+1,sec);
        else if (cd) snprintf(line,sizeof(line),"%s %s %d",cd,"+",amp+1);
        else snprintf(line,sizeof(line),"Effect +%d",amp+1);
        if (cd){env->ReleaseStringUTFChars(desc,cd);}
        if (desc)env->DeleteLocalRef(desc);

        float ry=y+i*rowH;
        dl->AddRectFilled(ImVec2(x,ry),ImVec2(x+w,ry+rowH-2),IM_COL32(0,0,0,140),3);
        // Duration bar
        float ratio=(float)dur/600.0f; if(ratio>1)ratio=1;
        ImU32 bc=(ratio>0.5f)?IM_COL32(80,200,80,180):(ratio>0.2f)?IM_COL32(220,180,40,180):IM_COL32(220,60,60,180);
        dl->AddRectFilled(ImVec2(x,ry+rowH-4),ImVec2(x+w*ratio,ry+rowH-2),bc);
        dl->AddText(ImVec2(x+4,ry+1),IM_COL32(255,255,255,255),line);

        env->DeleteLocalRef(fx); i++;
    }
    env->DeleteLocalRef(iter);
}
