#pragma once
#include "modules/module.h"
class NoHurtCam : public Module { public: NoHurtCam(); void onUpdate(JNIEnv* env) override; };
