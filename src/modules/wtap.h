#pragma once
#include "modules/module.h"
class WTap : public Module { public: WTap(); void onUpdate(JNIEnv* env) override; void onRender() override; private: long long m_last=0; bool m_tapPending=false; };
