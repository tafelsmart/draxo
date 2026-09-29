#pragma once
#include "modules/module.h"
class FastLadder : public Module { public: FastLadder(); void onUpdate(JNIEnv* env) override; };
