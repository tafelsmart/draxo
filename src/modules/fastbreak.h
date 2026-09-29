#pragma once
#include "modules/module.h"
class FastBreak : public Module { public: FastBreak(); void onUpdate(JNIEnv* env) override; };
