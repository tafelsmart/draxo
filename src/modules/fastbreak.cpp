#include "pch.h"
#include "core/strcrypt.h"
#include "modules/fastbreak.h"
#include "sdk/minecraft.h"
#include "core/jvm_wrapper.h"

FastBreak::FastBreak() : Module("FastBreak", ModuleCategory::PLAYER, 0, "Increases block breaking speed by removing the destroy delay") {
}

void FastBreak::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;

    // Moderner Ansatz (1.21.x): Minecraft.destroySpeed existiert nicht mehr.
    // Stattdessen halten wir MultiPlayerGameMode.destroyDelay auf 0 — der
    // Server erlaubt damit sofort einen neuen Block-Abbau (FastBreak).
    static jfieldID s_gmField = nullptr, s_delayField = nullptr;
    static bool init = false;
    if (!init) {
        jobject mc = CMinecraft::getInstance();
        if (mc) {
            jclass mcC = env->GetObjectClass(mc);
            s_gmField = env->GetFieldID(mcC, Mappings::MC_gameMode, Mappings::MC_gameMode_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_gmField = nullptr; }
            env->DeleteLocalRef(mcC);
            env->DeleteLocalRef(mc);
        }
        if (s_gmField) {
            jobject mc2 = CMinecraft::getInstance();
            if (mc2) {
                jobject gm = env->GetObjectField(mc2, s_gmField);
                if (gm) {
                    jclass gmC = env->GetObjectClass(gm);
                    s_delayField = env->GetFieldID(gmC,
                        Mappings::GameMode_destroyDelay, Mappings::GameMode_destroyDelay_Sig);
                    if (env->ExceptionCheck()) { env->ExceptionClear(); s_delayField = nullptr; }
                    env->DeleteLocalRef(gmC);
                    env->DeleteLocalRef(gm);
                }
                env->DeleteLocalRef(mc2);
            }
        }
        init = true;
    }
    if (!s_gmField || !s_delayField) return;

    jobject mc = CMinecraft::getInstance();
    if (mc) {
        jobject gm = env->GetObjectField(mc, s_gmField);
        if (gm) {
            env->SetIntField(gm, s_delayField, 0);
            env->DeleteLocalRef(gm);
        }
        env->DeleteLocalRef(mc);
    }
}
