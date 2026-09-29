#pragma once
#include "modules/module.h"
class Dolphin : public Module { public: Dolphin(); void onUpdate(JNIEnv* env) override; };
