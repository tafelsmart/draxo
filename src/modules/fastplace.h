#pragma once
#include "modules/module.h"
class FastPlace : public Module { public: FastPlace(); void onUpdate(JNIEnv* env) override; };
