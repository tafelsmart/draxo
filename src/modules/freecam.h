#pragma once
#include "modules/module.h"
class Freecam : public Module { public: Freecam(); void onEnable() override; void onDisable() override; void onUpdate(JNIEnv* env) override;
private: double m_savedX=0,m_savedY=0,m_savedZ=0; };
