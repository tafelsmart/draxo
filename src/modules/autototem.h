#pragma once
#include "modules/module.h"
#include <chrono>
class AutoTotem : public Module {
public: AutoTotem(); void onUpdate(JNIEnv* env) override; void onRender() override;
private: bool s_init=false; jmethodID s_getOffhand=nullptr;
    std::chrono::steady_clock::time_point m_lastF{};
    bool m_fDown=false;
};
