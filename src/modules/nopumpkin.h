#pragma once
#include "modules/module.h"
class NoPumpkin : public Module {
public: NoPumpkin(); void onUpdate(JNIEnv* env) override;
};
