#pragma once
#include "modules/module.h"
class TriggerBot : public Module {
public:
    TriggerBot();
    void onUpdate(JNIEnv* env) override;
private:
    static inline bool s_jniInit = false;
    static inline jclass s_mcCls = nullptr, s_gmCls = nullptr, s_handCls = nullptr;
    static inline jfieldID s_crosshair = nullptr, s_gmField = nullptr, s_mainHand = nullptr;
    static inline jmethodID s_gmAttack = nullptr, s_playerSwing = nullptr;
};
