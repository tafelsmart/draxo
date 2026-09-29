#pragma once
#include "modules/module.h"
class Step : public Module {
public: Step(); void onUpdate(JNIEnv* env) override;
};
