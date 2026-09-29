#include "pch.h"
#include "core/strcrypt.h"
#include "modules/fastplace.h"
#include "sdk/minecraft.h"
#include "core/jvm_wrapper.h"

FastPlace::FastPlace() : Module("FastPlace", ModuleCategory::PLAYER, 0, "Removes the block placement delay for faster building") {
    defineFloat("delay", "Place Delay (ms)", 0.0f, 0.0f, 4.0f, "%.0f");
}

void FastPlace::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;

    static jfieldID s_field = nullptr;
    static bool init = false;
    if (!init) {
        jobject mc = CMinecraft::getInstance();
        if (mc) {
            jclass c = env->GetObjectClass(mc);
            s_field = env->GetFieldID(c, Mappings::MC_rightClickDelay, Mappings::MC_rightClickDelay_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_field = nullptr; }
            env->DeleteLocalRef(c);
            env->DeleteLocalRef(mc);
        }
        init = true;
    }
    if (!s_field) return;

    jobject mc = CMinecraft::getInstance();
    if (mc) {
        env->SetIntField(mc, s_field, (jint)m_floatSettings["delay"]);
        env->DeleteLocalRef(mc);
    }
}
