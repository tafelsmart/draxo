#pragma once
#include "modules/module.h"
class AntiAFK : public Module { public: AntiAFK(); void onUpdate(JNIEnv* env) override; void onDisable() override; private: bool m_wasMoving = false; };
