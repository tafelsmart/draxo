#pragma once
#include "modules/module.h"
class Blink : public Module { public: Blink(); void onEnable() override; void onUpdate(JNIEnv* env) override; void onDisable() override; };
