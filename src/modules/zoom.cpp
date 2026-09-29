#include "pch.h"
#include "core/strcrypt.h"
#include "modules/zoom.h"
#include "sdk/minecraft.h"
#include "core/jvm_wrapper.h"

Zoom::Zoom() : Module("Zoom", ModuleCategory::RENDER, 0, "Zooms your camera in for long-distance viewing") {
    m_floatSettings["factor"] = 30.0f; m_displayNames["factor"] = "Zoom FOV";
}

void Zoom::onEnable() {}
void Zoom::onDisable() {}

void Zoom::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;
    static jfieldID s_optFov = nullptr; static jmethodID s_set = nullptr;
    static jfieldID s_optsField = nullptr; static bool init = false;
    if (!init) {
        jclass optsCls = JvmWrapper::findClass("net/minecraft/client/Options");
        if (optsCls) {
            s_optFov = env->GetFieldID(optsCls, "fov", "Lnet/minecraft/client/OptionInstance;");
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_optFov = nullptr; }
        }
        if (s_optFov) {
            jclass oi = JvmWrapper::findClass("net/minecraft/client/OptionInstance");
            if (oi) s_set = env->GetMethodID(oi, "set", "(Ljava/lang/Object;)V");
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_set = nullptr; }
        }
        init = true;
    }
    if (!s_optFov || !s_set) return;
    jobject mc = CMinecraft::getInstance(); if (!mc) return;
    if (!s_optsField) s_optsField = env->GetFieldID(env->GetObjectClass(mc), "options", "Lnet/minecraft/client/Options;");
    if (s_optsField) {
        jobject opts = env->GetObjectField(mc, s_optsField);
        if (opts && !env->ExceptionCheck()) {
            jobject fovOpt = env->GetObjectField(opts, s_optFov);
            if (fovOpt) {
                jclass intCls = env->FindClass("java/lang/Integer");
                jmethodID valOf = env->GetStaticMethodID(intCls, "valueOf", "(I)Ljava/lang/Integer;");
                if (valOf) {
                    jobject v = env->CallStaticObjectMethod(intCls, valOf, (jint)m_floatSettings["factor"]);
                    env->CallVoidMethod(fovOpt, s_set, v);
                    env->DeleteLocalRef(v);
                }
                env->DeleteLocalRef(intCls); env->DeleteLocalRef(fovOpt);
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(opts);
        }
    }
    env->DeleteLocalRef(mc);
}
