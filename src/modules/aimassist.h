#pragma once
#include "modules/module.h"
#include <chrono>

class AimAssist : public Module {
public:
    AimAssist();
    void onUpdate(JNIEnv* env) override;

private:
    float getAngleDifference(float a, float b);
    float wrapAngleTo180(float angle);

    long long m_lastAttack = 0;
    // JNI cache
    static inline bool s_jni = false;
    static inline jclass s_mcCls = nullptr, s_gmCls = nullptr, s_handCls = nullptr;
    static inline jfieldID s_gmField = nullptr, s_mainHand = nullptr, s_crosshair = nullptr;
    static inline jmethodID s_gmAttack = nullptr, s_playerSwing = nullptr, s_attackScale = nullptr;
};
