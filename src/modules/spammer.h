#pragma once
#include "modules/module.h"
class Spammer : public Module { public: Spammer(); void onUpdate(JNIEnv* env) override; private: long long m_last=0; int m_spamState=0; };
