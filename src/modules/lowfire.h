#pragma once
#include "modules/module.h"
class LowFire : public Module {
public:
    LowFire();
    void onUpdate(JNIEnv* env) override;
};
