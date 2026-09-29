#pragma once
#include "modules/module.h"
class AutoClicker : public Module {
public:
    AutoClicker();
    void onUpdate(JNIEnv* env) override;
private:
    long long m_last=0;
    static inline bool s_jni = false;
    static inline jclass s_mcCls = nullptr, s_gmCls = nullptr, s_handCls = nullptr;
    static inline jfieldID s_gmField = nullptr, s_crosshair = nullptr, s_mainHand = nullptr;
    static inline jmethodID s_gmAttack = nullptr, s_playerSwing = nullptr;
};
