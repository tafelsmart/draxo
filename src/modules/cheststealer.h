#pragma once
#include "modules/module.h"
class ChestStealer : public Module {
public: ChestStealer(); void onUpdate(JNIEnv* env) override; void onRender() override;
private: long long m_last=0; bool m_clickDown=false;
};
