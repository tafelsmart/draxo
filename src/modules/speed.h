#pragma once
#include "modules/module.h"

class Speed : public Module {
public:
    Speed();
    void onUpdate(JNIEnv* env) override;
    void onDisable() override;
private:
    enum Mode { Multiplier = 0, BHop = 1, Strafe = 2 };
    int m_bhopStage = 0;
};
