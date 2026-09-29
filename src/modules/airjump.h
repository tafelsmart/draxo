#pragma once
#include "modules/module.h"
class AirJump : public Module { public: AirJump(); void onUpdate(JNIEnv* env) override; };
