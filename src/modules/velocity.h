#pragma once
#include "modules/module.h"
#include "core/ac_bypass.h"

class Velocity : public Module {
public:
    Velocity();
    void onEnable() override;
    void onUpdate(JNIEnv* env) override;

private:
    ac::KBHumanizer m_humanizer;      // unified reaction curve + overshoot + inertia
    int   m_ticksLeft   = 0;          // legacy
    float m_randFactor  = 1.0f;       // legacy
};
