#pragma once
#include "modules/module.h"

class FullBright : public Module {
public:
    FullBright();
    void onDisable() override;
    void onUpdate(JNIEnv* env) override;

private:
    void setGamma(JNIEnv* env, double value);
};
