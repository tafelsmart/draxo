#pragma once
#include "modules/module.h"
class Zoom : public Module { public: Zoom(); void onEnable() override; void onDisable() override; void onUpdate(JNIEnv* env) override; };
