#pragma once
#include "modules/module.h"
class HitColor : public Module {
public: HitColor(); void onUpdate(JNIEnv* env) override;
};
