#include "pch.h"
#include "core/strcrypt.h"
#include "modules/lowfire.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"
#include "core/jvm_wrapper.h"

LowFire::LowFire() : Module("LowFire", ModuleCategory::RENDER, 0,
    "Reduces the fire overlay height when burning. Adjustable scale.") {
    setTickInterval(1);
    defineFloat("scale", "Height %", 0.35f, 0.0f, 1.0f, "%.0f%%");
    addSearchTag("fire");
}

void LowFire::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;
    float scale = m_floatSettings["scale"];
    if (scale >= 1.0f) return;  // no reduction needed

    jobject player = CMinecraft::getPlayer();
    if (!player) return;

    // Entity.setRemainingFireTicks(I)V — multi-version lookup
    static jmethodID s_getFire = nullptr;
    static jmethodID s_setFire = nullptr;
    static bool s_init = false;
    if (!s_init) {
        s_init = true;
        jclass entCls = JvmWrapper::findClass(Mappings::Entity_Class);
        if (entCls) {
            // Try modern name first
            s_getFire = env->GetMethodID(entCls, "getRemainingFireTicks", "()I");
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                s_getFire = env->GetMethodID(entCls, "getFireTicks", "()I");
                if (env->ExceptionCheck()) env->ExceptionClear();
            }
            s_setFire = env->GetMethodID(entCls, "setRemainingFireTicks", "(I)V");
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                s_setFire = env->GetMethodID(entCls, "setFireTicks", "(I)V");
                if (env->ExceptionCheck()) env->ExceptionClear();
            }
        }
    }
    if (!s_getFire || !s_setFire) return;

    jint ticks = env->CallIntMethod(player, s_getFire);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
    if (ticks <= 0) return;

    jint scaled = (jint)(ticks * scale);
    env->CallVoidMethod(player, s_setFire, scaled);
    if (env->ExceptionCheck()) env->ExceptionClear();
}
