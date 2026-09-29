#pragma once
#include "modules/module.h"
#include "core/ac_bypass.h"

class AntiKB : public Module {
public:
    AntiKB();
    void onEnable() override;
    void onUpdate(JNIEnv* env) override;

private:
    ac::KBHumanizer m_humanizer;      // unified reaction curve + overshoot + inertia
    int   m_ticksLeft   = 0;          // legacy — remove, now inside KBHumanizer
    float m_randFactor  = 1.0f;       // legacy — remove
};
