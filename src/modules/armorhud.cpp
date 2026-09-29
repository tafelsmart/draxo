#include "pch.h"
#include "core/strcrypt.h"
#include "modules/armorhud.h"
#include "sdk/minecraft.h"
#include "core/jvm_wrapper.h"

ArmorHUD::ArmorHUD() : Module("ArmorHUD", ModuleCategory::RENDER, 0,
    "Shows equipped armor with durability bars.") {
    defineFloat("x","X",4.0f,0,1920,"%.0f");
    defineFloat("y","Y",300.0f,0,1080,"%.0f");
    defineBool("vert","Vertical layout", true);
    addSearchTag("armor"); addSearchTag("durability");
}

void ArmorHUD::onRender() {
    if (!m_enabled) return;
    JNIEnv* env = JvmWrapper::getEnv();
    if (!env) return;
    jobject player = CMinecraft::getPlayer();
    if (!player) return;

    static jmethodID s_getArmorSlots=nullptr; static jclass s_playerCls=nullptr;
    static bool sj=false;
    if (!sj){sj=true;
        s_playerCls=JvmWrapper::findClass(Mappings::Player_Class);
        if (s_playerCls){s_getArmorSlots=env->GetMethodID(s_playerCls,"getArmorSlots","()Ljava/lang/Iterable;");
            if (env->ExceptionCheck()){env->ExceptionClear();s_getArmorSlots=nullptr;}}
    }
    if (!s_getArmorSlots) return;

    jobject armors=env->CallObjectMethod(player,s_getArmorSlots);
    if (!armors||env->ExceptionCheck()){env->ExceptionClear();return;}

    static jmethodID s_hover=nullptr,s_getStr=nullptr,s_dmg=nullptr,s_maxDmg=nullptr;
    static bool s2=false;
    if (!s2){s2=true;
        jclass isCls=JvmWrapper::findClass(Mappings::ItemStack_Class);
        jclass cpCls=JvmWrapper::findClass(Mappings::Component_Class);
        if (isCls){s_hover=env->GetMethodID(isCls,Mappings::ItemStack_getHoverName,Mappings::ItemStack_getHoverName_Sig);
            s_dmg=env->GetMethodID(isCls,"getDamageValue","()I");
            s_maxDmg=env->GetMethodID(isCls,"getMaxDamage","()I");
            if (env->ExceptionCheck())env->ExceptionClear();}
        if (cpCls){s_getStr=env->GetMethodID(cpCls,Mappings::Component_getString,Mappings::Component_getString_Sig);}
    }

    float x=m_floatSettings["x"],y=m_floatSettings["y"];
    ImDrawList* dl=ImGui::GetBackgroundDrawList();
    float rowH=16, barW=80; int i=0;

    static jmethodID s_it=nullptr,s_nxt=nullptr,s_hn=nullptr;
    if (!s_it){jclass ic=JvmWrapper::findClass("java/util/Iterator");
        if (ic){s_hn=env->GetMethodID(ic,"hasNext","()Z");s_nxt=env->GetMethodID(ic,"next","()Ljava/lang/Object;");}}
    if (!s_nxt)return;
    static jmethodID s_iter=nullptr; static jclass s_ic=nullptr;
    if (!s_ic){s_ic=JvmWrapper::findClass("java/lang/Iterable");
        if(s_ic)s_iter=env->GetMethodID(s_ic,"iterator","()Ljava/util/Iterator;");}
    if (!s_iter)return;
    jobject it=env->CallObjectMethod(armors,s_iter);
    env->DeleteLocalRef(armors);
    if (!it)return;

    while (env->CallBooleanMethod(it,s_hn)) {
        jobject stack=env->CallObjectMethod(it,s_nxt);
        if (!stack||env->ExceptionCheck()){env->ExceptionClear();continue;}
        jint dmg=env->CallIntMethod(stack,s_dmg);
        jint mx=env->CallIntMethod(stack,s_maxDmg);
        if (env->ExceptionCheck()){env->ExceptionClear();env->DeleteLocalRef(stack);continue;}
        if (mx<=0){env->DeleteLocalRef(stack);continue;}

        jobject hover=env->CallObjectMethod(stack,s_hover);
        const char* nm="Armor";
        if (hover&&!env->ExceptionCheck()){jstring js=(jstring)env->CallObjectMethod(hover,s_getStr);
            if (js){nm=env->GetStringUTFChars(js,nullptr);} if (js)env->DeleteLocalRef(js);}
        float ratio=1.0f-(float)dmg/(float)mx;
        ImU32 col=(ratio>0.6f)?IM_COL32(80,200,80,220):(ratio>0.3f)?IM_COL32(220,180,40,220):IM_COL32(220,60,60,220);

        float ry=y+i*rowH;
        dl->AddRectFilled(ImVec2(x,ry),ImVec2(x+barW,ry+rowH-2),IM_COL32(0,0,0,140),3);
        dl->AddRectFilled(ImVec2(x,ry+rowH-5),ImVec2(x+barW*ratio,ry+rowH-2),col);
        if (nm){dl->AddText(ImVec2(x+4,ry),(ImU32)IM_COL32(255,255,255,255),nm);}
        if (hover)env->DeleteLocalRef(hover);
        env->DeleteLocalRef(stack); i++;
    }
    env->DeleteLocalRef(it);
}
