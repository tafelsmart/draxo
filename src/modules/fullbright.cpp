#include "pch.h"
#include "core/strcrypt.h"
#include "modules/fullbright.h"
#include "sdk/minecraft.h"
#include "core/jvm_wrapper.h"
#include "config/mappings.h"

FullBright::FullBright() : Module("FullBright", ModuleCategory::RENDER, 0,
    "Sets maximum brightness so you can see in the dark") {
    defineFloat("gamma", "Gamma", 12.0f, 1.0f, 25.0f, "%.0f");
    addSearchTag("bright");
    addSearchTag("light");
    addSearchTag("vision");
}

void FullBright::onDisable() {
    // Restore default gamma = 1.0
    JNIEnv* env = JvmWrapper::getEnv();
    if (!env) return;
    setGamma(env, 1.0);
}

void FullBright::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;
    float g = m_floatSettings["gamma"];
    setGamma(env, g);
}

void FullBright::setGamma(JNIEnv* env, double value) {
    // ── One-time JNI init ──────────────────────────────────────────
    static jfieldID s_optGamma = nullptr;
    static jmethodID s_setGamma = nullptr;
    static bool s_init = false;

    if (!s_init) {
        jclass optsCls = JvmWrapper::findClass(Mappings::Options_Class);
        if (optsCls) {
            s_optGamma = env->GetFieldID(optsCls,
                Mappings::Options_gamma,
                Mappings::Options_gamma_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_optGamma = nullptr; }
        }
        if (s_optGamma) {
            jclass optInstCls = JvmWrapper::findClass("net/minecraft/client/OptionInstance");
            if (optInstCls) {
                s_setGamma = env->GetMethodID(optInstCls, "set", "(Ljava/lang/Object;)V");
                if (env->ExceptionCheck()) { env->ExceptionClear(); s_setGamma = nullptr; }
            }
        }
        s_init = true;
    }

    if (!s_optGamma || !s_setGamma) return;

    // ── Get Options from Minecraft ─────────────────────────────────
    jobject mc = CMinecraft::getInstance();
    if (!mc) return;

    jclass mcCls = env->GetObjectClass(mc);
    jfieldID optsField = env->GetFieldID(mcCls,
        Mappings::MC_options, Mappings::MC_options_Sig);
    env->DeleteLocalRef(mcCls);
    if (!optsField) { env->DeleteLocalRef(mc); return; }

    jobject opts = env->GetObjectField(mc, optsField);
    if (!opts || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(mc);
        return;
    }

    // ── Read gamma OptionInstance and set new value ────────────────
    jobject gammaOpt = env->GetObjectField(opts, s_optGamma);
    if (gammaOpt && !env->ExceptionCheck()) {
        jclass doubleCls = env->FindClass("java/lang/Double");
        jmethodID valOf = env->GetStaticMethodID(doubleCls, "valueOf", "(D)Ljava/lang/Double;");
        if (valOf) {
            jobject gVal = env->CallStaticObjectMethod(doubleCls, valOf, value);
            if (gVal && !env->ExceptionCheck()) {
                env->CallVoidMethod(gammaOpt, s_setGamma, gVal);
                if (env->ExceptionCheck()) env->ExceptionClear();
                env->DeleteLocalRef(gVal);
            }
        }
        env->DeleteLocalRef(doubleCls);
        env->DeleteLocalRef(gammaOpt);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();

    env->DeleteLocalRef(opts);
    env->DeleteLocalRef(mc);
}
