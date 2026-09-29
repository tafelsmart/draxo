#include "pch.h"
#include "core/strcrypt.h"
#include "modules/timer.h"
#include "sdk/minecraft.h"
#include "core/jvm_wrapper.h"
#include "config/mappings.h"

TimerM::TimerM() : Module("Timer", ModuleCategory::MOVEMENT, 0, "Speeds up game ticks for faster movement") {
    defineFloat("speed", "Speed", 1.5f, 0.1f, 10.0f, "%.1f");
    addSearchTag("gamespeed");
}

void TimerM::onEnable() { m_normalTPS = 20.0f; }
void TimerM::onDisable() {}

void TimerM::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;

    // ── Detect warnings ──────────────────────────────────────────────
    float tspeed = m_floatSettings["speed"];
    if (tspeed > 1.3f)
        addDetectWarning("Timer > 1.3x", true,
            "Timer above 1.3x is detected by GrimAC (tick-time check). Reduce to 1.1-1.25.");
    if (tspeed > 1.8f)
        addDetectWarning("Timer > 1.8x", true,
            "Extreme timer speed. Watchdog will ban within seconds at this rate.");
    if (tspeed > 2.5f)
        addDetectWarning("Timer > 2.5x", true,
            "Impossible tick rate. All anticheats detect this instantly. Use Disabler instead.");
    if (tspeed < 0.5f && tspeed > 0.0f)
        addDetectWarning("Timer < 0.5x", true,
            "Slow-down timer is still detectable (GrimAC notices delayed ticks).");

    if (!s_init) {
        jobject mc = CMinecraft::getInstance();
        if (mc) {
            jclass mcCls = env->GetObjectClass(mc);
            s_timerField = env->GetFieldID(mcCls, Mappings::MC_timer, Mappings::MC_timer_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_timerField = nullptr; }
            if (s_timerField) {
                jobject timer = env->GetObjectField(mc, s_timerField);
                if (timer) {
                    jclass tCls = env->GetObjectClass(timer);
                    s_tpsField = env->GetFieldID(tCls, Mappings::Timer_msPerTick, Mappings::Timer_msPerTick_Sig);
                    if (env->ExceptionCheck()) { env->ExceptionClear(); s_tpsField = nullptr; }
                    env->DeleteLocalRef(timer); env->DeleteLocalRef(tCls);
                }
            }
            env->DeleteLocalRef(mcCls); env->DeleteLocalRef(mc);
        }
        s_init = true;
    }
    if (!s_timerField || !s_tpsField) return;
    jobject mc = CMinecraft::getInstance();
    if (!mc) return;
    jobject timer = env->GetObjectField(mc, s_timerField);
    if (timer) {
        float target = 1000.0f / (m_normalTPS * m_floatSettings["speed"]);
        env->SetFloatField(timer, s_tpsField, target);
        env->DeleteLocalRef(timer);
    }
    env->DeleteLocalRef(mc);
}
