#pragma once
#include "modules/module.h"
class AutoRespawn : public Module {
public: AutoRespawn(); void onUpdate(JNIEnv* env) override; void onRender() override;
private: bool s_init=false; jmethodID s_isDead=nullptr; bool m_clickDown=false;
};
