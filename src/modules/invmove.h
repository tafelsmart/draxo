#pragma once
#include "modules/module.h"
class InvMove : public Module { public: InvMove(); void onUpdate(JNIEnv* env) override; };
