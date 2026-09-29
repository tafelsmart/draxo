#pragma once
#include "modules/module.h"

class NoSlow : public Module {
public:
    NoSlow();
    void onUpdate(JNIEnv* env) override;
    void onDisable() override;
};
