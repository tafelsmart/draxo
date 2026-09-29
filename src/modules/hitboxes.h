#pragma once
#include "modules/module.h"
class HitBoxes : public Module {
public:
    HitBoxes();
    void onUpdate(JNIEnv* env) override;
private:
    static inline bool s_init = false;
    static inline jfieldID s_entityRange = nullptr;
    static inline jmethodID s_getAttr = nullptr, s_setBase = nullptr;
};
