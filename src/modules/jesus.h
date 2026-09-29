#pragma once
#include "pch.h"
#include "modules/module.h"

class Jesus : public Module {
public:
    Jesus();
    void onEnable() override;
    void onDisable() override;
    void onUpdate(JNIEnv* env) override;

private:
    bool m_wasOnGround = false;
};
