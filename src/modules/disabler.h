#pragma once
#include "modules/module.h"
#include "core/ac_humanizer.h"

/*
 * Disabler v3 — Anti-cheat bypass engine with humanized jitter.
 *
 *   Timer  — forces 50ms with natural ±0.5ms jitter + lag spikes
 *   Flags  — resets suspicious movement flags (isFlying, isSpectator)
 *   Ground — forces onGround with random transition delays
 */

class Disabler : public Module {
public:
    Disabler();
    void onEnable() override;
    void onUpdate(JNIEnv* env) override;

private:
    static inline bool s_jni = false;
    static inline jfieldID s_timerField = nullptr, s_tpsField = nullptr;
    static inline jfieldID s_onGround = nullptr;
    static inline jclass s_entityClass = nullptr;
    ac::TimerNoiser m_timerNoise;
};
