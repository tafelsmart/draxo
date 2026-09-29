#pragma once
#include "modules/module.h"
class Criticals : public Module { public: Criticals(); void onUpdate(JNIEnv* env) override; };
