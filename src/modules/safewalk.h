#pragma once
#include "modules/module.h"
class SafeWalk : public Module { public: SafeWalk(); void onUpdate(JNIEnv* env) override; };
