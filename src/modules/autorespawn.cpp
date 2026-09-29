#include "pch.h"
#include "core/strcrypt.h"
#include "modules/autorespawn.h"
#include "sdk/minecraft.h"
#include "core/jvm_wrapper.h"
#include "config/mappings.h"

AutoRespawn::AutoRespawn() : Module("AutoRespawn", ModuleCategory::PLAYER, 0, "Automatically clicks the respawn button when you die") {
    setTickInterval(10);
    addSearchTag("death");
    addSearchTag("respawn");
}

void AutoRespawn::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;
    jobject p = CMinecraft::getPlayer();
    if (!p) return;

    if (!s_init) {
        jclass livingCls = JvmWrapper::findClass(Mappings::LivingEntity_Class);
        if (livingCls) {
            s_isDead = env->GetMethodID(livingCls, Mappings::LivingEntity_isDeadOrDying, Mappings::LivingEntity_isDeadOrDying_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_isDead = nullptr; }
        }
        s_init = true;
    }

    bool dead = false;
    if (s_isDead) dead = env->CallBooleanMethod(p, s_isDead) == JNI_TRUE;
    env->DeleteLocalRef(p);

    if (dead) {
        HWND h = FindWindowA("GLFW30", nullptr);
        if (h && GetForegroundWindow() == h) {
            // Non-blocking click (no Sleep)
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
            m_clickDown = true;
        }
    }
}

void AutoRespawn::onRender() {
    if (m_clickDown) {
        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
        m_clickDown = false;
    }
}
