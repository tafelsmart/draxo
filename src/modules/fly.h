#pragma once
#include "modules/module.h"

class Fly : public Module {
public:
    Fly();
    void onEnable() override;
    void onDisable() override;
    void onUpdate(JNIEnv* env) override;
private:
    enum Mode { Vanilla = 0, Packet = 1, Motion = 2 };
};
