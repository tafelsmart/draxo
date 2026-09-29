#include "pch.h"
#include "core/strcrypt.h"
#include "modules/disabler.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"
#include "core/jvm_wrapper.h"
#include "config/mappings.h"

Disabler::Disabler() : Module("Disabler", ModuleCategory::EXPLOIT, 0, "Anti-cheat bypass engine — timer spoof, flag reset, ground spoof") {
    defineGroup("Bypass");
    defineBool("timer_spoof",   "Timer Spoof (20 TPS)",    true);
    defineBool("flag_reset",    "Reset Fly/Spectate Flags",true);
    defineBool("ground_spoof",  "Ground Spoof",            true);
    defineGroupEnd();

    defineGroup("Expert");
    defineFloat("timer_target", "Target TPS", 20.0f, 15.0f, 25.0f, "%.1f");
    defineBool("verbose",       "Verbose Logging", false);
    defineGroupEnd();

    addSearchTag("bypass");
    addSearchTag("watchdog");
    addSearchTag("anticheat");
    addSearchTag("ac");
}

void Disabler::onEnable() {
    m_timerNoise.reset();
}

void Disabler::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;

    // One-time JNI init
    if (!s_jni) {
        s_entityClass = JvmWrapper::findClass(Mappings::Entity_Class);
        if (s_entityClass) {
            s_onGround = env->GetFieldID(s_entityClass, Mappings::Entity_onGround,
                                         Mappings::Entity_onGround_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_onGround = nullptr; }
        }

        jobject mc = CMinecraft::getInstance();
        if (mc) {
            jclass mcC = env->GetObjectClass(mc);
            s_timerField = env->GetFieldID(mcC, Mappings::MC_timer, Mappings::MC_timer_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_timerField = nullptr; }
            if (s_timerField) {
                jobject timer = env->GetObjectField(mc, s_timerField);
                if (timer) {
                    jclass tC = env->GetObjectClass(timer);
                    s_tpsField = env->GetFieldID(tC, Mappings::Timer_msPerTick,
                                                  Mappings::Timer_msPerTick_Sig);
                    if (env->ExceptionCheck()) { env->ExceptionClear(); s_tpsField = nullptr; }
                    env->DeleteLocalRef(timer);
                }
            }
            env->DeleteLocalRef(mc);
        }
        s_jni = true;
        printf(STR_C("[Draxo] Disabler JNI: timer=%p tps=%p ground=%p\n"),
               (void*)s_timerField, (void*)s_tpsField, (void*)s_onGround);
    }

    // ── Timer Spoof (humanized) ───────────────────────────────────
    // ECHTE Netzwerk-/Server-Tickraten haben Jitter: ±0.5ms um 50ms,
    // plus gelegentliche Lag-Spikes von 2-5ms für 150-400ms.
    // GrimAC erkennt "perfekte 50.000ms" als gespooften Timer.
    if (m_boolSettings["timer_spoof"] && s_timerField && s_tpsField) {
        jobject mc = CMinecraft::getInstance();
        if (mc) {
            jobject timer = env->GetObjectField(mc, s_timerField);
            if (timer) {
                float baseMs = 1000.0f / m_floatSettings["timer_target"];
                // Add human-like jitter: ±0.5ms normal, ±5ms during lag spikes
                float jittered = baseMs + m_timerNoise.noiseMs(1.0f / 20.0f);
                if (jittered < 48.0f) jittered = 48.0f;
                if (jittered > 52.0f) jittered = 52.0f;
                env->SetFloatField(timer, s_tpsField, jittered);
                if (env->ExceptionCheck()) env->ExceptionClear();
                env->DeleteLocalRef(timer);
            }
            env->DeleteLocalRef(mc);
        }
    }

    // ── Flag Reset ────────────────────────────────────────────────
    // Modules like Fly set onGround=true which can trigger isFlying
    // and isSpectator flags on the server side. We periodically reset
    // these flags via setSharedFlag to prevent ban waves.
    if (m_boolSettings["flag_reset"]) {
        jobject p = CMinecraft::getPlayer();
        if (p) {
            CEntity pl(p);
            // Reset shared flags: bit 7 = IS_FLYING, bit 8 = IS_SPECTATOR
            // These get set when the server detects impossible movement patterns
            // and can lead to automatic bans. Clearing them every tick prevents
            // the server from accumulating evidence.
            static jmethodID s_setFlag = nullptr;
            if (!s_setFlag) {
                jclass eC = env->GetObjectClass(p);
                s_setFlag = env->GetMethodID(eC, Mappings::Entity_setSharedFlag,
                                             Mappings::Entity_setSharedFlag_Sig);
                if (env->ExceptionCheck()) { env->ExceptionClear(); s_setFlag = nullptr; }
            }
            if (s_setFlag) {
                // bit 7 = flying flag, bit 8 = spectator flag
                env->CallVoidMethod(p, s_setFlag, (jint)7, JNI_FALSE);
                if (env->ExceptionCheck()) env->ExceptionClear();
                env->CallVoidMethod(p, s_setFlag, (jint)8, JNI_FALSE);
                if (env->ExceptionCheck()) env->ExceptionClear();
            }
            env->DeleteLocalRef(p);
        }
    }

    // ── Ground Spoof ───────────────────────────────────────────────
    // Forces onGround=true. Without this, Speed/Fly modules cause the
    // server to see "player floating mid-air for N ticks" which is
    // detected as flight hack. Setting onGround every tick hides this.
    if (m_boolSettings["ground_spoof"] && s_onGround) {
        jobject p = CMinecraft::getPlayer();
        if (p) {
            env->SetBooleanField(p, s_onGround, JNI_TRUE);
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(p);
        }
    }
}
