#pragma once
#include "modules/module.h"
class TimerM : public Module {
public: TimerM(); void onEnable() override; void onDisable() override; void onUpdate(JNIEnv* env) override;
private: float m_normalTPS=20.0f; bool s_init=false;
    jfieldID s_timerField=nullptr, s_tpsField=nullptr;
};
