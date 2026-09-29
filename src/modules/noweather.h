#pragma once
#include "modules/module.h"
class NoWeather : public Module { public: NoWeather(); void onUpdate(JNIEnv* env) override; };
