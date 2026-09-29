#pragma once
#include "modules/module.h"
class HitMarkers : public Module { public: HitMarkers(); void onUpdate(JNIEnv* env) override; };
